/* SPDX-License-Identifier: Apache-2.0 */
#include "platform/rtthread/meter_execution_port.h"
#include "platform/rtthread/meter_board_port.h"
#include "platform/rtthread/meter_board_settings.h"
#include "platform/rtthread/meter_nvm_port.h"
#include "platform/rtthread/debug/meter_debug_console.h"
#include "core/meter_snapshot.h"
#include "runtime/meter_execution.h"
#include "runtime/meter_periodic.h"
#include "platform/rtthread/meter_execution_budget.h"
#include "contracts/meter_time.h"
#include "runtime/meter_runtime.h"
#ifdef METER_ENABLE_CAN_UPDATE
#include "platform/rtthread/meter_update_port.h"
#endif
#include <finsh.h>
#include <rthw.h>
#include <rtthread.h>
#include <string.h>

/* 容量属于板端预算，不限制公共 Domain 或独立 Product 的目录大小。 */
#define BATCH_VALUES 32u
#define BATCH_DEPTH 64u
#define POOL_WORDS(type, count) (((RT_ALIGN(sizeof(type), RT_ALIGN_SIZE) + sizeof(void *)) / sizeof(rt_ubase_t)) * (count))
typedef struct
{
    uint32_t generation, received_ms;
    uint64_t sequence;
    uint8_t bus;
    meter_source_id_t source;
    size_t count;
    meter_update_t updates[BATCH_VALUES];
} batch_message_t;
typedef struct { uint32_t generation; uint64_t serial; meter_action_t action; } action_message_t;
typedef struct { uint32_t generation; meter_protocol_event_t event; } event_message_t;
typedef struct { uint32_t generation; meter_can_frame_t frame; meter_request_id_t command; } tx_message_t;
typedef struct { meter_request_id_t id; meter_command_t command; uint32_t timeout_ms; } command_message_t;
typedef struct
{
    size_t periodic_cursor;
    struct rt_messagequeue ordinary, urgent;
    struct rt_semaphore wake;
    rt_ubase_t ordinary_pool[POOL_WORDS(tx_message_t, 16u)], urgent_pool[POOL_WORDS(tx_message_t, 16u)];
    struct rt_thread thread;
    rt_ubase_t stack[2048u / sizeof(rt_ubase_t)];
    unsigned bus;
    bool prefer_periodic;
} tx_owner_t;
static meter_execution_config_t config;
static meter_execution_t execution;
static meter_mode_policy_t app_policy;
static meter_runtime_t protocol;
static meter_batch_builder_t builder;
static meter_update_t staging[BATCH_VALUES];
static meter_periodic_state_t periodic_state[METER_BOARD_PERIODIC_SLOTS];
static meter_tx_value_t tx_samples[METER_BOARD_TX_VALUES], tx_published_values[METER_BOARD_TX_VALUES], tx_protocol_values[METER_BOARD_TX_VALUES];
static meter_tx_publication_t tx_published = {.values = tx_published_values}, tx_protocol = {.values = tx_protocol_values};
static uint32_t tx_acquired_ms;
/* 此槽是短锁保护的原生线程间 mailbox，每项最多一个 pending/inflight/result。 */
typedef struct
{
    meter_periodic_result_t result;
    meter_periodic_result_t first_result;
    meter_periodic_message_t message;
    meter_periodic_message_t first_message;
    uint32_t replaced, expired, queue_max_ms, scheduler_max_ms, driver_max_ms;
    bool ready, inflight, result_ready;
} periodic_slot_t;
static periodic_slot_t periodic_slots[METER_BOARD_PERIODIC_SLOTS];
static struct rt_mutex tx_publication_lock;
static meter_diagnostics_t app_diag, protocol_diag;
static uint32_t app_trace_cursor, protocol_trace_cursor;
static meter_snapshot_t published_snapshot, debug_snapshot;
static struct rt_mutex state_lock, view_lock;
static struct rt_semaphore rx_event, app_event, action_credit;
static struct rt_messagequeue batches, actions, action_results, events, commands;
static rt_ubase_t command_pool[POOL_WORDS(command_message_t, 1u)];
static rt_ubase_t batch_pool[POOL_WORDS(batch_message_t, BATCH_DEPTH)];
static rt_ubase_t action_pool[POOL_WORDS(action_message_t, 1u)];
static rt_ubase_t action_result_pool[POOL_WORDS(meter_request_result_t, 1u)];
static rt_ubase_t event_pool[POOL_WORDS(event_message_t, 16u)];
static struct rt_thread protocol_thread, app_thread;
static rt_ubase_t protocol_stack[8192u / sizeof(rt_ubase_t)], app_stack[8192u / sizeof(rt_ubase_t)];
static tx_owner_t tx_owner[METER_BUS_COUNT];
static struct
{
    meter_execution_state_t state;
    meter_mode_t mode;
    meter_mode_policy_t policy;
    uint32_t generation;
    bool stop_requested, protocol_ready, protocol_failed, protocol_done, tx_done[METER_BUS_COUNT];
    bool ui_healthy, view_ready, normal_ui, ui_release, ui_done;
    uint32_t batch_full, batch_stale, batch_rejected, event_full, suppressed;
    uint64_t action_serial;
    uint32_t tx_ok[METER_BUS_COUNT], tx_error[METER_BUS_COUNT], tx_full, tx_cancelled, last_tx[METER_BUS_COUNT];
    uint32_t batch_high, tx_high[METER_BUS_COUNT], missed, max_late_ms;
    uint32_t protocol_runs, app_runs, ui_ticks, stop_wait_ms;
    meter_update_view_t update;
    bool command_reserved;
    meter_request_id_t command_id;
    meter_command_stage_t command_stage, command_completion;
    uint32_t command_sent, command_done[METER_BUS_COUNT], command_failed[METER_BUS_COUNT];
    uint64_t command_serial;
} shared;
static bool initialized;
static meter_request_id_t active_command;
static uint32_t command_deadline;
static bool command_terminal(meter_command_stage_t stage, meter_command_stage_t completion)
{
    return stage == completion || stage == METER_COMMAND_REMOTE_CONFIRMED || stage == METER_COMMAND_FAILED || stage == METER_COMMAND_CANCELLED;
}
static void command_progress(void *context, meter_request_id_t id, meter_command_stage_t stage)
{
    (void)context;
    (void)rt_mutex_take(&state_lock, RT_WAITING_FOREVER);
    if (shared.command_reserved && meter_request_id_equal(shared.command_id, id) &&
        !command_terminal(shared.command_stage, shared.command_completion) && (unsigned)stage <= (unsigned)METER_COMMAND_CANCELLED &&
        (unsigned)stage >= (unsigned)shared.command_stage)
        shared.command_stage = stage;
    (void)rt_mutex_release(&state_lock);
}

