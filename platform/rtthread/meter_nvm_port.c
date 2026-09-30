#include "platform/rtthread/meter_execution_port.h"
#include "contracts/meter_product.h"
#include "platform/rtthread/meter_eeprom_i2c.h"
#include "platform/rtthread/meter_nvm_port.h"
#include "storage/meter_file.h"
#include <finsh.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LOG_TAG "meter.nvm"
#define LOG_LVL LOG_LVL_WARNING
#include <ulog.h>

/* Board 分配 EEPROM 首 1 KiB；两个 512 字节槽的 seal 各独占最后一页。 */
#define NVM_SLOT_SIZE 512u
#define NVM_PAYLOAD_SIZE 256u
#define NVM_PAGE_SIZE 128u
#define NVM_WRITE_CYCLE_MS 10u

typedef struct
{
    bool scan, initialize, stop;
    meter_nvm_job_t job;
} work_t;
typedef struct
{
    bool scan, degraded, stopped;
    meter_slots_result_t result;
    meter_io_result_t error;
    uint32_t generation;
    uint64_t revision;
    meter_record_view_t record;
} result_t;
typedef struct
{
    unsigned kind;
    meter_action_t action;
} command_t;
typedef struct
{
    bool applied;
    uint64_t target;
} command_result_t;
static struct rt_messagequeue commands, command_results;
static struct rt_semaphore command_credit;
static rt_ubase_t
    command_pool[(sizeof(command_t) + sizeof(void *) + sizeof(rt_ubase_t) - 1u) / sizeof(rt_ubase_t)];
static rt_ubase_t command_result_pool[(sizeof(command_result_t) + sizeof(void *) + sizeof(rt_ubase_t) - 1u) /
                                      sizeof(rt_ubase_t)];
static void process_command(uint32_t now);
static struct
{
    meter_core_t *core;
    meter_nvm_service_t service;
    uint8_t pending[NVM_PAYLOAD_SIZE], inflight[NVM_PAYLOAD_SIZE], encoded[NVM_PAYLOAD_SIZE];
    meter_slots_t slots;
    uint8_t media[2u * NVM_SLOT_SIZE], scratch[NVM_SLOT_SIZE];
    meter_eeprom_t eeprom;
    meter_file_t file;
    struct rt_i2c_bus_device *bus;
    struct rt_messagequeue requests, results;
    rt_ubase_t request_pool[(sizeof(work_t) + sizeof(void *) + sizeof(rt_ubase_t) - 1u) / sizeof(rt_ubase_t)];
    rt_ubase_t
        result_pool[(sizeof(result_t) + sizeof(void *) + sizeof(rt_ubase_t) - 1u) / sizeof(rt_ubase_t)];
    struct rt_thread worker;
    rt_ubase_t stack[4096u / sizeof(rt_ubase_t)];
    meter_diag_storage_t diag;
    bool started, outstanding, loaded, retry, initialize, stopping, stopped;
} port;

