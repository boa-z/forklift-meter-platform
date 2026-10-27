#include "platform/rtthread/meter_nvm_port.h"
#include "contracts/meter_product.h"
#include "platform/rtthread/meter_eeprom_i2c.h"
#include "platform/rtthread/meter_execution_port.h"
#include "storage/meter_file.h"
#include <finsh.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>
#define LOG_TAG "meter.nvm"
#define LOG_LVL LOG_LVL_WARNING
#include <ulog.h>

/* 设置区固定 1 KiB：两个 512 字节槽，每槽的 seal 独占最后一页。位置与器件参数来自 Kconfig。 */
#define NVM_SLOT_SIZE 512u
#define NVM_PAYLOAD_SIZE 256u
#define NVM_WRITE_CYCLE_MS 10u
#ifndef AIC_FORKLIFT_NVM_EEPROM_BUS
#define AIC_FORKLIFT_NVM_EEPROM_BUS "i2c0"
#endif
#ifndef AIC_FORKLIFT_NVM_EEPROM_SIZE
#define AIC_FORKLIFT_NVM_EEPROM_SIZE 65536
#endif
#ifndef AIC_FORKLIFT_NVM_EEPROM_PAGE
#define AIC_FORKLIFT_NVM_EEPROM_PAGE 128
#endif
#ifndef AIC_FORKLIFT_NVM_EEPROM_OFFSET
#define AIC_FORKLIFT_NVM_EEPROM_OFFSET 0
#endif

typedef struct
{
    bool scan, initialize;
    meter_nvm_job_t job;
} work_t;
typedef struct
{
    bool scan, degraded;
    meter_slots_result_t result;
    meter_io_result_t error;
    uint32_t generation;
    uint64_t revision;
    meter_record_view_t record;
} result_t;
#define POOL_WORDS(type) ((sizeof(type) + sizeof(void *) + sizeof(rt_ubase_t) - 1u) / sizeof(rt_ubase_t))
static struct
{
    meter_core_t *core;
    meter_nvm_service_t service;
    uint8_t pending[NVM_PAYLOAD_SIZE], inflight[NVM_PAYLOAD_SIZE], encoded[NVM_PAYLOAD_SIZE];
    meter_slots_t slots;
    uint8_t media[2u * NVM_SLOT_SIZE], scratch[NVM_SLOT_SIZE];
    meter_eeprom_t eeprom;
    meter_file_t file;
    struct rt_messagequeue requests, results;
    rt_ubase_t request_pool[POOL_WORDS(work_t)], result_pool[POOL_WORDS(result_t)];
    struct rt_thread worker;
    rt_ubase_t stack[4096u / sizeof(rt_ubase_t)];
    meter_diag_storage_t diag;
    bool started, outstanding, loaded, retry, initialize;
    /* MSH 线程只置位，运行时线程在 poll 里消费。 */
    volatile bool save_requested, retry_requested, initialize_requested;
} port;