static void lock_state(void) { (void)rt_mutex_take(&state_lock, RT_WAITING_FOREVER); }
static void unlock_state(void) { (void)rt_mutex_release(&state_lock); }
static void copy_traces(const meter_diagnostics_t *source, uint32_t *cursor)
{
    uint32_t count = source->trace.sequence - *cursor;
    if (count > source->trace.count) count = source->trace.count;
    uint32_t index = (source->trace.write_index + METER_TRACE_CAPACITY - count) % METER_TRACE_CAPACITY;
    /* 每次短锁最多复制 16 条，其余留给下一次 owner 发布。 */
    if (count > 16u) count = 16u;
    *cursor = source->trace.sequence - (source->trace.count < source->trace.sequence - *cursor ?
        source->trace.count : source->trace.sequence - *cursor) + count;
    meter_debug_lock();
    for (uint32_t i = 0u; i < count; ++i)
    {
        const meter_trace_entry_t *e = &source->trace.entries[(index + i) % METER_TRACE_CAPACITY];
        meter_trace_append(&config.public_diagnostics->trace, e->timestamp_ms, e->module, e->event, e->arg0, e->arg1);
    }
    meter_debug_unlock();
}
static meter_batch_result_t submit_batch(void *context, const meter_update_batch_t *b)
{
    (void)context;
    if (!b || !b->updates || b->count > BATCH_VALUES) return METER_BATCH_TOO_LARGE;
    batch_message_t message = {.generation = b->generation, .received_ms = b->received_ms,
        .sequence = b->sequence, .bus = b->bus, .source = b->source, .count = b->count};
    memcpy(message.updates, b->updates, b->count * sizeof(*b->updates));
    if (rt_mq_send(&batches, &message, sizeof(message)) != RT_EOK)
    {
        lock_state(); meter_diag_increment(&shared.batch_full); unlock_state();
        return METER_BATCH_IPC_FULL;
    }
    rt_base_t level = rt_hw_interrupt_disable();
    uint32_t depth = batches.entry;
    rt_hw_interrupt_enable(level);
    lock_state(); if (depth > shared.batch_high) shared.batch_high = depth; unlock_state();
    (void)rt_sem_release(&app_event);
    return METER_BATCH_QUEUED;
}
static bool submit_event(void *context, const meter_protocol_event_t *event)
{
    (void)context;
    if (!event) return false;
    event_message_t message = {.generation = protocol.generation, .event = *event};
    if (rt_mq_send(&events, &message, sizeof(message)) != RT_EOK)
    {
        lock_state(); meter_diag_increment(&shared.event_full); unlock_state();
        return false;
    }
    (void)rt_sem_release(&app_event);
    return true;
}
bool meter_execution_can_submit(const meter_can_frame_t *frame, bool urgent)
{
    if (!initialized || !frame || (unsigned)frame->bus >= METER_BUS_COUNT || frame->size > 8u) return false;
    lock_state();
    bool allowed = shared.state == METER_EXEC_RUNNING && shared.generation == protocol.generation && !shared.stop_requested;
    uint32_t generation = shared.generation;
    unlock_state();
    if (!allowed) return false;
    tx_message_t message = {.generation = generation, .frame = *frame, .command = protocol.command_request};
    tx_owner_t *owner = &tx_owner[frame->bus];
    struct rt_messagequeue *queue = urgent ? &owner->urgent : &owner->ordinary;
    if (rt_mq_send(queue, &message, sizeof(message)) != RT_EOK)
    {
        lock_state(); meter_diag_increment(&shared.tx_full); unlock_state();
        return false;
    }
    (void)rt_sem_release(&owner->wake);
    if (message.command.serial)
    { lock_state(); ++shared.command_sent; unlock_state(); }
    rt_base_t level = rt_hw_interrupt_disable();
    uint32_t depth = owner->urgent.entry + owner->ordinary.entry;
    rt_hw_interrupt_enable(level);
    lock_state(); if (depth > shared.tx_high[frame->bus]) shared.tx_high[frame->bus] = depth; unlock_state();
    return true;
}
static bool send_protocol(void *context, const meter_can_frame_t *frame)
{
    (void)context;
    return meter_execution_can_submit(frame, false);
}
static bool periodic_take(tx_owner_t *owner, meter_periodic_message_t *message)
{
    lock_state();
    for (size_t k = 0u; k < config.product->periodic_count; ++k)
    {
        size_t i = (owner->periodic_cursor + k) % config.product->periodic_count;
        periodic_slot_t *slot = &periodic_slots[i];
        if (slot->ready && slot->message.frame.bus == (meter_bus_role_t)owner->bus)
        {
            *message = slot->message; slot->ready = false; slot->inflight = true;
            owner->periodic_cursor = (i + 1u) % config.product->periodic_count;
            unlock_state(); return true;
        }
    }
    unlock_state(); return false;
}
static bool ordinary_take(tx_owner_t *owner, tx_message_t *message, meter_periodic_message_t *periodic, bool *is_periodic)
{
    bool got = false;
    if (owner->prefer_periodic) { *is_periodic = periodic_take(owner, periodic); got = *is_periodic; }
    if (!got) got = rt_mq_recv(&owner->ordinary, message, sizeof(*message), 0) == RT_EOK;
    if (!got) { *is_periodic = periodic_take(owner, periodic); got = *is_periodic; }
    if (got) owner->prefer_periodic = !*is_periodic;
    return got;
}
static void periodic_result(const meter_periodic_message_t *message, uint32_t started, uint32_t finished, bool ok, bool cancelled)
{
    lock_state();
    periodic_slot_t *slot = &periodic_slots[message->entry];
    slot->result = (meter_periodic_result_t){.generation = message->generation, .ticket = message->ticket,
        .started_ms = started, .completed_ms = finished, .success = ok};
    if (slot->first_message.generation == message->generation && slot->first_message.ticket == message->ticket)
        slot->first_result = slot->result;
    slot->inflight = false; slot->result_ready = true;
    uint32_t delay = started - message->queued_ms, driver = finished - started;
    if (delay > slot->queue_max_ms) slot->queue_max_ms = delay;
    if (driver > slot->driver_max_ms) slot->driver_max_ms = driver;
    if (cancelled) ++slot->expired;
    unlock_state(); (void)rt_sem_release(&rx_event);
}
static void tx_entry(void *arg)
{
    tx_owner_t *owner = arg;
    unsigned burst = 0u;
    int failure_wait_ms = 100;
    for (;;)
    {
        (void)rt_sem_control(&owner->wake, RT_IPC_CMD_RESET, RT_NULL);
        tx_message_t message = {0};
        meter_periodic_message_t periodic_message = {0};
        bool is_periodic = false;
        bool got = false;
        /* 最多连续四个升级帧，随后给周期/协议帧一次机会。 */
        if (burst >= 4u)
        {
            got = ordinary_take(owner, &message, &periodic_message, &is_periodic);
            burst = 0u;
        }
        if (!got && rt_mq_recv(&owner->urgent, &message, sizeof(message), 0) == RT_EOK)
        { got = true; ++burst; }
        if (!got) got = ordinary_take(owner, &message, &periodic_message, &is_periodic);
        lock_state();
        bool stop = shared.state == METER_EXEC_STOPPING;
        uint32_t generation = shared.generation;
        unlock_state();
        if (got)
        {
            uint32_t started = meter_board_now_ms();
            bool cancelled = stop || (is_periodic ? !meter_periodic_sendable(&periodic_message, generation, started) : message.generation != generation);
            if (cancelled)
            {
                if (is_periodic) periodic_result(&periodic_message, started, started, false, true);
                lock_state(); meter_diag_increment(&shared.tx_cancelled); unlock_state(); continue;
            }
            bool ok = meter_board_can_send(is_periodic ? &periodic_message.frame : &message.frame);
            if (is_periodic) periodic_result(&periodic_message, started, meter_board_now_ms(), ok, false);
            lock_state();
            if (ok) { meter_diag_increment(&shared.tx_ok[owner->bus]); shared.last_tx[owner->bus] = meter_board_now_ms(); }
            else meter_diag_increment(&shared.tx_error[owner->bus]);
            if (message.command.serial && meter_request_id_equal(message.command, shared.command_id))
            {
                if (ok) ++shared.command_done[owner->bus];
                else ++shared.command_failed[owner->bus];
            }
            unlock_state();
            /* 无 ACK/总线故障时退避，限制厂商驱动同步错误输出对 UART 的占用。 */
            if (!ok)
            {
                rt_thread_mdelay(failure_wait_ms);
                if (failure_wait_ms < 1000) failure_wait_ms += 100;
            }
            else failure_wait_ms = 100;
        }
        else if (stop) break;
        else (void)rt_sem_take(&owner->wake, RT_WAITING_FOREVER);
    }
    lock_state(); shared.tx_done[owner->bus] = true; unlock_state();
    (void)rt_sem_release(&app_event);
}
static void command_poll(uint32_t now, uint32_t generation, bool allowed)
{
    command_message_t message;
    if (rt_mq_recv(&commands, &message, sizeof(message), 0) == RT_EOK)
    {
        active_command = message.id;
        command_deadline = now + message.timeout_ms;
        if (!allowed || message.id.session != generation)
            command_progress(NULL, message.id, METER_COMMAND_CANCELLED);
        else
            (void)meter_runtime_command_request(&protocol, &message.command, message.id, command_progress, NULL);
    }
    if (!active_command.serial) return;
    lock_state();
    meter_command_stage_t stage = shared.command_stage;
    meter_command_stage_t completion = shared.command_completion;
    uint32_t sent = shared.command_sent;
    uint32_t completed = shared.command_done[0] + shared.command_done[1];
    uint32_t failed = shared.command_failed[0] + shared.command_failed[1];
    unlock_state();
    if (command_terminal(stage, completion)) { active_command = (meter_request_id_t){0}; return; }
    if (!allowed || active_command.session != generation)
    { meter_runtime_cancel(&protocol, active_command); command_progress(NULL, active_command, METER_COMMAND_CANCELLED); }
    else if (failed || meter_time_reached(now, command_deadline))
    { meter_runtime_cancel(&protocol, active_command); command_progress(NULL, active_command, METER_COMMAND_FAILED); }
    else if (sent && completed == sent)
        command_progress(NULL, active_command, METER_COMMAND_TX_COMPLETED);
}
static void publish_protocol(void)
{
    meter_debug_lock();
    config.public_diagnostics->data.runtime = protocol_diag.data.runtime;
    memcpy(config.public_diagnostics->data.can, protocol_diag.data.can, sizeof(protocol_diag.data.can));
    config.public_diagnostics->data.pdo = protocol_diag.data.pdo;
    config.public_diagnostics->data.sdo = protocol_diag.data.sdo;
    meter_debug_unlock();
    copy_traces(&protocol_diag, &protocol_trace_cursor);
}
static void periodic_poll(uint32_t generation, meter_mode_policy_t policy, uint32_t now)
{
    uint64_t before = tx_protocol.revision;
    uint32_t old_generation = tx_protocol.generation;
    (void)rt_mutex_take(&tx_publication_lock, RT_WAITING_FOREVER);
    bool copied = meter_tx_copy(&tx_protocol, METER_BOARD_TX_VALUES, &tx_published);
    (void)rt_mutex_release(&tx_publication_lock);
    if (copied && (before != tx_protocol.revision || old_generation != tx_protocol.generation)) tx_acquired_ms = now;
    for (size_t i = 0u; i < config.product->periodic_count; ++i)
    {
        meter_periodic_state_t *state = &periodic_state[i];
        const meter_periodic_frame_t *definition = &config.product->periodic[i];
        periodic_slot_t *slot = &periodic_slots[i];
        lock_state();
        if (slot->result_ready)
        { (void)meter_periodic_complete(state, definition, &slot->result); slot->result_ready = false; }
        if (slot->ready && (slot->message.generation != generation ||
            (definition->backlog == METER_TX_REPLACE_PENDING && meter_time_reached(now, state->deadline.next_ms))))
        {
            meter_periodic_result_t cancelled = {.generation = slot->message.generation, .ticket = slot->message.ticket};
            (void)meter_periodic_complete(state, definition, &cancelled);
            slot->ready = false; ++slot->replaced;
        }
        bool available = !slot->ready && !slot->inflight && !slot->result_ready;
        unlock_state();
        meter_periodic_message_t message;
        bool allowed = available && (definition->critical ? policy.critical_tx : policy.ordinary_tx);
        if (meter_periodic_prepare(state, definition, copied ? &tx_protocol : NULL, allowed, now, &message))
        {
            message.entry = i; message.acquired_ms = tx_acquired_ms;
            lock_state();
            if (shared.generation == generation && shared.state == METER_EXEC_RUNNING && !shared.stop_requested &&
                meter_periodic_admit(state, definition, &message))
            {
                if (slot->first_message.generation != message.generation || slot->first_message.revision != message.revision || !slot->first_message.ticket)
                { slot->first_message = message; slot->first_result = (meter_periodic_result_t){0}; }
                slot->message = message; slot->ready = true;
                uint32_t late = now - message.deadline_ms;
                if (late > slot->scheduler_max_ms) slot->scheduler_max_ms = late;
            }
            bool wake = slot->ready;
            unlock_state();
            if (wake) (void)rt_sem_release(&tx_owner[message.frame.bus].wake);
        }
    }
}
static void protocol_entry(void *arg)
{
    (void)arg;
    const meter_product_t *p = config.product;
    meter_rtthread_board_port_t board = meter_board_port(&protocol_diag);
    bool enabled[METER_BUS_COUNT] = {false};
    for (size_t i = 0u; i < p->routes->count; ++i) enabled[p->routes->entries[i].bus] = true;
    for (size_t i = 0u; i < p->periodic_count; ++i) enabled[p->periodic[i].frame.bus] = true;
#ifdef METER_ENABLE_CAN_UPDATE
    if (p->update) enabled[METER_BUS_CAN0] = true;
#endif
    bool opened = true;
    for (unsigned bus = 0u; bus < METER_BUS_COUNT; ++bus)
        if (enabled[bus] && !board.can_open(board.context, (meter_bus_role_t)bus)) opened = false;
    lock_state(); shared.protocol_ready = opened; shared.protocol_failed = !opened; unlock_state();
    (void)rt_sem_release(&app_event);
    uint32_t previous_generation = 0u;
    while (opened)
    {
        lock_state();
        meter_execution_state_t state = shared.state;
        meter_mode_policy_t policy = shared.policy;
        uint32_t generation = shared.generation;
        meter_diag_increment(&shared.protocol_runs);
        unlock_state();
        if (state == METER_EXEC_STOPPING) { command_poll(meter_board_now_ms(), generation, false); break; }
        if (state != METER_EXEC_RUNNING)
        { (void)rt_sem_take(&rx_event, rt_tick_from_millisecond(10)); continue; }
        uint32_t now = meter_board_now_ms();
        if (generation != previous_generation)
        {
            (void)meter_runtime_session(&protocol, generation);
            previous_generation = generation;
            for (size_t i = 0u; i < p->periodic_count; ++i)
                (void)meter_periodic_reset(&periodic_state[i], &p->periodic[i], generation, now);
        }
        (void)rt_sem_control(&rx_event, RT_IPC_CMD_RESET, RT_NULL);
        meter_can_frame_t frame;
        size_t count = 0u;
        while (count < 64u && meter_board_can_raw_read(NULL, &frame))
        {
            ++count;
#ifdef METER_ENABLE_CAN_UPDATE
            if (meter_board_update_frame(&frame))
            { meter_diagnostics_can(&protocol_diag, frame.bus, METER_CAN_RX, frame.timestamp_ms); continue; }
#endif
            if (policy.telemetry)
            {
                if (meter_runtime_push(&protocol, &frame)) (void)meter_runtime_poll(&protocol, 1u);
            }
            else
            {
                meter_diagnostics_can(&protocol_diag, frame.bus, METER_CAN_RX, frame.timestamp_ms);
                lock_state(); meter_diag_increment(&shared.suppressed); unlock_state();
            }
        }
        now = meter_board_now_ms();
        command_poll(now, generation, policy.commands);
        if (policy.telemetry) (void)meter_runtime_process(&protocol, now);
        periodic_poll(generation, policy, now);
#ifdef METER_ENABLE_CAN_UPDATE
        meter_board_update_protocol();
#endif
        lock_state();
        shared.missed = 0u; shared.max_late_ms = 0u;
        for (size_t i = 0u; i < p->periodic_count; ++i)
        { shared.missed += periodic_state[i].deadline.missed; if (periodic_state[i].deadline.late_ms > shared.max_late_ms) shared.max_late_ms = periodic_state[i].deadline.late_ms; }
        for (unsigned bus = 0u; bus < METER_BUS_COUNT; ++bus)
        {
            protocol_diag.data.can[bus].tx = shared.tx_ok[bus];
            protocol_diag.data.can[bus].tx_error = shared.tx_error[bus];
            protocol_diag.data.can[bus].last_tx_ms = shared.last_tx[bus];
            protocol_diag.data.can[bus].tx_seen = shared.tx_ok[bus] != 0u;
        }
        unlock_state();
        meter_board_can_diagnostics(&protocol_diag);
        publish_protocol();
        /* 高负载每 64 帧有界让出；空闲由 RX 或协议时间期限唤醒。 */
        if (count == 64u) rt_thread_mdelay(1);
        else (void)rt_sem_take(&rx_event, rt_tick_from_millisecond(
#ifdef METER_ENABLE_CAN_UPDATE
            1
#else
            5
#endif
        ));
    }
    meter_runtime_connection(&protocol, false);
    publish_protocol();
    lock_state(); shared.protocol_done = true; unlock_state();
    (void)rt_sem_release(&app_event);
}
static void set_mode(meter_mode_t mode)
{
    if (!meter_execution_mode(&execution, mode)) return;
    app_policy = config.product->mode_policy ? config.product->mode_policy(mode) : meter_execution_policy(mode);
    lock_state();
    shared.mode = execution.mode; shared.generation = execution.generation; shared.policy = app_policy;
    unlock_state();
    if (config.product->app_reset) config.product->app_reset(execution.generation);
    if (config.core->snapshot.generation != execution.generation)
        meter_core_connection(config.core, false, execution.generation);
    meter_core_connection(config.core, mode != METER_MODE_SHUTDOWN, execution.generation);
    (void)rt_sem_release(&rx_event);
}
bool meter_execution_settings_allowed(void)
{
    if (!initialized) return false;
    lock_state();
    bool allowed = shared.state == METER_EXEC_RUNNING && shared.policy.settings && !shared.stop_requested;
    unlock_state();
    return allowed;
}
static void apply_actions(uint32_t now)
{
    action_message_t message;
    if (rt_mq_recv(&actions, &message, sizeof(message), 0) != RT_EOK) return;
    const meter_product_t *p = config.product;
    bool allowed = meter_execution_settings_allowed() && message.generation == execution.generation &&
        meter_board_nvm_ready() && p->auth->local_settings &&
        (message.action.kind != METER_ACTION_PARAMETER || p->capabilities->parameter_write);
    bool applied = false;
    if (allowed)
    {
        applied = message.action.kind == METER_ACTION_PRODUCT
            ? (p->local_action && p->local_action(&config.core->snapshot, &message.action, now))
            : meter_core_action(config.core, &message.action);
    }
    if (applied) (void)meter_board_nvm_changed(now);
    meter_request_result_t result = {.id = {.session = message.generation, .serial = message.serial},
        .code = applied ? METER_RESULT_APPLIED : METER_RESULT_REJECTED, .revision = config.core->snapshot.revision};
    /* 单个信用对应一个结果槽；结果领取之前不接受下一个请求。 */
    if (rt_mq_send(&action_results, &result, sizeof(result)) != RT_EOK)
        (void)meter_execution_transition(&execution, METER_EXEC_FAILED);
}
static void publish_app(uint32_t now)
{
    if (config.product->tx_sample && config.product->tx_sample(&config.core->snapshot, now, tx_samples, config.product->tx_value_count))
    {
        (void)rt_mutex_take(&tx_publication_lock, RT_WAITING_FOREVER);
        bool copied_tx = meter_tx_publish(&tx_published, METER_BOARD_TX_VALUES, tx_samples, config.product->tx_value_count,
                                         execution.generation, now);
        (void)rt_mutex_release(&tx_publication_lock);
        if (!copied_tx) { lock_state(); meter_diag_increment(&shared.batch_rejected); unlock_state(); }
    }
    meter_diag_snapshot_t view;
    (void)meter_diagnostics_snapshot(&app_diag, now, &view);
    meter_debug_lock();
    if (meter_snapshot_copy(&debug_snapshot, &config.diagnostic_storage, &config.core->snapshot) == METER_SNAPSHOT_COPIED)
        config.public_diagnostics->domain = &debug_snapshot;
    config.public_diagnostics->data.domain = view.domain;
    config.public_diagnostics->data.storage = view.storage;
    meter_debug_unlock();
    copy_traces(&app_diag, &app_trace_cursor);
    (void)rt_mutex_take(&view_lock, RT_WAITING_FOREVER);
    bool copied = meter_snapshot_copy(&published_snapshot, &config.published_storage, &config.core->snapshot) == METER_SNAPSHOT_COPIED;
    shared.view_ready = copied;
    shared.normal_ui = app_policy.normal_ui;
#ifdef METER_ENABLE_CAN_UPDATE
    meter_board_update_view(&shared.update);
#endif
    (void)rt_mutex_release(&view_lock);
}
static meter_request_admission_t app_command(void *context, const meter_command_t *command, uint32_t timeout, meter_request_id_t *id)
{ (void)context; return meter_execution_command(command, timeout, id); }
static bool app_result(void *context, meter_request_id_t id, meter_command_stage_t *stage, bool acknowledge)
{ (void)context; return meter_execution_command_result(id, stage, acknowledge); }
static const meter_command_port_t app_commands = {.submit = app_command, .result = app_result};
/* App owns shutdown progress. Never release UI before durability and all worker acknowledgements.
 * A timeout records wait duration only; it does not cancel I/O, reclaim stacks or permit restart. */