static void worker(void *arg)
{
    (void)arg;
    work_t work;
    while (rt_mq_recv(&port.requests, &work, sizeof(work), RT_WAITING_FOREVER) == RT_EOK)
    {
        result_t result = {0};
        if (work.stop)
        {
            result.stopped = true;
            (void)rt_mq_send(&port.results, &result, sizeof(result));
            return;
        }
        result.scan = work.scan;
        result.generation = work.job.generation;
        result.revision = work.job.revision;
        if (work.initialize)
        {
            result.scan = true;
            result.result = meter_slots_initialize_empty(&port.slots);
            if (result.result == METER_SLOTS_EMPTY)
                result.result =
                    meter_slots_scan(&port.slots, work.job.record.product_namespace, work.job.record.schema);
        }
        else if (work.scan)
        {
            result.result =
                meter_slots_scan(&port.slots, work.job.record.product_namespace, work.job.record.schema);
            if (result.result == METER_SLOTS_OK)
                (void)meter_slots_active(&port.slots, &result.record);
        }
        else
            result.result = meter_slots_commit(&port.slots, &work.job.record, SIZE_MAX);
        result.degraded = port.slots.degraded;
        result.error = port.slots.last_error;
        /* 一个在途请求预留一个结果槽；结果不被丢弃，不持 App/诊断锁做 I/O。 */
        while (rt_mq_send(&port.results, &result, sizeof(result)) != RT_EOK)
            rt_thread_mdelay(1);
    }
}
static bool submit_scan(void)
{
    work_t work = {0};
    work.scan = true;
    work.initialize = port.initialize;
    work.job.generation = port.service.generation;
    work.job.record.product_namespace = port.service.product_namespace;
    work.job.record.schema = port.service.schema;
    if (rt_mq_send(&port.requests, &work, sizeof(work)) != RT_EOK)
        return false;
    port.outstanding = true;
    port.initialize = false;
    return true;
}
bool meter_board_nvm_start(meter_core_t *core)
{
    const meter_product_t *product = meter_product_get();
    if (port.started || !core || !product->storage || !product->storage->enabled ||
        meter_settings_size(core) > NVM_PAYLOAD_SIZE)
        return false;
    port.core = core;
    if (!meter_nvm_init(&port.service, port.pending, port.inflight, sizeof(port.pending),
                        product->storage->record_type, product->storage->product_namespace,
                        product->storage->schema, product->storage->debounce_ms,
                        product->storage->max_delay_ms))
        return false;
    const meter_nvm_io_t *backend;
#ifdef AIC_FORKLIFT_NVM_FILE_BACKEND
    if (!meter_file_init(&port.file, AIC_FORKLIFT_NVM_FILE_PREFIX, NVM_SLOT_SIZE, AIC_FORKLIFT_NVM_FILE_KIND,
                         false))
        return false;
    backend = &port.file.io;
#else
    port.bus = rt_i2c_bus_device_find("i2c0");
    meter_eeprom_port_t io;
    if (!meter_eeprom_i2c_bind(&io, port.bus))
        return false;
    if (!meter_eeprom_init(&port.eeprom, &io, 65536u, 0u, sizeof(port.media), NVM_PAGE_SIZE,
                           NVM_WRITE_CYCLE_MS))
        return false;
    backend = &port.eeprom.io;
#endif
    if (!meter_slots_init(&port.slots, port.media, sizeof(port.media), port.scratch, sizeof(port.scratch)) ||
        !meter_slots_bind(&port.slots, backend))
        return false;
    if (rt_mq_init(&port.requests, "nvm_req", port.request_pool, sizeof(work_t), sizeof(port.request_pool),
                   RT_IPC_FLAG_FIFO) != RT_EOK)
        return false;
    if (rt_mq_init(&port.results, "nvm_done", port.result_pool, sizeof(result_t), sizeof(port.result_pool),
                   RT_IPC_FLAG_FIFO) != RT_EOK)
    {
        rt_mq_detach(&port.requests);
        return false;
    }
    if (rt_mq_init(&commands, "nvm_cmd", command_pool, sizeof(command_t), sizeof(command_pool),
                   RT_IPC_FLAG_FIFO) != RT_EOK)
        goto fail_results;
    if (rt_mq_init(&command_results, "nvm_ack", command_result_pool, sizeof(command_result_t),
                   sizeof(command_result_pool), RT_IPC_FLAG_FIFO) != RT_EOK)
        goto fail_commands;
    if (rt_sem_init(&command_credit, "nvm_credit", 1, RT_IPC_FLAG_FIFO) != RT_EOK)
        goto fail_ack;
    /* 在 App/Protocol 之后、UI 绘制之前调度，避免每次短 I/O 唤醒等待整帧渲染。 */
    if (rt_thread_init(&port.worker, "meter_nvm", worker, NULL, port.stack, sizeof(port.stack), 21, 10) !=
        RT_EOK)
        goto fail_credit;
    if (rt_thread_startup(&port.worker) != RT_EOK)
    {
        rt_thread_detach(&port.worker);
        goto fail_credit;
    }
    port.started = true;
    port.diag.available = true;
    port.diag.backend = backend->name;
    /* 初始队列为空且只有此 owner；提交失败保留已启动 worker 供诊断。 */
    if (!submit_scan())
    {
        port.retry = true;
        port.diag.errors = 1u;
    }
    return true;
fail_credit:
    rt_sem_detach(&command_credit);
fail_ack:
    rt_mq_detach(&command_results);
fail_commands:
    rt_mq_detach(&commands);
fail_results:
    rt_mq_detach(&port.results);
    rt_mq_detach(&port.requests);
    return false;
}
bool meter_board_nvm_changed(uint32_t now)
{
    if (!port.started || !port.loaded)
        return false;
    const size_t size = meter_settings_size(port.core);
    return meter_settings_encode(port.core, port.encoded, sizeof(port.encoded)) &&
           meter_nvm_observe(&port.service, port.encoded, size, now);
}
void meter_board_nvm_poll(uint32_t now, meter_diag_storage_t *status)
{
    if (!port.started)
        return;
    if (port.loaded && !port.stopping)
        process_command(now);
    result_t result;
    if (rt_mq_recv(&port.results, &result, sizeof(result), 0) == RT_EOK)
    {
        port.outstanding = false;
        if (result.stopped) { port.stopped = true; return; }
        if (result.scan)
        {
            meter_diag_increment(&port.diag.reads);
            if (!port.loaded)
            {
                const meter_product_t *product = meter_product_get();
                if (result.result == METER_SLOTS_OK &&
                    (result.record.type != port.service.type ||
                     !(product->allow_parameter_extension
                         ? meter_settings_decode_append_only(port.core, result.record.payload, result.record.payload_size)
                         : meter_settings_decode(port.core, result.record.payload, result.record.payload_size))))
                    result.result = METER_SLOTS_INCOMPATIBLE;
                (void)meter_nvm_loaded(&port.service, result.generation, result.result, &result.record);
                port.loaded = true;
                (void)meter_board_nvm_changed(now);
            }
            else
                (void)meter_nvm_reconcile(&port.service, result.result, &result.record);
        }
        else
        {
            (void)meter_nvm_complete(&port.service, result.generation, result.revision, result.result);
            if (result.result == METER_SLOTS_OK)
                meter_diag_increment(&port.diag.writes);
        }
        if (result.result != METER_SLOTS_OK && result.result != METER_SLOTS_EMPTY)
            meter_diag_increment(&port.diag.errors);
        port.diag.last_error = (uint32_t)result.error;
        port.diag.degraded = result.degraded;
    }
    if (!port.outstanding && port.retry)
    {
        port.retry = false;
        (void)submit_scan();
    }
    if (!port.outstanding && !port.stopping)
    {
        work_t work = {0};
        if (meter_nvm_take(&port.service, now, &work.job))
        {
            if (rt_mq_send(&port.requests, &work, sizeof(work)) == RT_EOK)
                port.outstanding = true;
            else
                (void)meter_nvm_complete(&port.service, work.job.generation, work.job.revision,
                                         METER_SLOTS_INVALID);
        }
    }
    port.diag.state = (uint32_t)port.service.state;
    port.diag.dirty = port.service.dirty;
    port.diag.ram_revision = port.service.ram_revision;
    port.diag.inflight_revision = port.service.inflight_revision;
    port.diag.durable_revision = port.service.durable_revision;
    port.diag.depth = port.outstanding ? 1u : 0u;
    port.diag.last_result = (uint32_t)port.service.last_result;
    port.diag.language = (uint32_t)port.core->snapshot.language;
    port.diag.brightness = port.core->snapshot.brightness;
    port.diag.configured_can_rate = (uint32_t)port.core->snapshot.can_rate;
    port.diag.imperial = port.core->snapshot.imperial;
    if (status)
        *status = port.diag;
}
bool meter_board_nvm_ready(void)
{
    return !port.started || port.loaded;
}
uint64_t meter_board_nvm_flush(void)
{
    if (!port.started || !port.loaded)
        return 0u;
    meter_nvm_request_save(&port.service);
    return port.service.ram_revision;
}
bool meter_board_nvm_barrier(uint64_t target)
{
    return port.started && meter_nvm_barrier(&port.service, target);
}
bool meter_board_nvm_retry(void)
{
    if (!port.started || !port.loaded || port.outstanding || port.retry)
        return false;
    port.retry = true;
    return true;
}

