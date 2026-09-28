#include "canopen/mixed_canopen.h"
#include "catalog/catalog.h"
#include "protocols/canopen/meter_canopen_cobid.h"
#include <math.h>
#include <string.h>
const meter_canopen_profile_t mixed_canopen_profile = {
    MIXED_CANOPEN_NODE_ID, true, true, true, false, false, false, false, false};
mixed_canopen_state_t mixed_canopen_state;
void mixed_canopen_init(mixed_canopen_state_t *s)
{
    if (!s)
        return;
    memset(s, 0, sizeof(*s));
    s->node_id = MIXED_CANOPEN_NODE_ID;
    s->rpdo1_cob = meter_canopen_cob(METER_CANOPEN_COB_RPDO1_BASE, s->node_id);
    s->tpdo1_cob = meter_canopen_cob(METER_CANOPEN_COB_TPDO1_BASE, s->node_id);
    s->sdo_resp_cob = meter_canopen_cob(METER_CANOPEN_COB_SDO_RESP_BASE, s->node_id);
    (void)meter_sdo_init(&s->sdo, METER_BUS_CAN1);
    (void)meter_deadline_arm(&s->tpdo_deadline, 0u, MIXED_CANOPEN_TPDO_PERIOD_MS);
    s->next_request_id = 3;
    s->tpdo_armed = true;
}
static void emit(const meter_protocol_services_t *s, uint16_t id, uint32_t now, uint32_t arg)
{
    if (s->event)
    {
        meter_protocol_event_t event = {id, now, arg};
        (void)s->event(s->event_context, &event);
    }
}
static void bind_diag(mixed_canopen_state_t *s, const meter_protocol_services_t *services)
{
    if (!s->node_id) mixed_canopen_init(s);
    meter_sdo_bind_diagnostics(&s->sdo, services->diagnostics);
    if (services->diagnostics)
    {
        meter_diag_pdo_t *d = &services->diagnostics->data.pdo;
        d->available = true;
        d->bindings = 2;
        d->timeout_ms = MIXED_CANOPEN_PDO_TIMEOUT_MS;
        d->seen = s->rpdo_seen;
        d->stale_state = s->timeout_reported;
        d->last_rx_ms = s->last_rpdo_ms;
    }
}
static bool on_frame(void *ctx, const meter_can_frame_t *frame, const meter_protocol_services_t *services)
{
    mixed_canopen_state_t *st = ctx;
    if (!st || !frame || !services || frame->bus != METER_BUS_CAN1 || frame->extended || frame->remote)
        return false;
    bind_diag(st, services);
    if (frame->id == st->sdo_resp_cob)
        return meter_sdo_receive(&st->sdo, frame);
    if (!services->update)
        return false;
    if (frame->id == st->rpdo1_cob)
    {
        if (frame->size != 8)
        {
            METER_DIAG_INC(services->diagnostics, pdo, decode_error);
            METER_DIAG_TRACE(services->diagnostics, METER_TRACE_PDO, PDO_DECODE_ERROR, frame->timestamp_ms,
                             frame->id, frame->size);
            return false;
        }
        METER_DIAG_INC(services->diagnostics, pdo, rx);
        /* RPDO1: byte0-1 speed uint16 0.1 m/s, byte2-3 torque int16 0.5 Nm，小端。 */
        uint16_t raw_speed = (uint16_t)frame->data[0] | ((uint16_t)frame->data[1] << 8);
        int16_t raw_torque = (int16_t)((uint16_t)frame->data[2] | ((uint16_t)frame->data[3] << 8));
        float speed = raw_speed * 0.1f;
        float torque = raw_torque * 0.5f;
        bool speed_bad = speed < 0.0f || speed > 50.0f;
        bool torque_bad = torque < -100.0f || torque > 100.0f;
        meter_update_t u_speed = {MIXED_PDO_SPEED,
                                  {speed, frame->timestamp_ms,
                                   speed_bad ? METER_VALUE_ERROR : METER_VALUE_VALID, MIXED_CANOPEN_SOURCE}};
        meter_update_t u_torque = {MIXED_PDO_TORQUE,
                                   {torque, frame->timestamp_ms,
                                    torque_bad ? METER_VALUE_ERROR : METER_VALUE_VALID,
                                    MIXED_CANOPEN_SOURCE}};
        bool ok = services->update(services->update_context, &u_speed);
        ok &= services->update(services->update_context, &u_torque);
        if (speed_bad || torque_bad)
        {
            METER_DIAG_INC(services->diagnostics, pdo, decode_error);
            METER_DIAG_TRACE(services->diagnostics, METER_TRACE_PDO, PDO_DECODE_ERROR, frame->timestamp_ms,
                             frame->id, 0);
        }
        if (!speed_bad && !torque_bad)
        {
            if (st->timeout_reported)
            {
                METER_DIAG_INC(services->diagnostics, pdo, recover);
                METER_DIAG_TRACE(services->diagnostics, METER_TRACE_PDO, PDO_RECOVER, frame->timestamp_ms,
                                 frame->id, 0);
            }
            if (!st->rpdo_seen || st->timeout_reported)
                emit(services, MIXED_EVENT_PDO_READY, frame->timestamp_ms, 0u);
            st->last_rpdo_ms = frame->timestamp_ms;
            st->rpdo_seen = true;
            st->timeout_reported = false;
        }
        ++st->pdo_frames;
        bind_diag(st, services);
        return ok;
    }
    return false;
}
static void collect_commands(mixed_canopen_state_t *s, uint32_t now,
                             const meter_protocol_services_t *services)
{
    for (unsigned i = 0; i < METER_SDO_CAPACITY; ++i)
        if (s->commands[i])
        {
            meter_sdo_result_t result;
            if (meter_sdo_take(&s->sdo, s->commands[i], &result))
            {
                bool read = s->command_kinds[i] == MIXED_CMD_READ_MAX_SPEED || s->command_kinds[i] == MIXED_CMD_READ_ACCEL;
                if (read && result.status == METER_SDO_SUCCESS && result.size != 2u)
                    result.status = METER_SDO_ABORTED;
                if (read && result.status == METER_SDO_SUCCESS)
                    emit(services, s->command_kinds[i] == MIXED_CMD_READ_MAX_SPEED ? MIXED_EVENT_PARAMETER_A : MIXED_EVENT_PARAMETER_B,
                        now, (uint32_t)result.payload[0] | ((uint32_t)result.payload[1] << 8));
                if (services->progress && s->command_requests[i].serial)
                    services->progress(services->progress_context, s->command_requests[i],
                        result.status == METER_SDO_SUCCESS ? METER_COMMAND_REMOTE_CONFIRMED : METER_COMMAND_FAILED);
                s->command_requests[i] = (meter_request_id_t){0};
                emit(services,
                     result.status == METER_SDO_SUCCESS ? MIXED_EVENT_SDO_COMPLETE : MIXED_EVENT_SDO_FAILED,
                     now, result.status == METER_SDO_SUCCESS ? result.request_id : result.abort_code);
                s->commands[i] = 0;
            }
        }
}
static bool process(void *ctx, uint32_t now, const meter_protocol_services_t *services)
{
    mixed_canopen_state_t *s = ctx;
    if (!s || !services)
        return false;
    bind_diag(s, services);
    if (s->rpdo_seen && !s->timeout_reported && now - s->last_rpdo_ms >= MIXED_CANOPEN_PDO_TIMEOUT_MS)
    {
        s->timeout_reported = true;
        ++s->timeouts;
        METER_DIAG_INC(services->diagnostics, pdo, stale);
        METER_DIAG_TRACE(services->diagnostics, METER_TRACE_PDO, PDO_STALE, now, s->rpdo1_cob,
                         now - s->last_rpdo_ms);
        meter_sdo_reset(&s->sdo);
        collect_commands(s, now, services);
        emit(services, MIXED_EVENT_PDO_TIMEOUT, now, s->rpdo1_cob);
    }
    meter_sdo_process(&s->sdo, now, services->tx);
    collect_commands(s, now, services);
    if (s->tpdo_armed && meter_deadline_take(&s->tpdo_deadline, now) && services->tx &&
        services->tx->send)
    {
        meter_can_frame_t f = {.bus = METER_BUS_CAN1,
                               .id = s->tpdo1_cob,
                               .timestamp_ms = now,
                               .size = 2,
                               .data = {s->rpdo_seen && !s->timeout_reported, (uint8_t)s->timeouts}};
        if (services->tx->send(services->tx->context, &f))
        {
            s->last_tpdo_ms = now;
            METER_DIAG_INC(services->diagnostics, pdo, tx);
            meter_diagnostics_can(services->diagnostics, f.bus, METER_CAN_TX, now);
        }
        else
            meter_diagnostics_can(services->diagnostics, f.bus, METER_CAN_TX_BUSY, now);
    }
    bind_diag(s, services);
    return true;
}
static bool command(void *ctx, const meter_command_t *cmd, const meter_protocol_services_t *services)
{
    mixed_canopen_state_t *s = ctx;
    if (!s || !cmd || !services || !services->tx || !services->tx->send || !isfinite(cmd->value) ||
        cmd->value < 0 || cmd->value > 6553.5f)
        return false;
    bind_diag(s, services);
    bool read = cmd->id == MIXED_CMD_READ_MAX_SPEED || cmd->id == MIXED_CMD_READ_ACCEL;
    uint8_t sub = cmd->id == MIXED_CMD_SET_MAX_SPEED || cmd->id == MIXED_CMD_READ_MAX_SPEED ? 1 :
        cmd->id == MIXED_CMD_SET_ACCEL || cmd->id == MIXED_CMD_READ_ACCEL ? 2 : 0;
    if (!sub || s->timeout_reported)
        return false;
    unsigned slot = 0;
    while (slot < METER_SDO_CAPACITY && s->commands[slot])
        ++slot;
    if (slot == METER_SDO_CAPACITY)
        return false;
    uint16_t raw = (uint16_t)(cmd->value * 10.0f);
    meter_sdo_request_t r = {.request_id = s->next_request_id,
                             .node_id = s->node_id,
                             .index = 0x2000,
                             .subindex = sub,
                             .operation = read ? METER_SDO_READ : METER_SDO_WRITE,
                             .size = 2,
                             .payload = {(uint8_t)raw, (uint8_t)(raw >> 8)},
                             .timeout_ms = MIXED_CANOPEN_SDO_TIMEOUT_MS,
                             .retry_count = MIXED_CANOPEN_SDO_RETRIES,
                             .retry_delay_ms = 120};
    if (!meter_sdo_submit(&s->sdo, &r))
        return false;
    s->commands[slot] = r.request_id;
    s->command_kinds[slot] = cmd->id;
    s->command_requests[slot] = services->request;
    if (++s->next_request_id < 3)
        s->next_request_id = 3;
    return true;
}
static void reset(void *ctx)
{
    mixed_canopen_state_t *s = ctx;
    if (!s)
        return;
    meter_diagnostics_t *diag = s->sdo.diag;
    bool armed = s->node_id == 0u || s->tpdo_armed;
    meter_sdo_reset(&s->sdo);
    mixed_canopen_init(s);
    s->tpdo_armed = armed;
    meter_sdo_bind_diagnostics(&s->sdo, diag);
}
static void cancel(void *context, meter_request_id_t request)
{
    mixed_canopen_state_t *s = context;
    for (unsigned i = 0u; i < METER_SDO_CAPACITY; ++i)
        if (meter_request_id_equal(s->command_requests[i], request))
        {
            /* 此产品只接纳一个 App 请求；通道 reset 停止所有本机重试。 */
            meter_sdo_reset(&s->sdo);
            return;
        }
}
const meter_protocol_adapter_t mixed_canopen_adapter = {.context = &mixed_canopen_state,
    .on_frame = on_frame, .process = process, .command = command, .reset = reset, .cancel = cancel};
bool mixed_command_route(void *ctx, const meter_command_t *cmd, meter_frame_route_owner_t *owner)
{
    (void)ctx;
    if (!cmd || !owner || (cmd->id != MIXED_CMD_SET_MAX_SPEED && cmd->id != MIXED_CMD_SET_ACCEL &&
        cmd->id != MIXED_CMD_READ_MAX_SPEED && cmd->id != MIXED_CMD_READ_ACCEL))
        return false;
    *owner = 2;
    return true;
}
