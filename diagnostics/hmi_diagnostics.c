#include "contracts/hmi_time.h"
#include "diagnostics/hmi_diagnostics.h"
#include <string.h>
void hmi_diagnostics_init(hmi_diagnostics_t *d)
{
    if (d)
        memset(d, 0, sizeof(*d));
}
void hmi_diagnostics_time(hmi_diagnostics_t *d, uint32_t now)
{
    if (d)
        d->data.uptime_ms = now;
}
bool hmi_diagnostics_snapshot(const hmi_diagnostics_t *d, uint32_t now, hmi_diag_snapshot_t *out)
{
    if (!d || !out)
        return false;
    *out = d->data;
    out->uptime_ms = now;
    out->trace_capacity = HMI_TRACE_CAPACITY;
    out->trace_count = d->trace.count;
    out->trace_write_index = d->trace.write_index;
    out->trace_overwritten = d->trace.overwritten;
    if (d->domain && d->domain->catalog)
    {
        const hmi_snapshot_t *s = d->domain;
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
bool hmi_diagnostics_signal(const hmi_diagnostics_t *d, const char *key, uint32_t now,
                              hmi_diag_signal_t *out)
{
    if (!d || !key || !out || !d->domain || !d->domain->catalog)
        return false;
    for (size_t i = 0; i < d->domain->catalog->signal_count; ++i)
    {
        const hmi_signal_def_t *def = &d->domain->catalog->signals[i];
        if (def->key && strcmp(def->key, key) == 0)
        {
            *out = (hmi_diag_signal_t){def->id,
                                         def->key,
                                         def->unit,
                                         d->domain->signals[i],
                                         /* 比 now 还晚的打戳（同一轮里后读的帧）按刚更新处理 */
                                         hmi_time_reached(now, d->domain->signals[i].timestamp_ms)
                                             ? now - d->domain->signals[i].timestamp_ms
                                             : 0u,
                                         def->stale_ms};
            return true;
        }
    }
    return false;
}
void hmi_diagnostics_can(hmi_diagnostics_t *d, hmi_bus_role_t bus, hmi_diag_can_event_t event,
                           uint32_t now)
{
    if (!d || (unsigned)bus >= HMI_BUS_COUNT)
        return;
    hmi_diag_can_t *c = &d->data.can[bus];
    c->available = true;
    switch (event)
    {
    case HMI_CAN_RX:
        hmi_diag_increment(&c->rx);
        c->rx_seen = true;
        c->last_rx_ms = now;
        break;
    case HMI_CAN_TX:
        hmi_diag_increment(&c->tx);
        c->tx_seen = true;
        c->last_tx_ms = now;
        break;
    case HMI_CAN_RX_DROP:
        hmi_diag_increment(&c->rx_drop);
        HMI_DIAG_TRACE(d, HMI_TRACE_CAN, CAN_RX_DROP, now, bus, 0);
        break;
    case HMI_CAN_TX_BUSY:
        hmi_diag_increment(&c->tx_busy);
        HMI_DIAG_TRACE(d, HMI_TRACE_CAN, CAN_TX_BUSY, now, bus, 0);
        break;
    case HMI_CAN_RX_ERROR:
        hmi_diag_increment(&c->rx_error);
        HMI_DIAG_TRACE(d, HMI_TRACE_CAN, CAN_ERROR, now, bus, 0);
        break;
    case HMI_CAN_TX_ERROR:
        hmi_diag_increment(&c->tx_error);
        HMI_DIAG_TRACE(d, HMI_TRACE_CAN, CAN_ERROR, now, bus, 1);
        break;
    }
}
