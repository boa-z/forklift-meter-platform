#include <assert.h>
/* Deterministic test-only OS world. No scheduler or device I/O. */
static unsigned changes;
static bool lifecycle_test;
static unsigned init_calls, fail_init, startup_calls, fail_startup, wake_bindings;
static unsigned close_calls, flush_calls, stop_calls;
static bool nvm_start_ok = true, nvm_ready = true, nvm_stopped = true, barrier_ready;
static unsigned settings_boot_calls;
static meter_can_rate_t boot_rate;
bool meter_board_settings_boot(const meter_snapshot_t *snapshot)
{
    assert(nvm_ready);
    ++settings_boot_calls;
    boot_rate = snapshot->can_rate;
    return true;
}
bool meter_board_backlight_apply(uint8_t brightness, uint32_t now)
{
    (void)brightness; (void)now; return true;
}
static uint64_t flush_ticket;
static uint32_t fake_now;
static void (*app_wait_hook)(void);
static bool init_fails(void)
{
    ++init_calls;
    return fail_init == init_calls;
}
/* 本测试不启动调度器；意外进入硬件或线程入口立即失败。 */
int rt_mutex_init(struct rt_mutex *m, const char *n, int f)
{
    (void)n;
    (void)f;
    if (init_fails())
        return -1;
    m->depth = 0u;
    return 0;
}
int rt_sem_init(struct rt_semaphore *s, const char *n, unsigned v, int f)
{
    (void)n;
    (void)f;
    if (init_fails())
        return -1;
    s->value = v;
    return 0;
}
int rt_sem_control(struct rt_semaphore *s, int c, void *a)
{
    (void)c;
    (void)a;
    s->value = 0u;
    return 0;
}
int rt_mq_init(struct rt_messagequeue *q, const char *n, void *p, size_t size, size_t bytes, int f)
{
    (void)n;
    (void)p;
    (void)f;
    if (init_fails())
        return -1;
    memset(q, 0, sizeof(*q));
    q->size = size;
    q->limit = (unsigned)(bytes / (RT_ALIGN(size, RT_ALIGN_SIZE) + sizeof(void *)));
    return 0;
}
int rt_thread_init(struct rt_thread *t, const char *n, void (*e)(void *), void *a, void *s, size_t z, int p,
                   int k)
{
    (void)t;
    (void)n;
    (void)e;
    (void)a;
    (void)s;
    (void)z;
    (void)p;
    (void)k;
    assert(lifecycle_test);
    return init_fails() ? -1 : 0;
}
int rt_thread_startup(struct rt_thread *t)
{
    (void)t;
    assert(lifecycle_test);
    ++startup_calls;
    return startup_calls == fail_startup ? -1 : 0;
}
void rt_thread_mdelay(int ms)
{
    (void)ms;
    assert(0);
}
int rt_tick_from_millisecond(int ms)
{
    return ms;
}
uint32_t rt_tick_get_millisecond(void)
{
    return fake_now;
}
int rt_kprintf(const char *fmt, ...)
{
    (void)fmt;
    return 0;
}
void meter_debug_lock(void) {}
void meter_debug_unlock(void) {}
meter_rtthread_board_port_t meter_board_port(meter_diagnostics_t *d)
{
    (void)d;
    assert(0);
    return (meter_rtthread_board_port_t){0};
}
void meter_board_can_wake(struct rt_semaphore *s)
{
    (void)s;
    assert(lifecycle_test);
    ++wake_bindings;
}
bool meter_board_can_raw_read(void *c, meter_can_frame_t *f)
{
    (void)c;
    (void)f;
    assert(0);
    return false;
}
bool meter_board_can_send(const meter_can_frame_t *f)
{
    (void)f;
    assert(0);
    return false;
}
void meter_board_can_diagnostics(meter_diagnostics_t *d)
{
    (void)d;
    assert(0);
}
void meter_board_can_close(meter_diagnostics_t *d)
{
    (void)d;
    assert(lifecycle_test);
    ++close_calls;
}
bool meter_board_nvm_start(meter_core_t *c)
{
    (void)c;
    assert(lifecycle_test);
    nvm_stopped = !nvm_start_ok;
    return nvm_start_ok;
}
void meter_board_nvm_poll(uint32_t t, meter_diag_storage_t *s)
{
    (void)t;
    (void)s;
    assert(lifecycle_test);
    ++wake_bindings;
}
uint64_t meter_board_nvm_flush(void)
{
    assert(lifecycle_test);
    ++flush_calls;
    return flush_ticket;
}
bool meter_board_nvm_barrier(uint64_t t)
{
    (void)t;
    assert(lifecycle_test);
    return barrier_ready;
}
void meter_board_nvm_stop(void)
{
    assert(lifecycle_test);
    ++stop_calls;
}
bool meter_board_nvm_stopped(void)
{
    assert(lifecycle_test);
    return nvm_stopped;
}
int rt_mutex_take(struct rt_mutex *m, int timeout)
{
    (void)timeout;
    assert(m->depth == 0u);
    ++m->depth;
    return 0;
}
int rt_mutex_release(struct rt_mutex *m)
{
    assert(m->depth == 1u);
    --m->depth;
    return 0;
}
int rt_sem_take(struct rt_semaphore *s, int timeout)
{
    if (lifecycle_test && s == &app_event && timeout > 0 && app_wait_hook)
        app_wait_hook();
    if (!s->value)
        return -1;
    --s->value;
    return 0;
}
int rt_sem_release(struct rt_semaphore *s)
{
    ++s->value;
    return 0;
}
rt_base_t rt_hw_interrupt_disable(void)
{
    return 0;
}
void rt_hw_interrupt_enable(rt_base_t level)
{
    (void)level;
}
int rt_mq_send(struct rt_messagequeue *q, const void *data, size_t size)
{
    assert(size <= 1024u);
    if (q->entry == q->limit)
        return -1;
    q->size = size;
    memcpy(q->data[q->tail], data, size);
    q->tail = (q->tail + 1u) % 64u;
    ++q->entry;
    return 0;
}
int rt_mq_recv(struct rt_messagequeue *q, void *data, size_t size, int timeout)
{
    (void)timeout;
    if (!q->entry)
        return -1;
    assert(size == q->size);
    memcpy(data, q->data[q->head], size);
    q->head = (q->head + 1u) % 64u;
    --q->entry;
    return 0;
}
bool meter_board_nvm_ready(void)
{
    return nvm_ready;
}
bool meter_board_nvm_changed(uint32_t now)
{
    (void)now;
    ++changes;
    return true;
}