/* MSH 只投递用户意图；结果槽保留到显式领取，查询不会产生 EEPROM 写入。 */
static void process_command(uint32_t now)
{
    command_t command;
    if (rt_mq_recv(&commands, &command, sizeof(command), 0) != RT_EOK)
        return;
    command_result_t result = {false, 0u};
    if (!meter_execution_settings_allowed())
    {
        (void)rt_mq_send(&command_results, &result, sizeof(result));
        return;
    }
    if (command.kind == 1u)
    {
        result.target = meter_board_nvm_flush();
        result.applied = result.target != 0u;
    }
    else if (command.kind == 2u)
        result.applied = meter_board_nvm_retry();
    else if (command.kind == 4u)
    {
        if (!port.outstanding && !port.retry &&
            (port.service.state == METER_NVM_CORRUPT || port.service.state == METER_NVM_INCOMPATIBLE))
        {
            port.initialize = true;
            port.retry = true;
            result.applied = true;
        }
    }
    else if (command.kind == 3u)
    {
        const meter_product_t *product = meter_product_get();
        result.applied =
            product->auth->local_settings &&
            (command.action.kind != METER_ACTION_PARAMETER || product->capabilities->parameter_write) &&
            (command.action.kind == METER_ACTION_PRODUCT
                ? product->local_action && product->local_action(&port.core->snapshot, &command.action, now)
                : meter_core_action(port.core, &command.action));
        if (result.applied)
            (void)meter_board_nvm_changed(now);
        result.target = port.service.ram_revision;
    }
    /* 信用在 MSH 领取结果后归还，因此此处一定已有容量。 */
    (void)rt_mq_send(&command_results, &result, sizeof(result));
}
static int meter_settings(int argc, char **argv)
{
    if (!port.started)
    {
        rt_kprintf("settings unavailable\n");
        return -RT_ERROR;
    }
    if (argc == 2 && !strcmp(argv[1], "result"))
    {
        command_result_t result;
        if (rt_mq_recv(&command_results, &result, sizeof(result), 0) != RT_EOK)
        {
            rt_kprintf("settings result=PENDING_OR_NONE\n");
            return -RT_ERROR;
        }
        rt_sem_release(&command_credit);
        /* RT-Thread 精简格式器不支持 long long，交由 libc 有界格式化后输出。 */
        char target[24];
        (void)snprintf(target, sizeof(target), "%llu", (unsigned long long)result.target);
        rt_kprintf("settings result=%s target=%s durability=query-meter-storage\n",
                   result.applied ? "APPLIED_OR_QUEUED" : "REJECTED", target);
        return RT_EOK;
    }
    if (!meter_execution_settings_allowed())
    { rt_kprintf("settings REJECTED by runtime mode or lifecycle\n"); return -RT_ERROR; }
    command_t command = {0};
    if (argc == 2 && !strcmp(argv[1], "save"))
        command.kind = 1u;
    else if (argc == 2 && !strcmp(argv[1], "retry"))
        command.kind = 2u;
    else if (argc == 3 && !strcmp(argv[1], "initialize") && !strcmp(argv[2], "CONFIRM"))
        command.kind = 4u;
    else if (argc == 3 && !strcmp(argv[1], "language") && (!strcmp(argv[2], "en") || !strcmp(argv[2], "zh")))
    {
        command.kind = 3u;
        command.action = (meter_action_t){METER_ACTION_LANGUAGE, 0u,
                                          !strcmp(argv[2], "zh") ? METER_LANGUAGE_ZH : METER_LANGUAGE_EN};
    }
    else if (argc == 3 && !strcmp(argv[1], "units") &&
             (!strcmp(argv[2], "metric") || !strcmp(argv[2], "imperial")))
    {
        command.kind = 3u;
        command.action = (meter_action_t){METER_ACTION_UNITS, 0u, !strcmp(argv[2], "imperial") ? 1.0f : 0.0f};
    }
    else if (argc == 4 && !strcmp(argv[1], "product"))
    {
        char *end;
        unsigned long id = strtoul(argv[2], &end, 10);
        if (!*argv[2] || *end || id > UINT16_MAX)
            return -RT_EINVAL;
        float value = strtof(argv[3], &end);
        if (!*argv[3] || *end)
            return -RT_EINVAL;
        /* 诊断入口也经过 Product 授权；通用端口不识别 Demo 密码或参数编号。 */
        command.kind = 3u;
        command.action = (meter_action_t){METER_ACTION_PRODUCT, (uint16_t)id, value};
    }
    else if (argc == 3 && !strcmp(argv[1], "brightness"))
    {
        char *end;
        unsigned long value = strtoul(argv[2], &end, 10);
        if (!*argv[2] || *end || value < 10u || value > 100u)
            return -RT_EINVAL;
        command.kind = 3u;
        command.action = (meter_action_t){METER_ACTION_BRIGHTNESS, 0u, (float)value};
    }
    else
    {
        rt_kprintf("meter_settings initialize CONFIRM|save|retry|result|language en/zh|units "
                   "metric/imperial|brightness 10..100|product id value\n");
        return -RT_EINVAL;
    }
    if (rt_sem_take(&command_credit, 0) != RT_EOK)
    {
        rt_kprintf("settings BUSY; collect result first\n");
        return -RT_ERROR;
    }
    if (rt_mq_send(&commands, &command, sizeof(command)) != RT_EOK)
    {
        rt_sem_release(&command_credit);
        return -RT_ERROR;
    }
    rt_kprintf("settings result=QUEUED; use meter_settings result\n");
    return RT_EOK;
}
MSH_CMD_EXPORT(meter_settings, Local settings asynchronous commands);

void meter_board_nvm_stop(void)
{
    if (!port.started || port.stopping || port.outstanding) return;
    work_t work = {.stop = true};
    if (rt_mq_send(&port.requests, &work, sizeof(work)) == RT_EOK)
    { port.stopping = true; port.outstanding = true; }
}
bool meter_board_nvm_stopped(void)
{
    return !port.started || port.stopped;
}
