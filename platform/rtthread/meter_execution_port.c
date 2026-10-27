/* SPDX-License-Identifier: Apache-2.0 */
#define LOG_TAG "meter.exec"
#define LOG_LVL LOG_LVL_INFO
#include "platform/rtthread/meter_execution_port.h"
#include "contracts/meter_time.h"
#include "core/meter_snapshot.h"
#include "meter_display_version.h"
#include "platform/rtthread/debug/meter_debug_console.h"
#include "platform/rtthread/meter_board_port.h"
#include "platform/rtthread/meter_board_settings.h"
#include "platform/rtthread/meter_nvm_port.h"
#include "runtime/meter_periodic.h"
#include "runtime/meter_runtime.h"
#ifdef METER_ENABLE_CAN_UPDATE
#include "platform/rtthread/meter_update_port.h"
#include "update/meter_update.h"
#endif
#include <finsh.h>
#include <rtthread.h>
#include <stdio.h>
#include <string.h>
#include <ulog.h>

/* 容量是板端静态预算，不限制 Product 目录大小；周期条目超过预算时启动失败。 */
#ifndef METER_BOARD_PERIODIC_SLOTS
#define METER_BOARD_PERIODIC_SLOTS 8u
#endif
#define TX_DEPTH 16u
#define ACTION_DEPTH 8u
#define RX_BUDGET 64u
#define NO_PERIODIC 0xFFu
#define POOL_WORDS(type, count)                                                                              \
    (((RT_ALIGN(sizeof(type), RT_ALIGN_SIZE) + sizeof(void *)) / sizeof(rt_ubase_t)) * (count))

typedef struct
{
    meter_can_frame_t frame;
    uint32_t generation, expires_ms;
    uint8_t periodic;   /* 周期条目序号；NO_PERIODIC 表示普通帧 */
    bool session;       /* 属于某个会话代次：代次切换后未发出的帧作废 */
    bool expires;       /* expires_ms 之前未交给驱动则作废 */
} tx_message_t;
typedef struct
{
    struct rt_messagequeue ordinary, urgent;
    struct rt_semaphore wake;
    rt_ubase_t ordinary_pool[POOL_WORDS(tx_message_t, TX_DEPTH)], urgent_pool[POOL_WORDS(tx_message_t, TX_DEPTH)];
    struct rt_thread thread;
    rt_ubase_t stack[2048u / sizeof(rt_ubase_t)];
    bool open;
    volatile bool stop_requested;
    bool stopped;
    unsigned burst;
    int backoff_ms;
    /* 以下计数只由本总线的发送线程写。 */
    volatile uint32_t ok, error, dropped, last_ms;
} tx_worker_t;

static meter_execution_config_t config;
static meter_runtime_t runtime;
static meter_diagnostics_t app_diag;
static uint32_t trace_cursor;
static tx_worker_t tx[METER_BUS_COUNT];
static meter_deadline_t periodic_deadline[METER_BOARD_PERIODIC_SLOTS];
static volatile bool periodic_in_flight[METER_BOARD_PERIODIC_SLOTS];
static struct rt_semaphore wake;
static struct rt_messagequeue actions;
static rt_ubase_t action_pool[POOL_WORDS(meter_action_t, ACTION_DEPTH)];
static struct rt_thread app_thread;
static rt_ubase_t app_stack[12288u / sizeof(rt_ubase_t)];
/* 发布区：meter_debug_lock 同时保护公共诊断与给 UI 的快照，查询端读同一份数据。 */
static meter_snapshot_t published;
static meter_update_view_t published_update;
static bool view_ready, view_normal;
/* 运行时线程写、其他线程只读的字宽状态。 */
static volatile bool initialized, started, maintenance, ui_healthy;
static volatile uint32_t generation, app_runs, ui_ticks;
static uint32_t suppressed, actions_rejected, tx_full, observed_revision;

