#include "platform/rtthread/meter_update_port.h"
#include "contracts/meter_product.h"
#include "meter_build_identity.h"
#include "meter_display_version.h"
#include "meter_update_build.h"
#include "platform/rtthread/meter_board_port.h"
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
    bool started, maintenance, session_maintenance, admitted, healthy, cancel, abandon, barrier_requested,
        busy, exclusive;
    /* worker 权威 Manifest 的只读副本：UDS 与 UART 都只读快照，不碰 service。 */
    bool manifest_valid, candidate_verified;
    uint64_t barrier_target;
    meter_update_state_t state;
    meter_update_error_t error;
    meter_update_manifest_t manifest;
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
static meter_bus_role_t uds_bus;
static uint32_t rx_id, tx_id;
int isotp_user_send_can(uint32_t id, const uint8_t *data, uint8_t size)
{
    if (size > 8u || id != tx_id)
        return ISOTP_RET_ERROR;
    meter_can_frame_t f = {0};
    f.bus = uds_bus;
    f.id = id;
    f.size = size;
    memcpy(f.data, data, size);
    bool queued = meter_execution_can_submit(&f, true);
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    if (queued)
        meter_diag_increment(&shared.tx);
    else
        meter_diag_increment(&shared.drops);
    rt_mutex_release(&lock);
    return queued ? ISOTP_RET_OK : ISOTP_RET_NOSPACE;
}
/* 已验证候选：只差 NVM barrier 或已激活，跨诊断连接保留。 */
static bool candidate_state(meter_update_state_t s)
{
    return s == METER_UPDATE_CANDIDATE || s == METER_UPDATE_WAIT_DURABLE || s == METER_UPDATE_ACTIVATING ||
           s == METER_UPDATE_ACTIVATED;
}
/* worker Manifest 只在下载到激活区间有效；FAILED/ABORTED/CONFIRMED 不得回读旧值。 */
static bool manifest_state(meter_update_state_t s)
{
    return s == METER_UPDATE_DOWNLOADING || s == METER_UPDATE_TRANSFERRED || s == METER_UPDATE_VERIFYING ||
           candidate_state(s);
}
/* 进度语义：空闲/撤销归零，下载中为实际值，其余为完成态满值，失败保留诊断值。 */
static void progress_of(meter_update_state_t s, uint32_t received, uint32_t total, uint32_t *out_received,
                        uint32_t *out_total)
{
    switch (s)
    {
    case METER_UPDATE_IDLE:
    case METER_UPDATE_ABORTED:
    case METER_UPDATE_CONFIRMED:
        *out_received = 0u;
        *out_total = 0u;
        break;
    case METER_UPDATE_DOWNLOADING:
    case METER_UPDATE_FAILED:
        *out_received = received;
        *out_total = total;
        break;
    default:
        *out_received = total;
        *out_total = total;
        break;
    }
}
static bool copy_text(char *dst, size_t capacity, const char *src)
{
    size_t n = src ? strlen(src) : 0u;
    if (n >= capacity)
        return false;
    memset(dst, 0, capacity);
    if (n)
        memcpy(dst, src, n);
    return true;
}
static void publish(void)
{
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    shared.state = service.state;
    shared.error = service.error;
    shared.generation = service.generation;
    shared.manifest = service.manifest;
    shared.manifest_valid = manifest_state(service.state);
    shared.candidate_verified = candidate_state(service.state);
    progress_of(service.state, service.received, service.manifest.size, &shared.received, &shared.total);
    memset(shared.target, 0, sizeof(shared.target));
    if (shared.manifest_valid)
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
    /* 已验证候选可跨诊断连接保留；显式 Abort 仍销毁候选。
     * WAIT_DURABLE 同样是已校验候选（只等 NVM barrier），会话超时/离开编程会话不得销毁它。 */
    if (service.state != METER_UPDATE_CANDIDATE && service.state != METER_UPDATE_WAIT_DURABLE &&
        service.state != METER_UPDATE_ACTIVATED)
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
        rt_mutex_release(&lock);
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
    bool accepted =
        !shared.cancel && !shared.busy && rt_mq_send(&jobs, job, sizeof(*job)) == RT_EOK;
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
static bool snapshot(void *ctx, meter_uds_snapshot_t *out)
{
    (void)ctx;
    const meter_product_t *p = meter_product_get();
    if (!out || !p || !p->update)
        return false;
    memset(out, 0, sizeof(*out));
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    out->state = (uint8_t)shared.state;
    out->error = (uint8_t)shared.error;
    out->received = shared.received;
    out->total = shared.total;
    out->manifest = shared.manifest;
    out->manifest_valid = shared.manifest_valid;
    out->candidate_verified = shared.candidate_verified;
    out->maintenance = shared.maintenance || shared.session_maintenance;
    out->exclusive = shared.exclusive;
    out->admitted = shared.admitted;
    rt_mutex_release(&lock);
    /* 身份与后端能力在启动后不变，直接取权威来源，不进锁也不复制进 shared。 */
    out->backend_supported = meter_aic_update_supported();
    out->capacity = meter_aic_update_capacity();
    return copy_text(out->product, sizeof(out->product), p->update->product) &&
           copy_text(out->hardware, sizeof(out->hardware), p->update->hardware) &&
           copy_text(out->version, sizeof(out->version), METER_UPDATE_FIRMWARE_VERSION) &&
           copy_text(out->os_file, sizeof(out->os_file), METER_UPDATE_OS_FILE) &&
           copy_text(out->reason, sizeof(out->reason), meter_aic_update_reason());
}
/* UART 与 CAN 共用同一份快照，键名保持既有诊断输出不变。 */
static size_t info(void *ctx, uint8_t *out, size_t capacity)
{
    meter_uds_snapshot_t s;
    if (!snapshot(ctx, &s))
        return 0;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    update_shared_t counters = shared;
    rt_mutex_release(&lock);
    int n = snprintf(
        (char *)out, capacity,
        "{\"product\":\"%s\",\"hardware\":\"%s\",\"version\":\"%s\",\"platform\":\"%s\",\"sdk\":\"%s\","
        "\"target\":\"%s\",\"state\":\"%s\",\"error\":%u,\"received\":%u,\"total\":%u,\"manifest_present\":%"
        "u,"
        "\"candidate_verified\":%u,\"backend_supported\":%s,\"backend_reason\":\"%s\",\"maintenance\":%u,"
        "\"exclusive\":%u,\"admitted\":%u,\"suppressed\":%u,\"rx\":%u,\"tx_queued\":%u,\"queue_rejected\":"
        "%u,\"tx_errors\":%u,\"os_file\":\"%s\",\"candidate_capacity\":%u,\"confirmation\":\"native_auto\"}",
        s.product, s.hardware, s.version, METER_BUILD_PLATFORM, METER_BUILD_SDK,
        s.manifest_valid ? s.manifest.version : "", meter_update_state_name((meter_update_state_t)s.state),
        (unsigned)s.error, (unsigned)s.received, (unsigned)s.total, s.manifest_valid ? 1u : 0u,
        s.candidate_verified ? 1u : 0u, s.backend_supported ? "true" : "false", s.reason,
        s.maintenance ? 1u : 0u, s.exclusive ? 1u : 0u, s.admitted ? 1u : 0u, (unsigned)counters.suppressed,
        (unsigned)counters.rx, (unsigned)counters.tx, (unsigned)counters.drops, (unsigned)counters.tx_errors,
        s.os_file, (unsigned)s.capacity);
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
static void session_maintenance_request(void *ctx, bool on)
{
    (void)ctx;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    shared.session_maintenance = on;
    rt_mutex_release(&lock);
}
bool meter_board_update_frame(const meter_can_frame_t *frame)
{
    if (!shared.started || frame->bus != uds_bus || frame->id != rx_id || frame->extended ||
        frame->remote || frame->size > 8u)
        return false;
    isotp_on_can_message(&transport.phys_link, frame->data, frame->size);
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    shared.rx++;
    rt_mutex_release(&lock);
    return true;
}
void meter_board_update_protocol(void)
{
    if (shared.started)
        UDSServerPoll(&uds.server);
}
bool meter_board_update_start(void)
{
    const meter_product_t *p = meter_product_get();
    if (!p->update || !p->update_admission || shared.started)
        return false;
    uds_bus = p->update->bus;
    rx_id = p->update->request_id ? p->update->request_id : 0x7e0u;
    tx_id = p->update->response_id ? p->update->response_id : 0x7e8u;
    if ((unsigned)uds_bus >= METER_BUS_COUNT || rx_id > 0x7ffu || tx_id > 0x7ffu || rx_id == tx_id)
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
    meter_uds_port_t port = {NULL,      submit, result, snapshot,
                             can_reset, reset,  cancel, session_maintenance_request};
    if (UDSServerTpISOTpCInit(&transport, rx_id, tx_id, UDS_TP_NOOP_ADDR) != UDS_OK ||
        !meter_uds_init(&uds, &transport.hdl, &port))
        return false;
    if (rt_thread_init(&update_thread, "meter_upd", update_entry, NULL, update_stack, sizeof(update_stack),
                       25, 10) != RT_EOK ||
        rt_thread_startup(&update_thread) != RT_EOK)
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
    /* 维护请求有两个来源：串口命令，或主机经 CAN 打开的 UDS 编程会话；准入仍由 Product 决定。 */
    bool maintenance = shared.maintenance || shared.session_maintenance;
    bool admitted = p->update_admission(&core->snapshot, maintenance) && meter_board_nvm_ready();
    if (shared.admitted && !admitted)
        shared.cancel = true;
    shared.admitted = admitted;
    shared.healthy = ui_healthy && meter_board_nvm_ready();
    /* 暂停业务必须保留 App/NVM 门禁轮询，不能挂起整个 App 线程。 */
    shared.exclusive = (maintenance || shared.cancel || shared.busy) && p->update_exclusive;
    if (shared.barrier_requested)
    {
        /* 与生命周期退出一致：未配置持久化的 Product 没有待提交设置。 */
        bool durable = !p->storage || !p->storage->enabled;
        if (!durable)
        {
            if (!shared.barrier_target)
                shared.barrier_target = meter_board_nvm_flush();
            durable = shared.barrier_target && meter_board_nvm_barrier(shared.barrier_target);
        }
        if (durable)
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
    _Static_assert(sizeof(METER_FIRMWARE_DISPLAY_VERSION) <= METER_UPDATE_VERSION_SIZE,
                   "display version must fit the update view");
    memcpy(view->current_version, METER_FIRMWARE_DISPLAY_VERSION, sizeof(METER_FIRMWARE_DISPLAY_VERSION));
    rt_mutex_release(&lock);
}
bool meter_board_update_maintenance(void)
{
    if (!shared.started)
        return false;
    rt_mutex_take(&lock, RT_WAITING_FOREVER);
    bool value = shared.maintenance || shared.session_maintenance;
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
        uint8_t text[1024];
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
