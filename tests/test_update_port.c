/* 直接执行板端适配的取消路径；IPC 替身检查额度和互斥锁契约。 */
#include "platform/rtthread/meter_update_port.c"
#include <assert.h>
#include <stdarg.h>
static char console_output[1024];
static size_t console_used;
static bool diagnostic_test;
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
    service.state = METER_UPDATE_DOWNLOADING;
    service.generation = 9;
    service.opened = true;
    service.backend.abort = release_test;
}
int main(void)
{
    meter_update_job_t job = {.kind = METER_UPDATE_JOB_WRITE}, discarded;
    meter_update_error_t error;
    uint32_t generation, offset;
    setup();
    assert(submit(NULL, &job) && !submit(NULL, &job));
    shared.cancel = true;
    assert(!submit(NULL, &job));
    cancel_work(&discarded);
    assert(released == 1 && !jobs.full && shared.busy && !shared.cancel);
    assert(result(NULL, &error, &generation, &offset));
    assert(error == METER_UPDATE_DENIED && generation == 9 && !shared.busy);
    assert(submit(NULL, &job));

    setup();
    assert(submit(NULL, &job));
    completion_t old = {METER_UPDATE_OK, 9, 512};
    assert(rt_mq_send(&results, &old, sizeof(old)) == 0);
    cancel(NULL);
    assert(!result(NULL, &error, &generation, &offset));
    cancel_work(&discarded);
    assert(!results.full && !jobs.full && !shared.busy && !shared.cancel);
    assert(!result(NULL, &error, &generation, &offset) && submit(NULL, &job));

    setup();
    service.state = METER_UPDATE_CANDIDATE;
    cancel(NULL);
    cancel_work(&discarded);
    assert(service.state == METER_UPDATE_CANDIDATE && released == 0);
    setup();
    service.state = METER_UPDATE_ACTIVATED;
    cancel(NULL);
    cancel_work(&discarded);
    assert(service.state == METER_UPDATE_ACTIVATED && released == 0);
    setup();
    diagnostic_test = true;
    shared.started = true;
    char *args[] = {"meter_update", "info"};
    uint8_t expected[896];
    size_t expected_size = info(NULL, expected, sizeof(expected));
    assert(expected_size > 128);
    assert(meter_update(2, args) == 0);
    assert(console_used == expected_size + 1);
    assert(memcmp(console_output, expected, expected_size) == 0);
    assert(console_output[expected_size] == '\n');
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
    (void)e;
    (void)b;
    UNEXPECTED();
    return -1;
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
const meter_product_t *meter_product_get(void)
{
    assert(diagnostic_test);
    static const meter_product_t product = {.id = "synthetic"};
    return &product;
}
meter_update_backend_t meter_aic_update_backend(meter_aic_update_t *a)
{
    (void)a;
    UNEXPECTED();
    return (meter_update_backend_t){0};
}
bool meter_aic_update_prepare(void)
{
    UNEXPECTED();
    return false;
}
bool meter_aic_update_supported(void)
{
    assert(diagnostic_test);
    return false;
}
const char *meter_aic_update_reason(void)
{
    assert(diagnostic_test);
    return "test";
}
uint32_t meter_aic_update_capacity(void)
{
    assert(diagnostic_test);
    return 0;
}
int meter_aic_update_confirm(void)
{
    UNEXPECTED();
    return -1;
}
bool meter_board_nvm_ready(void)
{
    UNEXPECTED();
    return false;
}
uint64_t meter_board_nvm_flush(void)
{
    UNEXPECTED();
    return 0;
}
bool meter_board_nvm_barrier(uint64_t n)
{
    (void)n;
    UNEXPECTED();
    return false;
}
bool meter_board_can_raw_read(void *ctx, meter_can_frame_t *f)
{
    (void)ctx;
    (void)f;
    UNEXPECTED();
    return false;
}
bool meter_board_can_send(const meter_can_frame_t *f)
{
    (void)f;
    UNEXPECTED();
    return false;
}
