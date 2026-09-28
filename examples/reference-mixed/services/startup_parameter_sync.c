#include "startup_parameter_sync.h"
#include "canopen/mixed_commands.h"
#include "contracts/meter_time.h"
#include <string.h>
mixed_startup_sync_t mixed_startup;
void mixed_startup_reset(uint32_t generation)
{
    meter_request_id_t pending = mixed_startup.request;
    memset(&mixed_startup, 0, sizeof(mixed_startup));
    mixed_startup.generation = generation;
    mixed_startup.request = pending;
}
void mixed_startup_event(const meter_protocol_event_t *e)
{
    if (!e) return;
    if (e->id == MIXED_EVENT_PDO_READY) mixed_startup.data_ready = true;
    else if (e->id == MIXED_EVENT_PDO_TIMEOUT)
    {
        mixed_startup.data_ready = false;
        mixed_startup.phase = MIXED_SYNC_WAIT_DATA;
        memset(mixed_startup.received, 0, sizeof(mixed_startup.received));
    }
    else if (e->id == MIXED_EVENT_PARAMETER_A || e->id == MIXED_EVENT_PARAMETER_B)
    {
        unsigned item = e->id == MIXED_EVENT_PARAMETER_A ? 0u : 1u;
        mixed_startup.parameters[item] = (uint16_t)e->argument;
        mixed_startup.received[item] = true;
    }
    else if (e->id == MIXED_EVENT_SDO_FAILED) mixed_startup.error = e->argument;
}
void mixed_startup_run(uint32_t now, const meter_command_port_t *port)
{
    mixed_startup_sync_t *s = &mixed_startup;
    if (!port || !port->submit || !port->result) return;
    if (s->request.serial)
    {
        meter_command_stage_t stage;
        bool available = port->result(port->context, s->request, &stage, false);
        if (available && s->request.session != s->generation &&
            (stage == METER_COMMAND_REMOTE_CONFIRMED || stage == METER_COMMAND_FAILED || stage == METER_COMMAND_CANCELLED))
        {
            (void)port->result(port->context, s->request, &stage, true);
            s->request = (meter_request_id_t){0};
            return;
        }
        if (available && (stage == METER_COMMAND_FAILED || stage == METER_COMMAND_CANCELLED))
        {
            (void)port->result(port->context, s->request, &stage, true);
            s->request = (meter_request_id_t){0};
            if (s->data_ready) s->phase = MIXED_SYNC_FAILED;
            if (!s->error) s->error = 1u;
            return;
        }
        unsigned item = s->phase == MIXED_SYNC_READ_B ? 1u : 0u;
        if (available && stage == METER_COMMAND_REMOTE_CONFIRMED && s->received[item])
        {
            (void)port->result(port->context, s->request, &stage, true);
            s->request = (meter_request_id_t){0};
            if (!s->data_ready) return;
            if (item == 1u) { s->phase = MIXED_SYNC_READY; return; }
            s->phase = MIXED_SYNC_READ_B;
        }
        else
        {
            if (meter_time_reached(now, s->deadline))
            {
                /* 无语义结果不得误报 READY；即使事件队列满，仍回收终态信用。 */
                if (available && stage == METER_COMMAND_REMOTE_CONFIRMED)
                { (void)port->result(port->context, s->request, &stage, true); s->request = (meter_request_id_t){0}; }
                s->phase = MIXED_SYNC_FAILED;
                if (!s->error) s->error = 2u;
            }
            return;
        }
    }
    if (!s->data_ready || s->phase == MIXED_SYNC_READY || s->phase == MIXED_SYNC_FAILED) return;
    unsigned item = s->phase == MIXED_SYNC_READ_B ? 1u : 0u;
    meter_command_t command = {.id = item ? MIXED_CMD_READ_ACCEL : MIXED_CMD_READ_MAX_SPEED};
    if (port->submit(port->context, &command, 2000u, &s->request) == METER_REQUEST_QUEUED)
    {
        s->phase = item ? MIXED_SYNC_READ_B : MIXED_SYNC_READ_A;
        s->received[item] = false;
        s->deadline = now + 2000u;
    }
}