static bool app_shutdown_progress(uint32_t now, uint32_t stop_at, uint64_t *durable)
{
    lock_state();
    shared.stop_wait_ms = now - stop_at;
    unlock_state();
    if (!*durable && meter_board_nvm_ready())
        *durable = meter_board_nvm_flush();
    bool nvm_done = meter_board_nvm_stopped() || !config.product->storage ||
                    !config.product->storage->enabled || (*durable && meter_board_nvm_barrier(*durable));
    if (nvm_done)
        meter_board_nvm_stop();
    lock_state();
    bool done = shared.protocol_done;
    for (unsigned i = 0u; i < METER_BUS_COUNT; ++i)
        done = done && shared.tx_done[i];
    unlock_state();
#ifdef METER_ENABLE_CAN_UPDATE
    done = done && meter_board_update_stopped();
#endif
    if (done && nvm_done && meter_board_nvm_stopped())
    {
        lock_state();
        bool releasing = shared.ui_release, ui_done = shared.ui_done;
        unlock_state();
        if (!releasing)
        {
            meter_board_can_close(&protocol_diag);
            publish_protocol();
            lock_state();
            shared.ui_release = true;
            unlock_state();
        }
        if (ui_done)
        {
            (void)meter_execution_transition(&execution, METER_EXEC_STOPPED);
            lock_state();
            shared.state = execution.state;
            unlock_state();
            return true;
        }
    }
    return false;
}
static void app_entry(void *arg)
{
    (void)arg;
    bool ok = true;
    if (config.product->storage && config.product->storage->enabled) ok = meter_board_nvm_start(config.core);
    /* 启动时先收敛异步读取，禁止用默认速率先开 CAN 再切换。停止请求仍可打断等待。 */
    while (ok && !meter_board_nvm_ready())
    {
        lock_state();
        bool stop = shared.stop_requested;
        unlock_state();
        if (stop) { ok = false; break; }
        meter_board_nvm_poll(meter_board_now_ms(), &app_diag.data.storage);
        if (!meter_board_nvm_ready())
            (void)rt_sem_take(&app_event, rt_tick_from_millisecond(5));
    }
    if (ok) ok = meter_board_settings_boot(&config.core->snapshot);
#ifdef METER_ENABLE_CAN_UPDATE
    if (ok && config.product->update) ok = meter_board_update_start();
#endif
    if (ok) ok = meter_execution_transition(&execution, METER_EXEC_READY);
    for (unsigned i = 0u; i < METER_BUS_COUNT; ++i)
    {
        bool started = ok && rt_thread_startup(&tx_owner[i].thread) == RT_EOK;
        lock_state(); shared.tx_done[i] = !started; unlock_state();
        ok = ok && started;
    }
    bool protocol_started = ok && rt_thread_startup(&protocol_thread) == RT_EOK;
    lock_state(); shared.protocol_done = !protocol_started; unlock_state();
    ok = ok && protocol_started;
    if (!ok)
    {
        (void)meter_execution_transition(&execution, METER_EXEC_FAILED);
        lock_state(); shared.state = execution.state; shared.stop_requested = true; unlock_state();
    }
    uint64_t durable = 0u;
    bool stopping = false;
    uint32_t last_overload = 0u, overload_at = 0u, stop_at = 0u;
    bool overloaded = false;
    for (;;)
    {
        (void)rt_sem_control(&app_event, RT_IPC_CMD_RESET, RT_NULL);
        uint32_t now = meter_board_now_ms();
        lock_state();
        bool ready = shared.protocol_ready, failed = shared.protocol_failed, stop = shared.stop_requested;
        bool ui_healthy = shared.ui_healthy;
        meter_diag_increment(&shared.app_runs);
        unlock_state();
        if (execution.state == METER_EXEC_READY && ready)
        {
            (void)meter_execution_transition(&execution, METER_EXEC_RUNNING);
            set_mode(METER_MODE_NORMAL);
            lock_state(); shared.state = execution.state; unlock_state();
        }
        if (failed && execution.state == METER_EXEC_READY)
        {
            (void)meter_execution_transition(&execution, METER_EXEC_FAILED);
            stop = true;
        }
        if (stop && !stopping)
        {
            stopping = true; stop_at = now;
            set_mode(METER_MODE_SHUTDOWN);
            (void)meter_execution_transition(&execution, METER_EXEC_STOPPING);
            lock_state(); shared.state = execution.state; unlock_state();
            (void)rt_sem_release(&rx_event);
            for (unsigned i = 0u; i < METER_BUS_COUNT; ++i) (void)rt_sem_release(&tx_owner[i].wake);
#ifdef METER_ENABLE_CAN_UPDATE
            meter_board_update_stop();
#endif
        }
        meter_diagnostics_time(&app_diag, now);
        meter_board_nvm_poll(now, &app_diag.data.storage);
        if (!stopping && execution.state == METER_EXEC_RUNNING)
        {
            bool maintenance = false;
#ifdef METER_ENABLE_CAN_UPDATE
            meter_board_update_poll(config.core, ui_healthy);
            maintenance = meter_board_update_maintenance() || meter_board_update_exclusive();
#else
            (void)ui_healthy;
#endif
            lock_state();
            uint32_t overload = shared.batch_full + shared.event_full + shared.tx_full;
            unlock_state();
            if (overload != last_overload) { overloaded = true; overload_at = now; last_overload = overload; }
            if (overloaded && now - overload_at >= 1000u) overloaded = false;
            meter_mode_t mode = maintenance ? METER_MODE_UPDATE_MAINTENANCE :
                overloaded ? METER_MODE_DEGRADED : METER_MODE_NORMAL;
            if (execution.mode != mode) set_mode(mode);
        }
        batch_message_t message;
        for (size_t i = 0u; i < BATCH_DEPTH && rt_mq_recv(&batches, &message, sizeof(message), 0) == RT_EOK; ++i)
        {
            meter_update_batch_t batch = {.generation = message.generation, .sequence = message.sequence,
                .received_ms = message.received_ms, .bus = message.bus, .source = message.source,
                .updates = message.updates, .count = message.count};
            if (stopping || message.generation != execution.generation)
            { lock_state(); meter_diag_increment(&shared.batch_stale); unlock_state(); }
            else if (!meter_core_apply_batch(config.core, &batch))
            { lock_state(); meter_diag_increment(&shared.batch_rejected); unlock_state(); }
        }
        event_message_t event;
        for (unsigned i = 0u; i < 16u && rt_mq_recv(&events, &event, sizeof(event), 0) == RT_EOK; ++i)
            if (!stopping && event.generation == execution.generation && config.product->on_event)
                config.product->on_event(&event.event);
        if (!stopping && app_policy.commands && config.product->app_run)
            config.product->app_run(now, &app_commands);
        apply_actions(now);
        if (!stopping && meter_board_nvm_ready())
            (void)meter_board_backlight_apply(config.core->snapshot.brightness, now);
        meter_core_tick(config.core, now);
        const uint32_t before_product = config.core->snapshot.revision;
        if (!stopping && config.product->app_tick)
            config.product->app_tick(&config.core->snapshot, now);
        if (config.product->evaluate) config.product->evaluate(&config.core->snapshot);
        /* 只观察编码后的设置；信号刷新不会产生介质写入，实际 I/O 仍由 NVM worker 完成。 */
        if (!stopping && config.product->storage && config.product->storage->enabled &&
            meter_board_nvm_ready() && config.core->snapshot.revision != before_product)
            (void)meter_board_nvm_changed(now);
        publish_app(now);
        if (stopping && app_shutdown_progress(now, stop_at, &durable))
        {
            return;
        }
        (void)rt_sem_take(&app_event, rt_tick_from_millisecond(5));
    }
}
bool meter_execution_start(const meter_execution_config_t *c)
{
    if (initialized || !c || !c->core || !c->product || !c->public_diagnostics ||
        c->product->periodic_count > METER_BOARD_PERIODIC_SLOTS || c->product->tx_value_count > METER_BOARD_TX_VALUES ||
        (c->product->tx_value_count && !c->product->tx_sample) || (c->product->periodic_count && !c->product->periodic)) return false;
    config = *c;
    if (meter_snapshot_copy(&published_snapshot, &config.published_storage, &c->core->snapshot) != METER_SNAPSHOT_COPIED ||
        meter_snapshot_copy(&debug_snapshot, &config.diagnostic_storage, &c->core->snapshot) != METER_SNAPSHOT_COPIED) return false;
    /* 两个发布区也必须互相独立，MSH 与 UI 各自持有不同短锁。 */
    if (meter_snapshot_copy(&debug_snapshot, &config.diagnostic_storage, &published_snapshot) != METER_SNAPSHOT_COPIED) return false;
    for (size_t i = 0u; i < c->product->periodic_count; ++i)
        if (!meter_periodic_valid(&c->product->periodic[i], c->product->tx_value_count) ||
            (c->product->periodic[i].encode && !c->product->tx_sample)) return false;
    meter_execution_init(&execution);
    meter_diagnostics_init(&app_diag); meter_diagnostics_init(&protocol_diag);
    meter_core_bind_diagnostics(c->core, &app_diag);
    if (rt_mutex_init(&tx_publication_lock, "exec_pub", RT_IPC_FLAG_PRIO) != RT_EOK ||
        rt_mutex_init(&state_lock, "exec_st", RT_IPC_FLAG_PRIO) != RT_EOK ||
        rt_mutex_init(&view_lock, "exec_ui", RT_IPC_FLAG_PRIO) != RT_EOK ||
        rt_sem_init(&rx_event, "can_rx", 0, RT_IPC_FLAG_FIFO) != RT_EOK ||
        rt_sem_init(&app_event, "app_rx", 0, RT_IPC_FLAG_FIFO) != RT_EOK ||
        rt_sem_init(&action_credit, "ui_req", 1, RT_IPC_FLAG_FIFO) != RT_EOK) return false;
    if (rt_mq_init(&commands, "app_cmd", command_pool, sizeof(command_message_t), sizeof(command_pool), RT_IPC_FLAG_FIFO) != RT_EOK) return false;
    if (rt_mq_init(&batches, "sem_batch", batch_pool, sizeof(batch_message_t), sizeof(batch_pool), RT_IPC_FLAG_FIFO) != RT_EOK ||
        rt_mq_init(&actions, "ui_action", action_pool, sizeof(action_message_t), sizeof(action_pool), RT_IPC_FLAG_FIFO) != RT_EOK ||
        rt_mq_init(&action_results, "ui_result", action_result_pool, sizeof(meter_request_result_t), sizeof(action_result_pool), RT_IPC_FLAG_FIFO) != RT_EOK ||
        rt_mq_init(&events, "proto_evt", event_pool, sizeof(event_message_t), sizeof(event_pool), RT_IPC_FLAG_FIFO) != RT_EOK) return false;
    if (!meter_batch_builder_init(&builder, staging, BATCH_VALUES) ||
        !meter_runtime_init(&protocol, c->product, meter_batch_builder_add, &builder) ||
        !meter_runtime_bind_batches(&protocol, &builder, submit_batch, NULL)) return false;
    static const meter_can_tx_port_t tx_port = {.send = send_protocol};
    meter_runtime_bind_services(&protocol, submit_event, NULL, &tx_port);
    meter_runtime_bind_diagnostics(&protocol, &protocol_diag);
    for (unsigned i = 0u; i < METER_BUS_COUNT; ++i)
    {
        tx_owner_t *t = &tx_owner[i]; t->bus = i;
        if (rt_sem_init(&t->wake, i ? "tx1_wake" : "tx0_wake", 0, RT_IPC_FLAG_FIFO) != RT_EOK) return false;
        if (rt_mq_init(&t->ordinary, i ? "can1_tx" : "can0_tx", t->ordinary_pool, sizeof(tx_message_t), sizeof(t->ordinary_pool), RT_IPC_FLAG_FIFO) != RT_EOK ||
            rt_mq_init(&t->urgent, i ? "can1_bulk" : "can0_bulk", t->urgent_pool, sizeof(tx_message_t), sizeof(t->urgent_pool), RT_IPC_FLAG_FIFO) != RT_EOK ||
            rt_thread_init(&t->thread, i ? "meter_tx1" : "meter_tx0", tx_entry, t, t->stack, sizeof(t->stack), 18, 5) != RT_EOK) return false;
    }
    if (rt_thread_init(&protocol_thread, "meter_proto", protocol_entry, NULL, protocol_stack, sizeof(protocol_stack), 19, 5) != RT_EOK ||
        rt_thread_init(&app_thread, "meter_app", app_entry, NULL, app_stack, sizeof(app_stack), 20, 5) != RT_EOK) return false;
    meter_board_can_wake(&rx_event);
    shared.generation = execution.generation;
    initialized = true;
    if (rt_thread_startup(&app_thread) != RT_EOK)
    {
        shared.state = METER_EXEC_FAILED;
        return false;
    }
    return true;
}
bool meter_execution_action(void *context, const meter_action_t *action)
{
    (void)context;
    if (!initialized || !action || rt_sem_take(&action_credit, 0) != RT_EOK) return false;
    lock_state();
    action_message_t message = {.generation = shared.generation, .serial = shared.action_serial, .action = *action};
    bool allowed = shared.action_serial < UINT64_MAX && shared.state == METER_EXEC_RUNNING && shared.policy.settings && !shared.stop_requested;
    if (allowed) message.serial = ++shared.action_serial;
    unlock_state();
    if (!allowed || rt_mq_send(&actions, &message, sizeof(message)) != RT_EOK)
    { (void)rt_sem_release(&action_credit); return false; }
    (void)rt_sem_release(&app_event);
    return true;
}
bool meter_execution_action_result(meter_request_result_t *out)
{
    if (!initialized || !out || rt_mq_recv(&action_results, out, sizeof(*out), 0) != RT_EOK) return false;
    (void)rt_sem_release(&action_credit); return true;
}
bool meter_execution_present(meter_snapshot_t *snapshot, const meter_core_storage_t *storage, meter_update_view_t *update, bool *normal_ui)
{
    if (!initialized || !snapshot || !storage || !update || !normal_ui) return false;
    (void)rt_mutex_take(&view_lock, RT_WAITING_FOREVER);
    bool ok = shared.view_ready && meter_snapshot_copy(snapshot, storage, &published_snapshot) == METER_SNAPSHOT_COPIED;
    *update = shared.update;
    *normal_ui = shared.normal_ui;
    (void)rt_mutex_release(&view_lock);
    return ok;
}
void meter_execution_ui_diagnostics(const meter_diagnostics_t *d)
{
    if (!initialized || !d) return;
    meter_debug_lock();
    config.public_diagnostics->data.ui = d->data.ui;
    config.public_diagnostics->data.touch = d->data.touch;
    meter_debug_unlock();
    lock_state();
    shared.ui_healthy = d->data.ui.flush_count != 0u;
    /* UI owner 每轮发布一次；这个计数是看门狗监督的 UI 心跳，不借用刷屏计数。 */
    ++shared.ui_ticks;
    unlock_state();
}
void meter_execution_stop(void)
{
    if (!initialized) return;
    lock_state(); shared.stop_requested = true; unlock_state();
    (void)rt_sem_release(&app_event);
}
bool meter_execution_stopped(void)
{
    if (!initialized) return false;
    lock_state(); bool stopped = shared.state == METER_EXEC_STOPPED; unlock_state(); return stopped;
}
bool meter_execution_liveness(meter_execution_liveness_t *out)
{
    if (!out) return false;
    *out = (meter_execution_liveness_t){0};
    if (!initialized) return false;
    lock_state();
    out->protocol_runs = shared.protocol_runs;
    out->app_runs = shared.app_runs;
    out->ui_ticks = shared.ui_ticks;
    bool stopping = shared.state == METER_EXEC_STOPPING || shared.state == METER_EXEC_STOPPED;
    out->stopping = stopping;
    out->started = true;
    out->owners_expected = !stopping;
    unlock_state();
    return true;
}
bool meter_execution_ui_shutdown_requested(void)
{
    if (!initialized) return false;
    lock_state(); bool release = shared.ui_release; unlock_state(); return release;
}
void meter_execution_ui_stopped(void)
{
    if (!initialized) return;
    lock_state();
    if (shared.ui_release) shared.ui_done = true;
    unlock_state();
    (void)rt_sem_release(&app_event);
}
static int meter_exec(int argc, char **argv)
{
    if (!initialized) return -RT_ERROR;
    if (argc == 2 && strcmp(argv[1], "stop") == 0) { meter_execution_stop(); rt_kprintf("runtime stop=QUEUED\n"); return RT_EOK; }
    lock_state();
    meter_execution_state_t state = shared.state; meter_mode_t mode = shared.mode;
    uint32_t generation = shared.generation, high = shared.batch_high, full = shared.batch_full,
        stale = shared.batch_stale, rejected = shared.batch_rejected, suppressed = shared.suppressed,
        missed = shared.missed, late = shared.max_late_ms, tx_full = shared.tx_full,
        tx0 = shared.tx_high[0], tx1 = shared.tx_high[1], runs = shared.protocol_runs, stop_wait = shared.stop_wait_ms;
    unlock_state();
    rt_kprintf("execution state=%u mode=%u generation=%u protocol_runs=%u\n", state, mode, generation, runs);
    rt_kprintf("batch high=%u capacity=%u full=%u stale=%u rejected=%u suppressed=%u\n", high, BATCH_DEPTH, full, stale, rejected, suppressed);
    rt_kprintf("tx high0=%u high1=%u full=%u periodic missed=%u late_max_ms=%u\n", tx0, tx1, tx_full, missed, late);
    for (size_t i = 0u; i < config.product->periodic_count; ++i)
    {
        lock_state(); periodic_slot_t slot = periodic_slots[i]; unlock_state();
        /* SDK 格式缓冲有限；每行在所有 uint32 最大十进制值下仍小于 120 字节。 */
        rt_kprintf("periodic entry=%u gen=%u revision=%u ticket=%u\n", (unsigned)i,
            slot.message.generation, (unsigned)slot.message.revision, (unsigned)slot.message.ticket);
        rt_kprintf("periodic entry=%u acquired_ms=%u published_ms=%u deadline_ms=%u\n", (unsigned)i,
            slot.message.acquired_ms, slot.message.published_ms, slot.message.deadline_ms);
        rt_kprintf("periodic entry=%u queued_ms=%u start_ms=%u complete_ms=%u\n", (unsigned)i,
            slot.message.queued_ms, slot.result.started_ms, slot.result.completed_ms);
        rt_kprintf("periodic entry=%u result_gen=%u result_ticket=%u ready=%u inflight=%u\n", (unsigned)i,
            slot.result.generation, (unsigned)slot.result.ticket, slot.ready, slot.inflight);
        rt_kprintf("periodic entry=%u replaced=%u expired=%u scheduler_max_ms=%u\n", (unsigned)i,
            slot.replaced, slot.expired, slot.scheduler_max_ms);
        rt_kprintf("periodic entry=%u queue_max_ms=%u driver_max_ms=%u\n", (unsigned)i,
            slot.queue_max_ms, slot.driver_max_ms);
        rt_kprintf("periodic_first entry=%u gen=%u revision=%u acquired_ms=%u\n", (unsigned)i,
            slot.first_message.generation, (unsigned)slot.first_message.revision, slot.first_message.acquired_ms);
        rt_kprintf("periodic_first entry=%u deadline_ms=%u queued_ms=%u start_ms=%u\n", (unsigned)i,
            slot.first_message.deadline_ms, slot.first_message.queued_ms, slot.first_result.started_ms);
        rt_kprintf("periodic_first entry=%u complete_ms=%u success=%u data0=%u data1=%u data3=%u\n", (unsigned)i,
            slot.first_result.completed_ms, slot.first_result.success, slot.first_message.frame.data[0],
            slot.first_message.frame.data[1], slot.first_message.frame.data[3]);
    }
    rt_kprintf("shutdown wait_ms=%u overdue=%u; blocked idle workers are healthy\n", stop_wait,
        state == METER_EXEC_STOPPING && stop_wait >= 5000u);
    return RT_EOK;
}
MSH_CMD_EXPORT(meter_exec, Runtime owners mode queues and cooperative stop);

