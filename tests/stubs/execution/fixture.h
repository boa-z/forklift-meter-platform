/* 确定性测试世界：没有调度器与设备；线程入口不运行，测试直接调用单轮函数。 */
#include <assert.h>
#include <string.h>
void test_boot_log(const char *format, ...)
{
    (void)format;
}
static uint32_t fake_now;
static unsigned changes, opened[METER_BUS_COUNT], sends, backlight_calls;
static unsigned stops[METER_BUS_COUNT];
static bool stop_ok = true;
static uint32_t opened_bitrate;
static bool nvm_ready = true, send_ok = true;
static meter_can_frame_t sent[16];
static meter_can_frame_t rx_queue[8];
static unsigned rx_count;
int rt_sem_init(struct rt_semaphore *s, const char *n, unsigned v, int f)
{
    (void)n;
    (void)f;
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
int rt_sem_take(struct rt_semaphore *s, int timeout)
{
    (void)timeout;
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
int rt_mq_init(struct rt_messagequeue *q, const char *n, void *p, size_t size, size_t bytes, int f)
{
    (void)n;
    (void)p;
    (void)f;
    memset(q, 0, sizeof(*q));
    q->size = size;
    q->limit = (unsigned)(bytes / (RT_ALIGN(size, RT_ALIGN_SIZE) + sizeof(void *)));
    return 0;
}
int rt_mq_send(struct rt_messagequeue *q, const void *data, size_t size)
{
    assert(size <= 1024u && size == q->size);
    if (q->entry == q->limit)
        return -1;
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
int rt_thread_init(struct rt_thread *t, const char *n, void (*e)(void *), void *a, void *s, size_t z, int p, int k)
{
    (void)t;
    (void)n;
    (void)e;
    (void)a;
    (void)s;
    (void)z;
    (void)p;
    (void)k;
    return 0;
}
int rt_thread_startup(struct rt_thread *t)
{
    (void)t;
    return 0;
}
void rt_thread_mdelay(int ms)
{
    (void)ms;
}
int rt_tick_from_millisecond(int ms)
{
    return ms;
}
int rt_kprintf(const char *fmt, ...)
{
    (void)fmt;
    return 0;
}
void meter_debug_lock(void) {}
void meter_debug_unlock(void) {}
uint32_t meter_board_now_ms(void)
{
    return fake_now;
}
void meter_board_can_wake(struct rt_semaphore *s)
{
    (void)s;
}
bool meter_board_can_open(meter_bus_role_t bus, uint32_t bitrate, meter_diagnostics_t *d)
{
    (void)d;
    ++opened[bus];
    opened_bitrate = bitrate;
    return true;
}
bool meter_board_can_read(meter_can_frame_t *f)
{
    if (!rx_count)
        return false;
    *f = rx_queue[0];
    memmove(rx_queue, rx_queue + 1, (--rx_count) * sizeof(rx_queue[0]));
    return true;
}
bool meter_board_can_send(const meter_can_frame_t *f)
{
    assert(sends < 16u);
    sent[sends++] = *f;
    return send_ok;
}
bool meter_board_can_stop(meter_bus_role_t bus)
{
    ++stops[bus];
    return stop_ok;
}
void meter_board_can_diagnostics(meter_diagnostics_t *d)
{
    (void)d;
}
bool meter_board_settings_boot(const meter_snapshot_t *snapshot)
{
    (void)snapshot;
    return true;
}
uint32_t meter_board_can_bitrate(void)
{
    return 250000u;
}
bool meter_board_backlight_apply(uint8_t brightness, uint32_t now)
{
    (void)brightness;
    (void)now;
    ++backlight_calls;
    return true;
}
bool meter_board_nvm_start(meter_core_t *c)
{
    (void)c;
    return true;
}
void meter_board_nvm_poll(uint32_t t, meter_diag_storage_t *s)
{
    (void)t;
    (void)s;
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
uint64_t meter_board_nvm_flush(void)
{
    return 0u;
}
bool meter_board_nvm_barrier(uint64_t t)
{
    (void)t;
    return true;
}
