/* 真实 owner 实现的确定性 IPC 测试；不把替身计为真实线程/HIL。 */
#include "platform/rtthread/meter_execution_port.c"
#include <assert.h>
static unsigned changes;
/* 本测试不启动调度器；意外进入硬件或线程入口立即失败。 */
int rt_mutex_init(struct rt_mutex *m, const char *n, int f) { (void)n; (void)f; m->depth = 0u; return 0; }
int rt_sem_init(struct rt_semaphore *s, const char *n, unsigned v, int f) { (void)n; (void)f; s->value = v; return 0; }
int rt_sem_control(struct rt_semaphore *s, int c, void *a) { (void)c; (void)a; s->value = 0u; return 0; }
int rt_mq_init(struct rt_messagequeue *q, const char *n, void *p, size_t size, size_t bytes, int f)
{ (void)n; (void)p; (void)f; memset(q, 0, sizeof(*q)); q->size = size; q->limit = (unsigned)(bytes / (RT_ALIGN(size, RT_ALIGN_SIZE) + sizeof(void *))); return 0; }
int rt_thread_init(struct rt_thread *t, const char *n, void (*e)(void *), void *a, void *s, size_t z, int p, int k)
{ (void)t; (void)n; (void)e; (void)a; (void)s; (void)z; (void)p; (void)k; assert(0); return -1; }
int rt_thread_startup(struct rt_thread *t) { (void)t; assert(0); return -1; }
void rt_thread_mdelay(int ms) { (void)ms; assert(0); }
int rt_tick_from_millisecond(int ms) { return ms; }
uint32_t rt_tick_get_millisecond(void) { return 0u; }
int rt_kprintf(const char *fmt, ...) { (void)fmt; return 0; }
void meter_debug_lock(void) { }
void meter_debug_unlock(void) { }
meter_rtthread_board_port_t meter_board_port(meter_diagnostics_t *d)
{ (void)d; assert(0); return (meter_rtthread_board_port_t){0}; }
void meter_board_can_wake(struct rt_semaphore *s) { (void)s; assert(0); }
bool meter_board_can_raw_read(void *c, meter_can_frame_t *f) { (void)c; (void)f; assert(0); return false; }
bool meter_board_can_send(const meter_can_frame_t *f) { (void)f; assert(0); return false; }
void meter_board_can_diagnostics(meter_diagnostics_t *d) { (void)d; assert(0); }
void meter_board_can_close(meter_diagnostics_t *d) { (void)d; assert(0); }
bool meter_board_nvm_start(meter_core_t *c) { (void)c; assert(0); return false; }
void meter_board_nvm_poll(uint32_t t, meter_diag_storage_t *s) { (void)t; (void)s; assert(0); }
uint64_t meter_board_nvm_flush(void) { assert(0); return 0u; }
bool meter_board_nvm_barrier(uint64_t t) { (void)t; assert(0); return false; }
void meter_board_nvm_stop(void) { assert(0); }
bool meter_board_nvm_stopped(void) { assert(0); return false; }
int rt_mutex_take(struct rt_mutex *m, int timeout) { (void)timeout; assert(m->depth == 0u); ++m->depth; return 0; }
int rt_mutex_release(struct rt_mutex *m) { assert(m->depth == 1u); --m->depth; return 0; }
int rt_sem_take(struct rt_semaphore *s, int timeout) { (void)timeout; if (!s->value) return -1; --s->value; return 0; }
int rt_sem_release(struct rt_semaphore *s) { ++s->value; return 0; }
rt_base_t rt_hw_interrupt_disable(void) { return 0; }
void rt_hw_interrupt_enable(rt_base_t level) { (void)level; }
int rt_mq_send(struct rt_messagequeue *q, const void *data, size_t size)
{
    assert(size <= 1024u);
    if (q->entry == q->limit) return -1;
    q->size = size; memcpy(q->data[q->tail], data, size); q->tail = (q->tail + 1u) % 64u; ++q->entry; return 0;
}
int rt_mq_recv(struct rt_messagequeue *q, void *data, size_t size, int timeout)
{
    (void)timeout;
    if (!q->entry) return -1;
    assert(size == q->size); memcpy(data, q->data[q->head], size); q->head = (q->head + 1u) % 64u; --q->entry; return 0;
}
bool meter_board_nvm_ready(void) { return true; }
bool meter_board_nvm_changed(uint32_t now) { (void)now; ++changes; return true; }
static bool decode(const meter_can_frame_t *f, const meter_protocol_services_t *s) { (void)f; (void)s; return true; }
static bool on_frame(void *ctx, const meter_can_frame_t *f, const meter_protocol_services_t *s) { (void)ctx; return decode(f,s); }
static bool command(void *ctx, const meter_command_t *c, const meter_protocol_services_t *s)
{
    (void)ctx; (void)c;
    meter_can_frame_t f = {.id = 0x123u, .bus = METER_BUS_CAN0, .size = 1u};
    return s->tx->send(s->tx->context, &f);
}
static bool route(void *ctx, const meter_command_t *c, meter_frame_route_owner_t *owner)
{ (void)ctx; (void)c; *owner = 1u; return true; }
static meter_command_stage_t completion_for(const meter_command_t *c)
{ return c->id == 2u ? METER_COMMAND_TX_COMPLETED : METER_COMMAND_REMOTE_CONFIRMED; }
int main(void)
{
    static const meter_signal_def_t signal[] = {{1u, "a", "", 0u}};
    static const meter_catalog_t catalog = {.signals = signal, .signal_count = 1u};
    static const meter_auth_profile_t auth = {.local_settings = true};
    static const meter_capability_profile_t caps = {.language_selection = true};
    static const meter_protocol_adapter_t adapter = {.on_frame = on_frame, .command = command};
    static const meter_protocol_binding_t binding[] = {{.owner = 1u, .adapter = &adapter}};
    static const meter_protocol_profile_t profile = {binding, 1u};
    static const meter_frame_route_t entry[] = {{METER_BUS_CAN0, 0x100u, false, 1u}};
    static const meter_route_profile_t routes = {entry, 1u};
    static const meter_product_t product = {.catalog = &catalog, .auth = &auth, .capabilities = &caps,
        .protocols = &profile, .routes = &routes, .command_route = route, .command_completion = completion_for};
    static meter_core_t core; static meter_value_t values[1];
    meter_core_storage_t store = {.signals = values, .signal_capacity = 1u};
    assert(meter_core_init(&core, &catalog, &store));
    config.core = &core; config.product = &product; initialized = true;
    meter_execution_init(&execution);
    assert(meter_execution_transition(&execution, METER_EXEC_READY));
    assert(meter_execution_transition(&execution, METER_EXEC_RUNNING));
    set_mode(METER_MODE_NORMAL); shared.state = METER_EXEC_RUNNING;
    actions.limit = 1u; action_results.limit = 1u; action_credit.value = 1u;
    meter_action_t action = {.kind = METER_ACTION_BRIGHTNESS, .value = 30.0f};
    assert(meter_execution_action(NULL, &action));
    assert(core.snapshot.brightness == 80u && !meter_execution_action(NULL, &action));
    apply_actions(1u);
    meter_request_result_t result;
    assert(meter_execution_action_result(&result) && result.code == METER_RESULT_APPLIED);
    assert(core.snapshot.brightness == 30u && changes == 1u);
    uint64_t first = result.id.serial;
    assert(meter_execution_action(NULL, &action)); set_mode(METER_MODE_UPDATE_MAINTENANCE);
    apply_actions(2u); assert(meter_execution_action_result(&result));
    assert(result.code == METER_RESULT_REJECTED && result.id.serial > first && changes == 1u);
    assert(!meter_execution_action(NULL, &action));
    batches.limit = 1u;
    meter_update_t update = {1u, {5.0f, 0u, METER_VALUE_VALID, 1u}};
    meter_update_batch_t batch = {.generation = 1u, .sequence = 1u, .source = 1u, .updates = &update, .count = 1u};
    assert(submit_batch(NULL, &batch) == METER_BATCH_QUEUED);
    update.value.value = 99.0f;
    assert(submit_batch(NULL, &batch) == METER_BATCH_IPC_FULL && shared.batch_full == 1u);
    batch_message_t received; assert(rt_mq_recv(&batches, &received, sizeof(received), 0) == 0);
    assert(received.updates[0].value.value == 5.0f);
    set_mode(METER_MODE_NORMAL); commands.limit = 1u;
    assert(meter_runtime_init(&protocol, &product, meter_batch_builder_add, &builder));
    meter_runtime_connection(&protocol, true); protocol.generation = shared.generation;
    static const meter_can_tx_port_t tx = {.send = send_protocol};
    meter_runtime_bind_services(&protocol, NULL, NULL, &tx);
    tx_owner[0].ordinary.limit = 16u;
    meter_command_t request = {.id = 1u}; meter_request_id_t id;
    assert(meter_execution_command(&request, 100u, &id) == METER_REQUEST_QUEUED);
    meter_command_stage_t stage;
    assert(meter_execution_command_result(id, &stage, false) && stage == METER_COMMAND_QUEUED);
    command_poll(0u, shared.generation, true);
    assert(meter_execution_command_result(id, &stage, false) && stage == METER_COMMAND_APPLIED);
    assert(shared.command_sent == 1u && !meter_execution_command_result(id, &stage, true));
    shared.command_done[0] = 1u; command_poll(1u, shared.generation, true);
    assert(meter_execution_command_result(id, &stage, false) && stage == METER_COMMAND_TX_COMPLETED);
    command_progress(NULL, id, METER_COMMAND_REMOTE_CONFIRMED);
    assert(meter_execution_command_result(id, &stage, true) && stage == METER_COMMAND_REMOTE_CONFIRMED);
    assert(meter_execution_command(&request, 10u, &id) == METER_REQUEST_QUEUED);
    command_poll(2u, shared.generation, true); command_poll(12u, shared.generation, true);
    assert(meter_execution_command_result(id, &stage, true) && stage == METER_COMMAND_FAILED);
    assert(meter_execution_command(&request, 10u, &id) == METER_REQUEST_QUEUED);
    command_poll(14u, shared.generation + 1u, false);
    assert(meter_execution_command_result(id, &stage, true) && stage == METER_COMMAND_CANCELLED);
    request.id = 2u;
    assert(meter_execution_command(&request, 10u, &id) == METER_REQUEST_QUEUED);
    command_poll(20u, shared.generation, true);
    shared.command_done[0] = 1u; command_poll(21u, shared.generation, true);
    command_poll(100u, shared.generation, true);
    assert(meter_execution_command_result(id, &stage, true) && stage == METER_COMMAND_TX_COMPLETED);
    meter_diagnostics_t session_diag; meter_diagnostics_init(&session_diag);
    meter_runtime_bind_diagnostics(&protocol, &session_diag);
    assert(meter_runtime_session(&protocol, 123u));
    assert(protocol.generation == 123u && session_diag.data.runtime.generation == 123u);
    assert(protocol.count == 0u && protocol.connected);
    /* 使用真实短锁 mailbox 路径：替代、跳过、驱动结果、代次隔离、队列公平性。 */
    meter_periodic_frame_t definitions[2] = {
        {.frame = {.id = 0x100u, .bus = METER_BUS_CAN0, .size = 1u, .data = {1u}}, .period_ms = 10u,
         .critical = true, .backlog = METER_TX_REPLACE_PENDING},
        {.frame = {.id = 0x101u, .bus = METER_BUS_CAN0, .size = 1u, .data = {2u}}, .period_ms = 20u}
    };
    meter_product_t periodic_product = product;
    periodic_product.periodic = definitions; periodic_product.periodic_count = 2u;
    config.product = &periodic_product;
    meter_mode_policy_t normal = meter_execution_policy(METER_MODE_NORMAL);
    for (size_t i = 0u; i < 2u; ++i) assert(meter_periodic_reset(&periodic_state[i], &definitions[i], shared.generation, 0u));
    periodic_poll(shared.generation, normal, 10u);
    assert(periodic_slots[0].ready && periodic_slots[0].message.deadline_ms == 10u);
    periodic_poll(shared.generation, normal, 20u);
    assert(periodic_slots[0].replaced == 1u && periodic_slots[0].message.deadline_ms == 20u && periodic_slots[1].ready);
    meter_periodic_message_t pm;
    assert(periodic_take(&tx_owner[0], &pm) && pm.entry == 0u);
    assert(!meter_periodic_sendable(&pm, shared.generation, 30u));
    periodic_poll(shared.generation, normal, 30u);
    assert(periodic_slots[0].inflight && !periodic_slots[0].ready);
    periodic_result(&pm, 30u, 30u, false, true);
    assert(periodic_state[0].busy);
    periodic_poll(shared.generation, normal, 31u);
    assert(!periodic_state[0].busy && periodic_state[0].failed == 2u);
    assert(periodic_take(&tx_owner[0], &pm) && pm.entry == 1u);
    uint32_t next_generation = shared.generation + 1u;
    assert(meter_periodic_reset(&periodic_state[1], &definitions[1], next_generation, 31u));
    periodic_result(&pm, 32u, 33u, true, false);
    periodic_poll(next_generation, normal, 34u);
    assert(periodic_state[1].completed == 0u);
    /* maintenance 的普通槽不能重新生成，保留 critical；复位后下一整周期开始。 */
    shared.generation = next_generation;
    for (size_t i = 0u; i < 2u; ++i) assert(meter_periodic_reset(&periodic_state[i], &definitions[i], next_generation, 40u));
    meter_mode_policy_t maintenance = {.critical_tx = true};
    periodic_poll(next_generation, maintenance, 60u);
    assert(periodic_slots[0].ready && !periodic_slots[1].ready);
    config.product = &product;
    shared.state = METER_EXEC_STOPPING;
    assert(!meter_execution_ui_shutdown_requested());
    meter_execution_ui_stopped(); assert(!shared.ui_done);
    shared.ui_release = true;
    assert(meter_execution_ui_shutdown_requested() && !meter_execution_stopped());
    meter_execution_ui_stopped(); assert(shared.ui_done && !meter_execution_stopped());
    meter_can_frame_t blocked = {.bus = METER_BUS_CAN0};
    assert(!meter_execution_can_submit(&blocked, false));
    return 0;
}

uint32_t meter_board_now_ms(void) { return rt_tick_get_millisecond(); }
