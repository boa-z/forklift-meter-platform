/* 直接执行板端适配的取消路径；IPC 替身检查额度和互斥锁契约。 */
#include "platform/rtthread/hmi_update_port.c"
#include <assert.h>
#include <stdarg.h>
static char console_output[1024];
static size_t console_used;
static bool diagnostic_test, nvm_ready = true;
static bool barrier_test, durable_ready;
static unsigned barrier_events, flush_requests;
static uint64_t durable_revision;
static bool admission(const hmi_snapshot_t *snapshot, bool maintenance)
{
    (void)snapshot;
    return maintenance;
}
static const hmi_update_policy_t test_policy = {"synthetic", "reference-board", 4096, 1000};
static hmi_product_t test_product = {
    .id = "synthetic", .update = &test_policy, .update_admission = admission, .update_exclusive = true};
static unsigned released;
int rt_mutex_take(struct rt_mutex *mutex, int timeout)
{
    (void)timeout;
    assert(!mutex->held);
    mutex->held = true;
    return 0;
}
int rt_mutex_release(struct rt_mutex *mutex)
{
    assert(mutex->held);
    mutex->held = false;
    return 0;
}
int rt_mq_send(struct rt_messagequeue *q, const void *data, size_t size)
{
    assert(size <= sizeof(q->data));
    if (q->full)
        return -1;
    memcpy(q->data, data, size);
    q->size = size;
    q->full = true;
    return 0;
}
int rt_mq_recv(struct rt_messagequeue *q, void *data, size_t size, int timeout)
{
    (void)timeout;
    if (!q->full)
        return -1;
    assert(size == q->size);
    memcpy(data, q->data, size);
    q->full = false;
    return 0;
}
static void release_test(void *context)
{
    (void)context;
    released++;
}
static void setup(void)
{
    memset(&shared, 0, sizeof(shared));
    memset(&jobs, 0, sizeof(jobs));
    memset(&results, 0, sizeof(results));
    memset(&service, 0, sizeof(service));
    released = 0;
    rx_id = 0x7e0u;
    tx_id = 0x7e8u;
    service.state = HMI_UPDATE_DOWNLOADING;
    service.generation = 9;
    service.opened = true;
    service.backend.abort = release_test;
}
static void test_optional_storage_barrier(void)
{
    /* 没有持久化职责时可完成屏障；已启用的存储仍须等待具体 durable revision。 */
    setup();
    barrier_test = true;
    shared.started = shared.maintenance = shared.barrier_requested = true;
    hmi_core_t core = {0};
    test_product.storage = NULL;
    hmi_board_update_poll(&core, true);
    assert(barrier_events == 1 && flush_requests == 0);
    hmi_storage_profile_t storage = {.enabled = false};
    test_product.storage = &storage;
    hmi_board_update_poll(&core, true);
    assert(barrier_events == 2 && flush_requests == 0);
    storage.enabled = true;
    nvm_ready = false;
    hmi_board_update_poll(&core, true);
    assert(barrier_events == 2 && flush_requests == 1 && !shared.admitted);
    nvm_ready = true;
    durable_revision = 9;
    hmi_board_update_poll(&core, true);
    assert(barrier_events == 2 && flush_requests == 2 && shared.barrier_target == 9);
    durable_ready = true;
    hmi_board_update_poll(&core, true);
    assert(barrier_events == 3 && flush_requests == 2);
    test_product.storage = NULL;
    barrier_test = false;
}
/* 进度语义、候选/Manifest 有效性与身份字符串必须来自同一份快照，CAN 与 UART 同源。 */
static void test_snapshot_contract(void)
{
    hmi_uds_snapshot_t s;
    uint8_t json[1024];
    diagnostic_test = true;
    setup();
    service.state = HMI_UPDATE_IDLE;
    publish();
    assert(snapshot(NULL, &s) && s.received == 0 && s.total == 0 && !s.manifest_valid &&
           !s.candidate_verified);
    service.state = HMI_UPDATE_DOWNLOADING;
    service.received = 100;
    service.manifest.size = 260;
    publish();
    assert(snapshot(NULL, &s) && s.received == 100 && s.total == 260 && s.manifest_valid &&
           !s.candidate_verified);
    service.state = HMI_UPDATE_CANDIDATE;
    publish();
    assert(snapshot(NULL, &s) && s.received == 260 && s.total == 260 && s.manifest_valid &&
           s.candidate_verified);
    service.state = HMI_UPDATE_WAIT_DURABLE;
    publish();
    assert(snapshot(NULL, &s) && s.manifest_valid && s.candidate_verified);
    service.state = HMI_UPDATE_ACTIVATED;
    publish();
    assert(snapshot(NULL, &s) && s.manifest_valid && s.candidate_verified);
    service.state = HMI_UPDATE_CONFIRMED;
    publish();
    assert(snapshot(NULL, &s) && s.received == 0 && s.total == 0 && !s.manifest_valid &&
           !s.candidate_verified);
    service.state = HMI_UPDATE_FAILED;
    service.received = 128;
    publish();
    assert(snapshot(NULL, &s) && s.received == 128 && s.total == 260 && !s.manifest_valid &&
           !s.candidate_verified);
    service.state = HMI_UPDATE_ABORTED;
    publish();
    assert(snapshot(NULL, &s) && s.received == 0 && s.total == 0 && !s.manifest_valid);
    assert(strcmp(s.hardware, "reference-board") == 0 && strcmp(s.version, "test") == 0 &&
           strcmp(s.reason, "test") == 0 && strcmp(s.os_file, "d13x_os.itb") == 0);
    size_t n = info(NULL, json, sizeof(json));
    assert(n > 0 && strstr((char *)json, "\"hardware\":\"reference-board\"") &&
           strstr((char *)json, "\"os_file\":\"d13x_os.itb\"") &&
           strstr((char *)json, "\"backend_reason\":\"test\""));
}
int main(void)
{
    hmi_update_job_t job = {.kind = HMI_UPDATE_JOB_WRITE}, discarded;
    hmi_update_error_t error;
    uint32_t generation, offset;
    setup();
    assert(submit(NULL, &job) && !submit(NULL, &job));
    shared.cancel = true;
    assert(!submit(NULL, &job));
    cancel_work(&discarded);
    assert(released == 1 && !jobs.full && shared.busy && !shared.cancel);
    assert(result(NULL, &error, &generation, &offset));
    assert(error == HMI_UPDATE_DENIED && generation == 9 && !shared.busy);
    assert(submit(NULL, &job));

    setup();
    assert(submit(NULL, &job));
    completion_t old = {HMI_UPDATE_OK, 9, 512};
    assert(rt_mq_send(&results, &old, sizeof(old)) == 0);
    cancel(NULL);
    assert(!result(NULL, &error, &generation, &offset));
    cancel_work(&discarded);
    assert(!results.full && !jobs.full && !shared.busy && !shared.cancel);
    assert(!result(NULL, &error, &generation, &offset) && submit(NULL, &job));

    setup();
    service.state = HMI_UPDATE_CANDIDATE;
    cancel(NULL);
    cancel_work(&discarded);
    assert(service.state == HMI_UPDATE_CANDIDATE && released == 0);
    /* WAIT_DURABLE 只差 NVM barrier，仍属已验证候选：会话超时/取消必须保留。 */
    setup();
    service.state = HMI_UPDATE_WAIT_DURABLE;
    cancel(NULL);
    cancel_work(&discarded);
    assert(service.state == HMI_UPDATE_WAIT_DURABLE && released == 0);
    setup();
    service.state = HMI_UPDATE_ACTIVATED;
    cancel(NULL);
    cancel_work(&discarded);
    assert(service.state == HMI_UPDATE_ACTIVATED && released == 0);
    setup();
    diagnostic_test = true;
    shared.started = true;
    char *args[] = {"hmi_update", "info"};
    uint8_t expected[1024];
    size_t expected_size = info(NULL, expected, sizeof(expected));
    assert(expected_size > 128);
    assert(hmi_update(2, args) == 0);
    assert(console_used == expected_size + 1);
    assert(memcmp(console_output, expected, expected_size) == 0);
    assert(console_output[expected_size] == '\n');
    /* 维护入口清空旧业务帧，流控丢弃单独计数；离开后恢复接收。 */
    hmi_core_t core = {0};
    hmi_can_frame_t frame = {0};
    assert(!hmi_board_update_frame(&frame));
    shared.maintenance = true;
    hmi_board_update_poll(&core, true);
    assert(hmi_board_update_exclusive() && shared.admitted);

    shared.total = 1065984;
    shared.received = 532992;
    hmi_update_view_t view;
    hmi_board_update_view(&view);
    assert(view.visible && view.received == 532992 && view.total == 1065984);
    nvm_ready = false;
    hmi_board_update_poll(&core, true);
    assert(shared.cancel && !shared.admitted && hmi_board_update_exclusive());
    shared.maintenance = false;
    hmi_board_update_poll(&core, true);
    assert(hmi_board_update_exclusive());
    cancel_work(&discarded);
    hmi_board_update_poll(&core, true);
    assert(!hmi_board_update_exclusive());

    /* 经 CAN 的编程会话同样提出维护请求，离开会话即撤销，串口标志互不影响。 */
    nvm_ready = true;
    session_maintenance_request(NULL, true);
    hmi_board_update_poll(&core, true);
    assert(hmi_board_update_maintenance() && shared.admitted && hmi_board_update_exclusive());
    session_maintenance_request(NULL, false);
    hmi_board_update_poll(&core, true);
    /* 失去准入会取消进行中的升级，业务在取消完成后才恢复。 */
    assert(!hmi_board_update_maintenance() && !shared.admitted && shared.cancel);
    cancel_work(&discarded);
    hmi_board_update_poll(&core, true);
    assert(!hmi_board_update_exclusive());
    session_maintenance_request(NULL, true);
    shared.maintenance = true;
    session_maintenance_request(NULL, false);
    assert(hmi_board_update_maintenance());
    shared.maintenance = false;

    shared.maintenance = true;
    test_product.update_exclusive = false;
    hmi_board_update_poll(&core, true);
    assert(shared.admitted && !hmi_board_update_exclusive());
    /* UDS 寻址来自升级策略：零值使用 0x7E0/0x7E8，帧只在策略指定的总线与标识上被消费。 */
    hmi_can_frame_t foreign = {.bus = HMI_BUS_CAN1, .id = rx_id, .size = 1u};
    assert(!hmi_board_update_frame(&foreign));
    test_optional_storage_barrier();
    test_snapshot_contract();
    return 0;
}

