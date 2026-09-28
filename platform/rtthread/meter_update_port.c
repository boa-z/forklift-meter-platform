#include "platform/rtthread/meter_board_port.h"
#include "platform/rtthread/meter_update_port.h"
#include "contracts/meter_product.h"
#include "meter_build_identity.h"
#include "meter_update_build.h"
#include "platform/rtthread/meter_execution_port.h"
#include "platform/rtthread/meter_nvm_port.h"
#include "platform/rtthread/meter_update_backend.h"
#include "protocols/uds/meter_uds.h"
#include "tp/isotp_c.h"
#include <finsh.h>
#include <rthw.h>
#include <rtthread.h>
#include <stdio.h>
#include <string.h>
#define RX_ID 0x7e0u
#define TX_ID 0x7e8u
#define POOL(name, type, count)                                                                              \
    static rt_ubase_t                                                                                        \
        name[((sizeof(type) + sizeof(void *) + sizeof(rt_ubase_t) - 1u) / sizeof(rt_ubase_t)) * (count)]
typedef struct
{
    meter_update_error_t error;
    uint32_t generation, offset;
} completion_t;
static struct rt_messagequeue jobs, results;
POOL(job_pool, meter_update_job_t, 1);
POOL(result_pool, completion_t, 1);

static struct rt_thread update_thread;
static rt_ubase_t update_stack[12288 / sizeof(rt_ubase_t)];
static struct rt_mutex lock;
static struct rt_event barrier_event;
static meter_aic_update_t backend;
static meter_firmware_update_t service;
static meter_uds_t uds;
static UDSTpISOTpC_t transport;
typedef struct
{
    bool started, maintenance, admitted, healthy, cancel, abandon, barrier_requested, busy, exclusive;
    bool stopping, stopped;
    uint64_t barrier_target;
    meter_update_state_t state;
    meter_update_error_t error;
    uint32_t generation, received, total, rx, tx, drops, tx_errors, suppressed;
    char target[METER_UPDATE_VERSION_SIZE];
} update_shared_t;
static update_shared_t shared;
uint32_t UDSMillis(void)
{
    return meter_board_now_ms();
}
uint32_t isotp_user_get_us(void)
{
    return UDSMillis() * 1000u;
}
void isotp_user_debug(const char *message, ...)
{
    (void)message;
}
int isotp_user_send_can(uint32_t id, const uint8_t *data, uint8_t size)
{
    if (size > 8u || id != TX_ID)
        return ISOTP_RET_ERROR;
    meter_can_frame_t f = {0};
    f.bus = METER_BUS_CAN0;
    f.id = id;
    f.size = size;
    memcpy(f.data, data, size);
    bool queued = meter_execution_can_submit(&f, true);
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    if (queued) meter_diag_increment(&shared.tx);
    else meter_diag_increment(&shared.drops);
    rt_mutex_release(&lock);
    return queued ? ISOTP_RET_OK : ISOTP_RET_NOSPACE;
}
static void publish(void)
{
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    shared.state = service.state;
    shared.error = service.error;
    shared.generation = service.generation;
    shared.received = service.received;
    shared.total = service.manifest.size;
    memcpy(shared.target, service.manifest.version, sizeof(shared.target));
    rt_mutex_release(&lock);
}
static bool durable_barrier(void)
{
    rt_uint32_t bits;
    (void)rt_event_recv(&barrier_event, 1, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, 0, &bits);
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    shared.barrier_target = 0;
    shared.barrier_requested = true;
    rt_mutex_release(&lock);
    bool ok = rt_event_recv(&barrier_event, 1, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR,
                            rt_tick_from_millisecond(5000), &bits) == RT_EOK;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    shared.barrier_requested = false;
    ok = ok && shared.admitted && !shared.cancel;
    rt_mutex_release(&lock);
    return ok;
}
static void cancel_work(meter_update_job_t *job)
{
    /* 已验证候选可跨诊断连接保留；显式 Abort 仍销毁候选。 */
    if (service.state != METER_UPDATE_CANDIDATE && service.state != METER_UPDATE_ACTIVATED)
        (void)meter_update_abort(&service, service.generation);
    completion_t discarded;
    while (rt_mq_recv(&results, &discarded, sizeof(discarded), 0) == RT_EOK)
    {
    }
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    while (rt_mq_recv(&jobs, job, sizeof(*job), 0) == RT_EOK)
    {
    }
    /* 本地撤销准入须结束仍在等待的 UDS 请求；会话超时则丢弃旧结果。 */
    if (shared.busy && !shared.abandon)
    {
        completion_t denied = {METER_UPDATE_DENIED, service.generation, service.received};
        (void)rt_mq_send(&results, &denied, sizeof(denied));
    }
    else
        shared.busy = false;
    shared.abandon = false;
    shared.cancel = false;
    rt_mutex_release(&lock);
    publish();
}
static void update_entry(void *arg)
{
    (void)arg;
    meter_update_job_t job;
    for (;;)
    {
        bool got = rt_mq_recv(&jobs, &job, sizeof(job), rt_tick_from_millisecond(20)) == RT_EOK;
        rt_mutex_take(&lock, RT_WAITING_FOREVER);
        bool cancel = shared.cancel, admitted = shared.admitted, healthy = shared.healthy;
        bool stopping = shared.stopping;
        rt_mutex_release(&lock);
        if (stopping)
        {
            (void)meter_update_abort(&service, service.generation);
            publish();
            rt_mutex_take(&lock, RT_WAITING_FOREVER);
            shared.stopped = true;
            rt_mutex_release(&lock);
            return;
        }
        if (cancel)
        {
            cancel_work(&job);
            continue;
        }
        meter_update_tick(&service, UDSMillis());
        if (got)
        {
            meter_update_error_t e = METER_UPDATE_STATE;
            switch (job.kind)
            {
            case METER_UPDATE_JOB_BEGIN:
                e = meter_update_begin(&service, &job.manifest, admitted, UDSMillis());
                break;
            case METER_UPDATE_JOB_WRITE:
                e = meter_update_write(&service, job.generation, job.offset, job.data, job.size, UDSMillis());
                break;
            case METER_UPDATE_JOB_VERIFY:
                e = meter_update_verify(&service, job.generation, UDSMillis());
                break;
            case METER_UPDATE_JOB_ABORT:
                e = meter_update_abort(&service, job.generation);
                break;
            case METER_UPDATE_JOB_ACTIVATE:
                if (service.state == METER_UPDATE_CANDIDATE || service.state == METER_UPDATE_WAIT_DURABLE)
                {
                    (void)meter_update_activate(&service, job.generation, false);
                    publish();
                    e = meter_update_activate(&service, job.generation, admitted && durable_barrier());
                }
                break;
            case METER_UPDATE_JOB_CONFIRM:
                if (healthy && service.state == METER_UPDATE_IDLE && durable_barrier())
                {
                    e = meter_aic_update_confirm() ? METER_UPDATE_BACKEND : METER_UPDATE_OK;
                    if (e == METER_UPDATE_OK)
                        service.state = METER_UPDATE_CONFIRMED;
                }
                break;
            }
            completion_t result = {e, service.generation, service.received};
            publish();
            rt_mutex_take(&lock, RT_WAITING_FOREVER);
            if (!shared.cancel && rt_mq_send(&results, &result, sizeof(result)) != RT_EOK)
                shared.cancel = true;
            rt_mutex_release(&lock);
        }
        else
            publish();
    }
}
static bool submit(void *ctx, const meter_update_job_t *job)
{
    (void)ctx;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    bool accepted = !shared.stopping && !shared.cancel && !shared.busy && rt_mq_send(&jobs, job, sizeof(*job)) == RT_EOK;
    if (accepted)
        shared.busy = true;
    rt_mutex_release(&lock);
    return accepted;
}
static bool result(void *ctx, meter_update_error_t *error, uint32_t *gen, uint32_t *offset)
{
    (void)ctx;
    completion_t r;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    if (shared.cancel || rt_mq_recv(&results, &r, sizeof(r), 0) != RT_EOK)
    {
        rt_mutex_release(&lock);
        return false;
    }
    *error = r.error;
    *gen = r.generation;
    *offset = r.offset;
    shared.busy = false;
    rt_mutex_release(&lock);
    return true;
}
static size_t info(void *ctx, uint8_t *out, size_t capacity)
{
    (void)ctx;
    const meter_product_t *p = meter_product_get();
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    update_shared_t snapshot = shared;
    rt_mutex_release(&lock);
    int n = snprintf(
        (char *)out, capacity,
        "{\"product\":\"%s\",\"hardware\":\"%s\",\"version\":\"%s\",\"platform\":\"%s\",\"sdk\":\"%s\","
        "\"target\":\"%s\","
        "\"state\":\"%s\",\"error\":%u,\"received\":%u,\"total\":%u,\"backend_supported\":%s,\"backend_"
        "reason\":\"%s\",\"maintenance\":%u,\"exclusive\":%u,\"suppressed\":%u,\"rx\":%u,\"tx_queued\":%u,\"queue_rejected\":"
        "%u,\"tx_errors\":%u,\"os_file\":"
        "\"d13x_os.itb\",\"candidate_capacity\":%u,\"confirmation\":\"native_auto\"}",
        p->id, METER_BUILD_BOARD, METER_UPDATE_FIRMWARE_VERSION, METER_BUILD_PLATFORM, METER_BUILD_SDK,
        snapshot.target, meter_update_state_name(snapshot.state), (unsigned)snapshot.error,
        (unsigned)snapshot.received, (unsigned)snapshot.total, meter_aic_update_supported() ? "true" : "false",
        meter_aic_update_reason(), snapshot.maintenance, snapshot.exclusive, (unsigned)snapshot.suppressed,
        (unsigned)snapshot.rx, (unsigned)snapshot.tx, (unsigned)snapshot.drops, (unsigned)snapshot.tx_errors,
        (unsigned)meter_aic_update_capacity());
    return n >= 0 && (size_t)n < capacity ? (size_t)n : 0;
}
static bool can_reset(void *ctx)
{
    (void)ctx;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    bool ok = shared.state == METER_UPDATE_ACTIVATED;
    rt_mutex_release(&lock);
    return ok;
}
static void reset(void *ctx)
{
    (void)ctx;
    rt_hw_cpu_reset();
}
static void cancel(void *ctx)
{
    (void)ctx;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    shared.cancel = true;
    shared.abandon = true;
    rt_mutex_release(&lock);
}
bool meter_board_update_frame(const meter_can_frame_t *frame)
{
    if (!shared.started || frame->bus != METER_BUS_CAN0 || frame->id != RX_ID ||
        frame->extended || frame->remote || frame->size > 8u) return false;
    isotp_on_can_message(&transport.phys_link, frame->data, frame->size);
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    shared.rx++;
    rt_mutex_release(&lock);
    return true;
}
void meter_board_update_protocol(void)
{
    if (shared.started) UDSServerPoll(&uds.server);
}
void meter_board_update_stop(void)
{
    if (!shared.started) return;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    shared.stopping = true;
    shared.admitted = false;
    rt_mutex_release(&lock);
}
bool meter_board_update_stopped(void)
{
    if (!shared.started) return true;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    bool value = shared.stopped;
    rt_mutex_release(&lock);
    return value;
}
bool meter_board_update_start(void)
{
    const meter_product_t *p = meter_product_get();
    if (!p->update || !p->update_admission || shared.started)
        return false;
    meter_update_backend_t b = meter_aic_update_backend(&backend);
    (void)meter_aic_update_prepare();
    if (!meter_update_init(&service, p->update, &b))
        return false;
    if (rt_mutex_init(&lock, "ota_lk", RT_IPC_FLAG_PRIO) != RT_EOK ||
        rt_event_init(&barrier_event, "ota_nv", RT_IPC_FLAG_FIFO) != RT_EOK)
        return false;
    if (rt_mq_init(&jobs, "ota_job", job_pool, sizeof(meter_update_job_t), sizeof(job_pool),
                   RT_IPC_FLAG_FIFO) != RT_EOK ||
        rt_mq_init(&results, "ota_res", result_pool, sizeof(completion_t), sizeof(result_pool),
                   RT_IPC_FLAG_FIFO) != RT_EOK)
        return false;
    meter_uds_port_t port = {NULL, submit, result, info, can_reset, reset, cancel};
    if (UDSServerTpISOTpCInit(&transport, RX_ID, TX_ID, UDS_TP_NOOP_ADDR) != UDS_OK ||
        !meter_uds_init(&uds, &transport.hdl, &port))
        return false;
    if (rt_thread_init(&update_thread, "meter_upd", update_entry, NULL, update_stack, sizeof(update_stack),
                       25, 10) != RT_EOK || rt_thread_startup(&update_thread) != RT_EOK)
        return false;
    shared.started = true;
    return true;
}
void meter_board_update_poll(meter_core_t *core, bool ui_healthy)
{
    if (!shared.started)
        return;
    const meter_product_t *p = meter_product_get();
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    bool admitted = p->update_admission(&core->snapshot, shared.maintenance) && meter_board_nvm_ready();
    if (shared.admitted && !admitted)
        shared.cancel = true;
    shared.admitted = admitted;
    shared.healthy = ui_healthy && meter_board_nvm_ready();
    /* 暂停业务必须保留 App/NVM 门禁轮询，不能挂起整个 App 线程。 */
    shared.exclusive = (shared.maintenance || shared.cancel || shared.busy) && p->update_exclusive;
    if (shared.barrier_requested)
    {
        if (!shared.barrier_target)
            shared.barrier_target = meter_board_nvm_flush();
        if (shared.barrier_target && meter_board_nvm_barrier(shared.barrier_target))
            rt_event_send(&barrier_event, 1);
    }
    rt_mutex_release(&lock);
}
bool meter_board_update_exclusive(void)
{
    if (!shared.started)
        return false;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    bool value = shared.exclusive;
    rt_mutex_release(&lock);
    return value;
}
void meter_board_update_view(meter_update_view_t *view)
{
    if (!view)
        return;
    memset(view, 0, sizeof(*view));
    if (!shared.started)
        return;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    view->visible = shared.exclusive;
    view->state = shared.state;
    view->error = (uint32_t)shared.error;
    view->received = shared.received;
    view->total = shared.total;
    memcpy(view->target_version, shared.target, sizeof(view->target_version));
    memcpy(view->current_version, METER_UPDATE_FIRMWARE_VERSION, sizeof(METER_UPDATE_FIRMWARE_VERSION));
    rt_mutex_release(&lock);
}
bool meter_board_update_maintenance(void)
{
    if (!shared.started)
        return false;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    bool value = shared.maintenance;
    rt_mutex_release(&lock);
    return value;
}
bool meter_board_update_started(void)
{
    return shared.started;
}
static int meter_update(int argc, char **argv)
{
    if (!shared.started)
    {
        rt_kprintf("update unavailable\n");
        return -1;
    }
    if (argc == 3 && !strcmp(argv[1], "maintenance") && (!strcmp(argv[2], "on") || !strcmp(argv[2], "off")))
    {
        rt_mutex_take(&lock, RT_WAITING_FOREVER);
        shared.maintenance = !strcmp(argv[2], "on");
        rt_mutex_release(&lock);
        rt_kprintf("maintenance requested; Product owns admission\n");
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "info"))
    {
        uint8_t text[896];
        size_t n = info(NULL, text, sizeof(text));
        if (!n)
            return -1;
        /* 避免 RT-Thread 有界格式化缓冲截断完整 JSON 诊断。 */
        for (size_t offset = 0; offset < n; offset += 64u)
        {
            size_t count = n - offset;
            if (count > 64u)
                count = 64u;
            rt_kprintf("%.*s", (int)count, (const char *)&text[offset]);
        }
        rt_kprintf("\n");
        return 0;
    }
    rt_kprintf("meter_update info|maintenance on/off\n");
    return -1;
}
MSH_CMD_EXPORT(meter_update, Firmware update diagnostics and maintenance);