meter_request_admission_t meter_execution_command(const meter_command_t *command, uint32_t timeout_ms, meter_request_id_t *id)
{
    if (!initialized || !command || !id || !timeout_ms || timeout_ms >= METER_TIME_HALF_RANGE) return METER_REQUEST_INVALID;
    meter_command_stage_t completion = config.product->command_completion ?
        config.product->command_completion(command) : METER_COMMAND_REMOTE_CONFIRMED;
    if (completion != METER_COMMAND_APPLIED && completion != METER_COMMAND_TX_COMPLETED &&
        completion != METER_COMMAND_REMOTE_CONFIRMED) return METER_REQUEST_INVALID;
    lock_state();
    if (shared.command_reserved || shared.state != METER_EXEC_RUNNING || !shared.policy.commands || shared.stop_requested)
    { unlock_state(); return METER_REQUEST_BUSY; }
    if (shared.command_serial == UINT64_MAX) { unlock_state(); return METER_REQUEST_EXHAUSTED; }
    meter_request_id_t next = {.session = shared.generation, .serial = ++shared.command_serial};
    command_message_t message = {.id = next, .command = *command, .timeout_ms = timeout_ms};
    shared.command_reserved = true;
    shared.command_id = next;
    shared.command_stage = METER_COMMAND_QUEUED;
    shared.command_completion = completion;
    shared.command_sent = 0u;
    memset(shared.command_done, 0, sizeof(shared.command_done));
    memset(shared.command_failed, 0, sizeof(shared.command_failed));
    if (rt_mq_send(&commands, &message, sizeof(message)) != RT_EOK)
    { shared.command_reserved = false; unlock_state(); return METER_REQUEST_BUSY; }
    *id = next;
    unlock_state();
    (void)rt_sem_release(&rx_event);
    return METER_REQUEST_QUEUED;
}
bool meter_execution_command_result(meter_request_id_t id, meter_command_stage_t *stage, bool acknowledge)
{
    if (!initialized || !stage) return false;
    lock_state();
    bool valid = shared.command_reserved && meter_request_id_equal(shared.command_id, id);
    if (valid)
    {
        *stage = shared.command_stage;
        if (acknowledge && command_terminal(*stage, shared.command_completion)) shared.command_reserved = false;
        else if (acknowledge) valid = false;
    }
    unlock_state(); return valid;
}