static bool tx_enqueue(const meter_can_frame_t *frame, bool urgent, bool session, uint32_t expires_ms,
                       bool expires, uint8_t periodic)
{
    if (!frame || (unsigned)frame->bus >= METER_BUS_COUNT || frame->size > 8u || !tx[frame->bus].open ||
        tx[frame->bus].stop_requested)
        return false;
    tx_worker_t *t = &tx[frame->bus];
    tx_message_t m = {.frame = *frame, .generation = generation, .expires_ms = expires_ms,
                      .periodic = periodic, .session = session, .expires = expires};
    if (rt_mq_send(urgent ? &t->urgent : &t->ordinary, &m, sizeof(m)) != RT_EOK)
    {
        meter_diag_increment(&tx_full);
        return false;
    }
    (void)rt_sem_release(&t->wake);
    return true;
}
bool meter_execution_can_submit(const meter_can_frame_t *frame, bool urgent)
{
    return started && tx_enqueue(frame, urgent, !urgent, 0u, false, NO_PERIODIC);
}
static bool runtime_send(void *context, const meter_can_frame_t *frame)
{
    (void)context;
    return tx_enqueue(frame, false, true, 0u, false, NO_PERIODIC);
}
static bool runtime_stop(void *context, meter_bus_role_t bus)
{
    (void)context;
    if ((unsigned)bus >= METER_BUS_COUNT || !tx[bus].open)
        return false;
    tx[bus].stop_requested = true;
    (void)rt_sem_release(&tx[bus].wake);
    return true;
}
static const meter_can_tx_port_t runtime_tx = {.send = runtime_send, .stop = runtime_stop};

/* 发送线程的一次服务：取一帧并交给驱动；队列为空返回 false。 */
static bool tx_service(tx_worker_t *t)
{
    if (t->stop_requested && !t->stopped)
    {
        t->stopped = meter_board_can_stop((meter_bus_role_t)(t - tx));
        if (!t->stopped)
            rt_thread_mdelay(100);
        return true;
    }
    tx_message_t m;
    bool got = false;
    /* 升级帧优先，但最多连续四帧，随后给周期/协议帧一次机会。 */
    if (t->burst < 4u && rt_mq_recv(&t->urgent, &m, sizeof(m), 0) == RT_EOK)
    {
        got = true;
        ++t->burst;
    }
    else
    {
        t->burst = 0u;
        got = rt_mq_recv(&t->ordinary, &m, sizeof(m), 0) == RT_EOK ||
              rt_mq_recv(&t->urgent, &m, sizeof(m), 0) == RT_EOK;
    }
    if (!got)
        return false;
    uint32_t now = meter_board_now_ms();
    bool stale = t->stop_requested || (m.session && m.generation != generation) ||
                 (m.expires && meter_time_reached(now, m.expires_ms));
    bool ok = !stale && meter_board_can_send(&m.frame);
    if (m.periodic != NO_PERIODIC)
        periodic_in_flight[m.periodic] = false;
    if (stale)
        ++t->dropped;
    else if (ok)
    {
        ++t->ok;
        t->last_ms = meter_board_now_ms();
        t->backoff_ms = 100;
    }
    else
    {
        /* 无应答/总线故障时退避，限制厂商驱动同步错误输出对串口的占用。 */
        ++t->error;
        rt_thread_mdelay(t->backoff_ms);
        if (t->backoff_ms < 1000)
            t->backoff_ms += 100;
    }
    return true;
}
static void tx_entry(void *arg)
{
    tx_worker_t *t = arg;
    t->backoff_ms = 100;
    for (;;)
    {
        (void)rt_sem_control(&t->wake, RT_IPC_CMD_RESET, RT_NULL);
        if (!tx_service(t))
            (void)rt_sem_take(&t->wake, RT_WAITING_FOREVER);
    }
}