/* 本测试不启动线程或访问设备；意外越过取消路径立即失败。 */
#define UNEXPECTED() assert(!"unexpected hardware or scheduler call")
void rt_hw_cpu_reset(void)
{
    UNEXPECTED();
}
uint32_t rt_tick_get_millisecond(void)
{
    UNEXPECTED();
    return 0;
}
int rt_tick_from_millisecond(int n)
{
    UNEXPECTED();
    return n;
}
void rt_thread_mdelay(int n)
{
    (void)n;
    UNEXPECTED();
}
int rt_kprintf(const char *s, ...)
{
    assert(diagnostic_test);
    char bounded[128];
    va_list args;
    va_start(args, s);
    int n = vsnprintf(bounded, sizeof(bounded), s, args);
    va_end(args);
    assert(n >= 0 && (size_t)n < sizeof(bounded));
    assert(console_used + (size_t)n < sizeof(console_output));
    memcpy(console_output + console_used, bounded, (size_t)n);
    console_used += (size_t)n;
    return n;
}
int rt_mutex_init(struct rt_mutex *m, const char *s, int f)
{
    (void)m;
    (void)s;
    (void)f;
    UNEXPECTED();
    return -1;
}
int rt_event_init(struct rt_event *e, const char *s, int f)
{
    (void)e;
    (void)s;
    (void)f;
    UNEXPECTED();
    return -1;
}
int rt_event_recv(struct rt_event *e, unsigned b, int f, int t, rt_uint32_t *r)
{
    (void)e;
    (void)b;
    (void)f;
    (void)t;
    (void)r;
    UNEXPECTED();
    return -1;
}
int rt_event_send(struct rt_event *e, unsigned b)
{
    assert(barrier_test && e == &barrier_event && b == 1u);
    ++barrier_events;
    return RT_EOK;
}
int rt_mq_init(struct rt_messagequeue *q, const char *s, void *p, size_t a, size_t b, int f)
{
    (void)q;
    (void)s;
    (void)p;
    (void)a;
    (void)b;
    (void)f;
    UNEXPECTED();
    return -1;
}
int rt_thread_init(struct rt_thread *t, const char *s, void (*fn)(void *), void *a, void *stack, size_t n,
                   int p, int tick)
{
    (void)t;
    (void)s;
    (void)fn;
    (void)a;
    (void)stack;
    (void)n;
    (void)p;
    (void)tick;
    UNEXPECTED();
    return -1;
}
int rt_thread_startup(struct rt_thread *t)
{
    (void)t;
    UNEXPECTED();
    return -1;
}
const hmi_product_t *hmi_product_get(void)
{
    assert(diagnostic_test);
    return &test_product;
}
hmi_update_backend_t hmi_aic_update_backend(void)
{
    UNEXPECTED();
    return (hmi_update_backend_t){0};
}
bool hmi_aic_update_prepare(void)
{
    UNEXPECTED();
    return false;
}
bool hmi_aic_update_supported(void)
{
    assert(diagnostic_test);
    return false;
}
const char *hmi_aic_update_reason(void)
{
    assert(diagnostic_test);
    return "test";
}
uint32_t hmi_aic_update_capacity(void)
{
    assert(diagnostic_test);
    return 0;
}
int hmi_aic_update_confirm(void)
{
    UNEXPECTED();
    return -1;
}
bool hmi_board_nvm_ready(void)
{
    assert(diagnostic_test);
    return nvm_ready;
}
uint64_t hmi_board_nvm_flush(void)
{
    assert(barrier_test);
    ++flush_requests;
    return durable_revision;
}
bool hmi_board_nvm_barrier(uint64_t n)
{
    assert(barrier_test && n == durable_revision && n != 0);
    return durable_ready;
}
bool hmi_board_can_raw_read(void *ctx, hmi_can_frame_t *f)
{
    (void)ctx;
    (void)f;
    UNEXPECTED();
    return false;
}
bool hmi_board_can_send(const hmi_can_frame_t *f)
{
    (void)f;
    UNEXPECTED();
    return false;
}

bool hmi_execution_can_submit(const hmi_can_frame_t *frame, bool urgent)
{
    (void)frame;
    (void)urgent;
    UNEXPECTED();
    return false;
}

uint32_t hmi_board_now_ms(void)
{
    return rt_tick_get_millisecond();
}
