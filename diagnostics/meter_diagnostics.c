#include "diagnostics/meter_diagnostics.h"
#include <string.h>
void meter_diagnostics_init(meter_diagnostics_t *d)
{
    if (d)
        memset(d, 0, sizeof(*d));
}
void meter_diagnostics_time(meter_diagnostics_t *d, uint32_t now)
{
    if (d)
        d->data.uptime_ms = now;
}
bool meter_diagnostics_snapshot(const meter_diagnostics_t *d, uint32_t now, meter_diag_snapshot_t *out)
{
    if (!d || !out)
        return false;
    *out = d->data;
    out->uptime_ms = now;
    out->trace_capacity = METER_TRACE_CAPACITY;
    out->trace_count = d->trace.count;
    out->trace_write_index = d->trace.write_index;
    out->trace_overwritten = d->trace.overwritten;
    if (d->domain && d->domain->catalog)
    {
        const meter_snapshot_t *s = d->domain;
        out->domain.available = true;
        out->domain.connected = s->connected;
        out->domain.signals = s->catalog->signal_count;
        out->domain.parameters = s->catalog->parameter_count;
        out->domain.faults = s->catalog->fault_count;
        out->domain.generation = s->generation;
        out->domain.revision = s->revision;
    }
    return true;
}
bool meter_diagnostics_signal(const meter_diagnostics_t *d, const char *key, uint32_t now,
                              meter_diag_signal_t *out)
{
    if (!d || !key || !out || !d->domain || !d->domain->catalog)
        return false;
    for (size_t i = 0; i < d->domain->catalog->signal_count; ++i)
    {
        const meter_signal_def_t *def = &d->domain->catalog->signals[i];
        if (def->key && strcmp(def->key, key) == 0)
        {
            *out = (meter_diag_signal_t){def->id,
                                         def->key,
                                         def->unit,
                                         d->domain->signals[i],
                                         now - d->domain->signals[i].timestamp_ms,
                                         def->stale_ms};
            return true;
        }
    }
    return false;
}
void meter_diagnostics_can(meter_diagnostics_t *d, meter_bus_role_t bus, meter_diag_can_event_t event,
                           uint32_t now)
{
    if (!d || (unsigned)bus >= METER_BUS_COUNT)
        return;
    meter_diag_can_t *c = &d->data.can[bus];
    c->available = true;
    switch (event)
    {
    case METER_CAN_RX:
        meter_diag_increment(&c->rx);
        c->rx_seen = true;
        c->last_rx_ms = now;
        break;
    case METER_CAN_TX:
        meter_diag_increment(&c->tx);
        c->tx_seen = true;
        c->last_tx_ms = now;
        break;
    case METER_CAN_RX_DROP:
        meter_diag_increment(&c->rx_drop);
        METER_DIAG_TRACE(d, METER_TRACE_CAN, CAN_RX_DROP, now, bus, 0);
        break;
    case METER_CAN_TX_BUSY:
        meter_diag_increment(&c->tx_busy);
        METER_DIAG_TRACE(d, METER_TRACE_CAN, CAN_TX_BUSY, now, bus, 0);
        break;
    case METER_CAN_RX_ERROR:
        meter_diag_increment(&c->rx_error);
        METER_DIAG_TRACE(d, METER_TRACE_CAN, CAN_ERROR, now, bus, 0);
        break;
    case METER_CAN_TX_ERROR:
        meter_diag_increment(&c->tx_error);
        METER_DIAG_TRACE(d, METER_TRACE_CAN, CAN_ERROR, now, bus, 1);
        break;
    }
}