/* 会话代次切换（启动、进入/退出升级维护）：复位适配器与 Product 工作流，旧代次未发出的帧作废。 */
static void restart_session(void)
{
    meter_runtime_connection(&runtime, false);
    meter_runtime_connection(&runtime, true);
    generation = runtime.generation;
    memset(periodic_deadline, 0, sizeof(periodic_deadline));
    if (config.product->app_reset)
        config.product->app_reset(runtime.generation);
    meter_core_connection(config.core, false, runtime.generation);
    meter_core_connection(config.core, true, runtime.generation);
}
bool meter_execution_settings_allowed(void)
{
    return started && !maintenance && meter_board_nvm_ready();
}
static void apply_actions(uint32_t now)
{
    const meter_product_t *p = config.product;
    meter_action_t a;
    while (rt_mq_recv(&actions, &a, sizeof(a), 0) == RT_EOK)
    {
        bool applied = meter_execution_settings_allowed() &&
                       (a.kind == METER_ACTION_PRODUCT ? p->local_action && p->local_action(&config.core->snapshot, &a, now)
                                                       : meter_core_action(config.core, &a));
        if (!applied)
        {
            meter_diag_increment(&actions_rejected);
            LOG_W("action kind=%u id=%u rejected", (unsigned)a.kind, (unsigned)a.id);
        }
    }
}
static void periodic_poll(uint32_t now)
{
    const meter_product_t *p = config.product;
    for (size_t i = 0u; i < p->periodic_count; ++i)
    {
        const meter_periodic_frame_t *d = &p->periodic[i];
        meter_can_frame_t frame;
        if (meter_periodic_due(&periodic_deadline[i], d, &config.core->snapshot, now, !maintenance || d->critical,
                               periodic_in_flight[i], &frame))
        {
            periodic_in_flight[i] = true;
            if (!tx_enqueue(&frame, false, true, now + d->period_ms, true, (uint8_t)i))
                periodic_in_flight[i] = false;
        }
    }
}
static void copy_traces(void)
{
    const meter_trace_t *source = &app_diag.trace;
    uint32_t count = source->sequence - trace_cursor;
    if (count > source->count)
        count = source->count;
    /* 每次最多复制 16 条，其余留给下一轮。 */
    if (count > 16u)
        count = 16u;
    uint32_t pending = source->sequence - trace_cursor;
    if (pending > source->count)
        pending = source->count;
    uint32_t index = (source->write_index + METER_TRACE_CAPACITY - pending) % METER_TRACE_CAPACITY;
    trace_cursor = source->sequence - pending + count;
    for (uint32_t i = 0u; i < count; ++i)
    {
        const meter_trace_entry_t *e = &source->entries[(index + i) % METER_TRACE_CAPACITY];
        meter_trace_append(&config.public_diagnostics->trace, e->timestamp_ms, e->module, e->event, e->arg0, e->arg1);
    }
}
static void publish(void)
{
    meter_core_t *core = config.core;
    for (unsigned bus = 0u; bus < METER_BUS_COUNT; ++bus)
    {
        meter_diag_can_t *c = &app_diag.data.can[bus];
        c->tx = tx[bus].ok;
        c->tx_error = tx[bus].error;
        c->last_tx_ms = tx[bus].last_ms;
        c->tx_seen = tx[bus].ok != 0u;
    }
    meter_board_can_diagnostics(&app_diag);
    meter_update_view_t update;
#ifdef METER_ENABLE_CAN_UPDATE
    meter_board_update_view(&update);
#else
    /* 没有 CAN 升级端点时没人填更新视图，但软件版本页仍要显示本次构建的版本。 */
    memset(&update, 0, sizeof(update));
    (void)snprintf(update.current_version, sizeof(update.current_version), "%s", METER_FIRMWARE_DISPLAY_VERSION);
#endif
    bool copy = !view_ready || published.revision != core->snapshot.revision ||
                published.generation != core->snapshot.generation;
    meter_debug_lock();
    if (copy)
        view_ready = meter_snapshot_copy(&published, &config.published_storage, &core->snapshot) ==
                     METER_SNAPSHOT_COPIED;
    published_update = update;
    view_normal = !maintenance;
    meter_diagnostics_t *d = config.public_diagnostics;
    d->data.runtime = app_diag.data.runtime;
    memcpy(d->data.can, app_diag.data.can, sizeof(d->data.can));
    d->data.domain = app_diag.data.domain;
    d->data.storage = app_diag.data.storage;
    copy_traces();
    meter_debug_unlock();
}
static bool open_buses(void)
{
    const meter_product_t *p = config.product;
    bool used[METER_BUS_COUNT] = {false};
    for (size_t i = 0u; i < p->routes->count; ++i)
        used[p->routes->entries[i].bus] = true;
    for (size_t i = 0u; i < p->periodic_count; ++i)
        used[p->periodic[i].frame.bus] = true;
#ifdef METER_ENABLE_CAN_UPDATE
    if (p->update)
        used[p->update->bus] = true;
#endif
    const uint32_t bitrate = meter_board_can_bitrate();
    for (unsigned bus = 0u; bus < METER_BUS_COUNT; ++bus)
    {
        if (!used[bus])
            continue;
        if (!meter_board_can_open((meter_bus_role_t)bus, bitrate, &app_diag) ||
            rt_thread_startup(&tx[bus].thread) != RT_EOK)
            return false;
        tx[bus].open = true;
    }
    return true;
}
/* 启动顺序：先收敛设置读取（CAN 速率要在开总线前确定），再开总线、起升级端点，最后进入会话。 */
static bool app_start(void)
{
    const meter_product_t *p = config.product;
    meter_core_t *core = config.core;
    if (p->storage && p->storage->enabled)
    {
        if (!meter_board_nvm_start(core))
        {
            LOG_E("settings storage failed to start");
            return false;
        }
        while (!meter_board_nvm_ready())
        {
            meter_board_nvm_poll(meter_board_now_ms(), &app_diag.data.storage);
            if (!meter_board_nvm_ready())
                (void)rt_sem_take(&wake, rt_tick_from_millisecond(5));
        }
    }
    if (!meter_board_settings_boot(&core->snapshot) || !open_buses())
        return false;
#ifdef METER_ENABLE_CAN_UPDATE
    if (p->update && !meter_board_update_start())
        return false;
#endif
    restart_session();
    return true;
}
/* 运行时线程一轮：升级状态 → 收帧分发 → 设置动作 → Product 业务 → 周期帧 → 背光/设置保存 → 发布。
 * 返回本轮读到的帧数。 */