static void worker(void *arg)
{
    (void)arg;
    work_t work;
    while (rt_mq_recv(&port.requests, &work, sizeof(work), RT_WAITING_FOREVER) == RT_EOK)
    {
        result_t result = {.scan = work.scan, .generation = work.job.generation, .revision = work.job.revision};
        if (work.initialize)
        {
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
        /* 一个在途请求对应一个结果槽；结果不丢弃，I/O 期间不持任何运行时锁。 */
        while (rt_mq_send(&port.results, &result, sizeof(result)) != RT_EOK)
            rt_thread_mdelay(1);
    }
}
static bool submit_scan(void)
{
    work_t work = {.scan = true, .initialize = port.initialize};
    work.job.generation = port.service.generation;
    work.job.record.product_namespace = port.service.product_namespace;
    work.job.record.schema = port.service.schema;
    if (rt_mq_send(&port.requests, &work, sizeof(work)) != RT_EOK)
        return false;
    port.outstanding = true;
    port.initialize = false;
    return true;
}
static const meter_nvm_io_t *backend(void)
{
#ifdef AIC_FORKLIFT_NVM_FILE_BACKEND
    if (!meter_file_init(&port.file, AIC_FORKLIFT_NVM_FILE_PREFIX, NVM_SLOT_SIZE, AIC_FORKLIFT_NVM_FILE_KIND,
                         false))
        return NULL;
    return &port.file.io;
#else
    meter_eeprom_port_t io;
    if (!meter_eeprom_i2c_bind(&io, rt_i2c_bus_device_find(AIC_FORKLIFT_NVM_EEPROM_BUS)))
    {
        LOG_E("settings EEPROM bus %s not found", AIC_FORKLIFT_NVM_EEPROM_BUS);
        return NULL;
    }
    if (!meter_eeprom_init(&port.eeprom, &io, AIC_FORKLIFT_NVM_EEPROM_SIZE, AIC_FORKLIFT_NVM_EEPROM_OFFSET,
                           sizeof(port.media), AIC_FORKLIFT_NVM_EEPROM_PAGE, NVM_WRITE_CYCLE_MS))
        return NULL;
    return &port.eeprom.io;
#endif
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
                        product->storage->schema, product->storage->debounce_ms, product->storage->max_delay_ms))
        return false;
    const meter_nvm_io_t *io = backend();
    if (!io || !meter_slots_init(&port.slots, port.media, sizeof(port.media), port.scratch, sizeof(port.scratch)) ||
        !meter_slots_bind(&port.slots, io))
        return false;
    if (rt_mq_init(&port.requests, "nvm_req", port.request_pool, sizeof(work_t), sizeof(port.request_pool),
                   RT_IPC_FLAG_FIFO) != RT_EOK ||
        rt_mq_init(&port.results, "nvm_done", port.result_pool, sizeof(result_t), sizeof(port.result_pool),
                   RT_IPC_FLAG_FIFO) != RT_EOK)
        return false;
    /* 优先级低于运行时线程、高于 UI：短 I/O 唤醒不必等整帧渲染。 */
    if (rt_thread_init(&port.worker, "meter_nvm", worker, NULL, port.stack, sizeof(port.stack), 21, 10) !=
            RT_EOK ||
        rt_thread_startup(&port.worker) != RT_EOK)
        return false;
    port.started = true;
    port.diag.available = true;
    port.diag.backend = io->name;
    if (!submit_scan())
    {
        port.retry = true;
        port.diag.errors = 1u;
    }
    return true;
}
bool meter_board_nvm_changed(uint32_t now)
{
    if (!port.started || !port.loaded)
        return false;
    const size_t size = meter_settings_size(port.core);
    return meter_settings_encode(port.core, port.encoded, sizeof(port.encoded)) &&
           meter_nvm_observe(&port.service, port.encoded, size, now);
}
/* MSH 维护请求：只在运行时线程里执行，与正常保存走同一服务。 */
static void maintenance_requests(void)
{
    if (port.save_requested)
    {
        port.save_requested = false;
        meter_nvm_request_save(&port.service);
    }
    if (port.retry_requested && !port.outstanding)
    {
        port.retry_requested = false;
        port.retry = true;
    }
    if (port.initialize_requested && !port.outstanding)
    {
        port.initialize_requested = false;
        if (port.service.state == METER_NVM_CORRUPT || port.service.state == METER_NVM_INCOMPATIBLE)
        {
            port.initialize = true;
            port.retry = true;
        }
    }
}
static void handle(const result_t *result, uint32_t now)
{
    if (result->scan)
    {
        meter_diag_increment(&port.diag.reads);
        if (!port.loaded)
        {
            const meter_product_t *product = meter_product_get();
            meter_slots_result_t code = result->result;
            if (code == METER_SLOTS_OK &&
                (result->record.type != port.service.type ||
                 !(product->allow_parameter_extension
                       ? meter_settings_decode_append_only(port.core, result->record.payload,
                                                           result->record.payload_size)
                       : meter_settings_decode(port.core, result->record.payload, result->record.payload_size))))
                code = METER_SLOTS_INCOMPATIBLE;
            (void)meter_nvm_loaded(&port.service, result->generation, code, &result->record);
            port.loaded = true;
            (void)meter_board_nvm_changed(now);
        }
        else
            (void)meter_nvm_reconcile(&port.service, result->result, &result->record);
    }
    else
    {
        (void)meter_nvm_complete(&port.service, result->generation, result->revision, result->result);
        if (result->result == METER_SLOTS_OK)
            meter_diag_increment(&port.diag.writes);
    }
    if (result->result != METER_SLOTS_OK && result->result != METER_SLOTS_EMPTY)
        meter_diag_increment(&port.diag.errors);
    port.diag.last_error = (uint32_t)result->error;
    port.diag.degraded = result->degraded;
}
void meter_board_nvm_poll(uint32_t now, meter_diag_storage_t *status)
{
    if (!port.started)
        return;
    if (port.loaded)
        maintenance_requests();
    result_t result;
    if (rt_mq_recv(&port.results, &result, sizeof(result), 0) == RT_EOK)
    {
        port.outstanding = false;
        handle(&result, now);
    }
    if (!port.outstanding && port.retry)
    {
        port.retry = false;
        (void)submit_scan();
    }
    if (!port.outstanding)
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

/* 设置项与 UI 走同一个动作队列；介质维护只置请求位，结果用 `meter storage` 查看。 */
static int meter_settings(int argc, char **argv)
{
    if (!port.started)
    {
        rt_kprintf("settings unavailable\n");
        return -RT_ERROR;
    }
    meter_action_t action = {0};
    if (argc == 2 && !strcmp(argv[1], "save"))
        port.save_requested = true;
    else if (argc == 2 && !strcmp(argv[1], "retry"))
        port.retry_requested = true;
    else if (argc == 3 && !strcmp(argv[1], "initialize") && !strcmp(argv[2], "CONFIRM"))
        port.initialize_requested = true;
    else
    {
        char *end = NULL;
        if (argc == 3 && !strcmp(argv[1], "language") && (!strcmp(argv[2], "en") || !strcmp(argv[2], "zh")))
            action = (meter_action_t){METER_ACTION_LANGUAGE, 0u,
                                      !strcmp(argv[2], "zh") ? (float)METER_LANGUAGE_ZH : (float)METER_LANGUAGE_EN};
        else if (argc == 3 && !strcmp(argv[1], "units") &&
                 (!strcmp(argv[2], "metric") || !strcmp(argv[2], "imperial")))
            action = (meter_action_t){METER_ACTION_UNITS, 0u, !strcmp(argv[2], "imperial") ? 1.0f : 0.0f};
        else if (argc == 3 && !strcmp(argv[1], "brightness"))
        {
            unsigned long value = strtoul(argv[2], &end, 10);
            if (!*argv[2] || *end || value < 10u || value > 100u)
                return -RT_EINVAL;
            action = (meter_action_t){METER_ACTION_BRIGHTNESS, 0u, (float)value};
        }
        else if (argc == 4 && !strcmp(argv[1], "product"))
        {
            unsigned long id = strtoul(argv[2], &end, 10);
            if (!*argv[2] || *end || id > UINT16_MAX)
                return -RT_EINVAL;
            float value = strtof(argv[3], &end);
            if (!*argv[3] || *end)
                return -RT_EINVAL;
            /* 诊断入口同样交给 Product 的 local_action 判定，通用端口不识别产品编号。 */
            action = (meter_action_t){METER_ACTION_PRODUCT, (uint16_t)id, value};
        }
        else
        {
            rt_kprintf("meter_settings initialize CONFIRM|save|retry|language en/zh|units metric/imperial|"
                       "brightness 10..100|product id value\n");
            return -RT_EINVAL;
        }
        if (!meter_execution_action(NULL, &action))
        {
            rt_kprintf("settings REJECTED: runtime not accepting settings or queue full\n");
            return -RT_ERROR;
        }
    }
    rt_kprintf("settings QUEUED; check `meter storage`\n");
    return RT_EOK;
}
MSH_CMD_EXPORT(meter_settings, Local settings and storage maintenance);