static size_t app_step(uint32_t now)
{
    meter_core_t *core = config.core;
    ++app_runs;
    meter_diagnostics_time(&app_diag, now);
    bool paused = false;
#ifdef METER_ENABLE_CAN_UPDATE
    meter_board_update_poll(core, ui_healthy);
    paused = meter_board_update_maintenance() || meter_board_update_exclusive();
#endif
    if (paused != maintenance)
    {
        maintenance = paused;
        restart_session();
    }
    meter_can_frame_t frame;
    size_t count = 0u;
    while (count < RX_BUDGET && meter_board_can_read(&frame))
    {
        ++count;
#ifdef METER_ENABLE_CAN_UPDATE
        if (meter_board_update_frame(&frame))
        {
            meter_diagnostics_can(&app_diag, frame.bus, METER_CAN_RX, frame.timestamp_ms);
            continue;
        }
#endif
        if (maintenance)
        {
            meter_diagnostics_can(&app_diag, frame.bus, METER_CAN_RX, frame.timestamp_ms);
            meter_diag_increment(&suppressed);
        }
        else
            (void)meter_runtime_receive(&runtime, &frame);
    }
    apply_actions(now);
    meter_runtime_step(&runtime, core, now, !maintenance);
    periodic_poll(now);
#ifdef METER_ENABLE_CAN_UPDATE
    meter_board_update_protocol();
#endif
    if (meter_board_nvm_ready())
        (void)meter_board_backlight_apply(core->snapshot.brightness, now);
    /* 快照有变化才编码设置；内容与上次相同时 NVM 服务不会产生写入。 */
    if (core->snapshot.revision != observed_revision)
    {
        observed_revision = core->snapshot.revision;
        (void)meter_board_nvm_changed(now);
    }
    meter_board_nvm_poll(now, &app_diag.data.storage);
    publish();
    return count;
}
static void app_entry(void *arg)
{
    (void)arg;
    if (!app_start())
    {
        LOG_E("runtime startup failed");
        return;
    }
    observed_revision = config.core->snapshot.revision;
    publish();
    started = true;
    LOG_I("runtime started: generation=%u", (unsigned)generation);
    for (;;)
    {
        (void)rt_sem_control(&wake, RT_IPC_CMD_RESET, RT_NULL);
        /* 高负载每 64 帧让出一次；空闲时由 RX、UI 动作或期限唤醒（升级时 1 ms，否则 5 ms）。 */
        if (app_step(meter_board_now_ms()) == RX_BUDGET)
            rt_thread_mdelay(1);
        else
            (void)rt_sem_take(&wake, rt_tick_from_millisecond(
#ifdef METER_ENABLE_CAN_UPDATE
                                         1
#else
                                         5
#endif
                                         ));
    }
}
bool meter_execution_start(const meter_execution_config_t *c)
{
    if (initialized || !c || !c->core || !c->product || !c->public_diagnostics ||
        c->product->periodic_count > METER_BOARD_PERIODIC_SLOTS || (c->product->periodic_count && !c->product->periodic))
        return false;
    for (size_t i = 0u; i < c->product->periodic_count; ++i)
        if (!meter_periodic_valid(&c->product->periodic[i]))
            return false;
    config = *c;
    meter_diagnostics_init(&app_diag);
    meter_core_bind_diagnostics(c->core, &app_diag);
    if (!meter_runtime_init(&runtime, c->product, meter_core_apply, c->core))
        return false;
    meter_runtime_bind_tx(&runtime, &runtime_tx);
    meter_runtime_bind_diagnostics(&runtime, &app_diag);
    /* 公共诊断的 Domain 查询读发布副本，与 UI 看到的是同一份。 */
    config.public_diagnostics->domain = &published;
    if (rt_sem_init(&wake, "meter_rx", 0, RT_IPC_FLAG_FIFO) != RT_EOK ||
        rt_mq_init(&actions, "ui_action", action_pool, sizeof(meter_action_t), sizeof(action_pool),
                   RT_IPC_FLAG_FIFO) != RT_EOK)
        return false;
    for (unsigned i = 0u; i < METER_BUS_COUNT; ++i)
    {
        tx_worker_t *t = &tx[i];
        if (rt_sem_init(&t->wake, i ? "tx1_wake" : "tx0_wake", 0, RT_IPC_FLAG_FIFO) != RT_EOK ||
            rt_mq_init(&t->ordinary, i ? "can1_tx" : "can0_tx", t->ordinary_pool, sizeof(tx_message_t),
                       sizeof(t->ordinary_pool), RT_IPC_FLAG_FIFO) != RT_EOK ||
            rt_mq_init(&t->urgent, i ? "can1_bulk" : "can0_bulk", t->urgent_pool, sizeof(tx_message_t),
                       sizeof(t->urgent_pool), RT_IPC_FLAG_FIFO) != RT_EOK ||
            rt_thread_init(&t->thread, i ? "meter_tx1" : "meter_tx0", tx_entry, t, t->stack, sizeof(t->stack), 18,
                           5) != RT_EOK)
            return false;
    }
    if (rt_thread_init(&app_thread, "meter_app", app_entry, NULL, app_stack, sizeof(app_stack), 19, 5) != RT_EOK)
        return false;
    meter_board_can_wake(&wake);
    initialized = true;
    return rt_thread_startup(&app_thread) == RT_EOK;
}
bool meter_execution_action(void *context, const meter_action_t *action)
{
    (void)context;
    if (!action || !meter_execution_settings_allowed() || rt_mq_send(&actions, action, sizeof(*action)) != RT_EOK)
        return false;
    (void)rt_sem_release(&wake);
    return true;
}
bool meter_execution_present(meter_snapshot_t *snapshot, const meter_core_storage_t *storage,
                              meter_update_view_t *update, bool *normal_ui)
{
    if (!started || !snapshot || !storage || !update || !normal_ui)
        return false;
    meter_debug_lock();
    bool ok = view_ready;
    if (ok && (!snapshot->catalog || snapshot->revision != published.revision ||
               snapshot->generation != published.generation))
        ok = meter_snapshot_copy(snapshot, storage, &published) == METER_SNAPSHOT_COPIED;
    *update = published_update;
    *normal_ui = view_normal;
    meter_debug_unlock();
    return ok;
}
void meter_execution_ui_diagnostics(const meter_diagnostics_t *d)
{
    if (!initialized || !d)
        return;
    meter_debug_lock();
    config.public_diagnostics->data.ui = d->data.ui;
    config.public_diagnostics->data.touch = d->data.touch;
    meter_debug_unlock();
    ui_healthy = d->data.ui.flush_count != 0u;
    /* UI 线程每轮发布一次；这个计数是看门狗的 UI 心跳，不借用刷屏计数。 */
    ++ui_ticks;
}
bool meter_execution_liveness(meter_execution_liveness_t *out)
{
    if (!out || !started)
        return false;
    *out = (meter_execution_liveness_t){.app_runs = app_runs, .ui_ticks = ui_ticks};
    return true;
}
static int meter_exec(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!initialized)
        return -RT_ERROR;
    rt_kprintf("runtime started=%u maintenance=%u generation=%u runs=%u\n", started, maintenance,
               (unsigned)generation, (unsigned)app_runs);
    rt_kprintf("actions rejected=%u rx suppressed=%u tx queue_full=%u\n", (unsigned)actions_rejected,
               (unsigned)suppressed, (unsigned)tx_full);
    for (unsigned bus = 0u; bus < METER_BUS_COUNT; ++bus)
        if (tx[bus].open)
            rt_kprintf("tx bus=%u ok=%u error=%u dropped=%u\n", bus, (unsigned)tx[bus].ok, (unsigned)tx[bus].error,
                       (unsigned)tx[bus].dropped);
    for (size_t i = 0u; i < config.product->periodic_count; ++i)
        rt_kprintf("periodic entry=%u id=0x%x period_ms=%u missed=%u late_max_ms=%u in_flight=%u\n",
                   (unsigned)i, (unsigned)config.product->periodic[i].frame.id,
                   (unsigned)config.product->periodic[i].period_ms, (unsigned)periodic_deadline[i].missed,
                   (unsigned)periodic_deadline[i].late_ms, periodic_in_flight[i]);
    return RT_EOK;
}
MSH_CMD_EXPORT(meter_exec, Runtime thread and CAN transmit counters);
