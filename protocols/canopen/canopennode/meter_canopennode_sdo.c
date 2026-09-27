#include "meter_canopennode_sdo.h"
#include <limits.h>
#include <string.h>

static void diag_sync(meter_sdo_channel_t *c)
{
    if (!c || !c->diag)
        return;
    meter_diag_sdo_t *d = &c->diag->data.sdo;
    d->available = true;
    d->bus = c->can.bus;
    d->depth = c->count;
    d->active_request = 0;
    if (c->active >= 0)
    {
        const meter_sdo_request_t *r = &c->requests[c->active];
        d->active_request = r->request_id;
        d->node = r->node_id;
        d->index = r->index;
        d->subindex = r->subindex;
        d->operation = (uint8_t)r->operation;
        d->attempt = c->results[c->active].attempts;
        d->state = METER_SDO_PENDING;
    }
}
void meter_sdo_bind_diagnostics(meter_sdo_channel_t *c, meter_diagnostics_t *d)
{
    if (c)
    {
        c->diag = d;
        c->can.diag = d;
        diag_sync(c);
    }
}
bool meter_sdo_init(meter_sdo_channel_t *c, meter_bus_role_t bus)
{
    if (!c || bus >= METER_BUS_COUNT)
        return false;
    memset(c, 0, sizeof(*c));
    c->active = -1;
    c->can = (CO_CANmodule_t){
        .rxArray = &c->rx, .txArray = &c->tx, .rxSize = 1, .txSize = 1, .CANnormal = true, .bus = bus};
    c->initialized = meter_co_sdo_init_od(&c->client, &c->can) == CO_ERROR_NO;
    return c->initialized;
}
bool meter_sdo_submit(meter_sdo_channel_t *c, const meter_sdo_request_t *r)
{
    if (!c || !c->initialized || !r || !r->request_id || !r->node_id || r->node_id > 127 || !r->timeout_ms ||
        !r->size || r->size > METER_SDO_PAYLOAD_SIZE ||
        (r->operation != METER_SDO_READ && r->operation != METER_SDO_WRITE))
        return false;
    int slot = -1;
    for (unsigned i = 0; i < METER_SDO_CAPACITY; ++i)
    {
        if (c->results[i].status == METER_SDO_IDLE)
            slot = (int)i;
        else if (c->results[i].request_id == r->request_id)
            return false;
    }
    if (slot < 0)
    {
        METER_DIAG_INC(c->diag, sdo, queue_full);
        return false;
    }
    c->requests[slot] = *r;
    c->results[slot] = (meter_sdo_result_t){.request_id = r->request_id, .status = METER_SDO_PENDING};
    c->queue[(c->head + c->count) % METER_SDO_CAPACITY] = (uint8_t)slot;
    ++c->count;
    METER_DIAG_INC(c->diag, sdo, queued);
    METER_DIAG_TRACE(c->diag, METER_TRACE_SDO, SDO_QUEUE, c->can.now_ms, r->request_id,
                     ((uint32_t)r->node_id << 24) | ((uint32_t)r->index << 8) | r->subindex);
    diag_sync(c);
    return true;
}
static bool start(meter_sdo_channel_t *c)
{
    meter_sdo_request_t *r = &c->requests[c->active];
    meter_sdo_result_t *out = &c->results[c->active];
    ++out->attempts;
    METER_DIAG_INC(c->diag, sdo, started);
    METER_DIAG_TRACE(c->diag, METER_TRACE_SDO, SDO_START, c->can.now_ms, r->request_id, out->attempts);
    out->size = 0;
    out->abort_code = 0;
    c->overflow = false;
    if (CO_SDOclient_setup(&c->client, 0x600u + r->node_id, 0x580u + r->node_id, r->node_id) < 0)
        return false;
    if (r->operation == METER_SDO_READ)
        return CO_SDOclientUploadInitiate(&c->client, r->index, r->subindex, r->timeout_ms, false) == 0;
    if (CO_SDOclientDownloadInitiate(&c->client, r->index, r->subindex, r->size, r->timeout_ms, false) != 0)
        return false;
    return CO_SDOclientDownloadBufWrite(&c->client, r->payload, r->size) == r->size;
}
static void finish(meter_sdo_channel_t *c, CO_SDO_return_t ret, CO_SDO_abortCode_t abort, uint32_t now)
{
    meter_sdo_request_t *r = &c->requests[c->active];
    meter_sdo_result_t *out = &c->results[c->active];
    CO_SDOclientClose(&c->client);
    bool timed_out = abort == CO_SDO_AB_TIMEOUT;
    if (ret == 0)
        METER_DIAG_INC(c->diag, sdo, completed);
    else if (timed_out)
        METER_DIAG_INC(c->diag, sdo, timeout);
    else
        METER_DIAG_INC(c->diag, sdo, aborted);
    if (c->diag)
    {
        if (abort)
            c->diag->data.sdo.last_abort = (uint32_t)abort;
        c->diag->data.sdo.state = ret == 0    ? METER_SDO_SUCCESS
                                  : timed_out ? METER_SDO_TIMEOUT
                                              : METER_SDO_ABORTED;
    }
    METER_DIAG_TRACE(c->diag, METER_TRACE_SDO,
                     ret == 0    ? SDO_COMPLETE
                     : timed_out ? SDO_TIMEOUT
                                 : SDO_ABORT,
                     now, r->request_id, (uint32_t)abort);
    if (ret < 0 && out->attempts <= r->retry_count && (timed_out || r->retry_abort))
    {
        METER_DIAG_INC(c->diag, sdo, retry);
        METER_DIAG_TRACE(c->diag, METER_TRACE_SDO, SDO_RETRY, now, r->request_id, out->attempts + 1);
        c->retry_wait = true;
        c->retry_at = now + r->retry_delay_ms;
        return;
    }
    if (ret < 0)
        METER_DIAG_TRACE(c->diag, METER_TRACE_SDO, SDO_FAILED, now, r->request_id, (uint32_t)abort);
    out->status = ret == 0 ? METER_SDO_SUCCESS : (timed_out ? METER_SDO_TIMEOUT : METER_SDO_ABORTED);
    out->abort_code = ret == 0 ? 0 : (uint32_t)abort;
    c->active = -1;
}
void meter_sdo_process(meter_sdo_channel_t *c, uint32_t now, const meter_can_tx_port_t *tx)
{
    if (!c || !c->initialized)
        return;
    uint32_t delta = c->clock_set ? now - c->last_ms : 0;
    c->clock_set = true;
    c->last_ms = now;
    c->can.now_ms = now;
    c->can.port = tx ? *tx : (meter_can_tx_port_t){0};
    bool flushed = meter_co_flush(&c->can);
    if (c->active < 0 && c->count && flushed)
    {
        c->active = c->queue[c->head];
        c->head = (c->head + 1) % METER_SDO_CAPACITY;
        --c->count;
        c->retry_wait = true;
        c->retry_at = now;
    }
    if (c->active < 0)
        goto done;
    if (c->retry_wait)
    {
        if (!flushed || (int32_t)(now - c->retry_at) < 0)
            goto done;
        c->retry_wait = false;
        delta = 0;
        if (!start(c))
        {
            finish(c, CO_SDO_RT_wrongArguments, CO_SDO_AB_DEVICE_INCOMPAT, now);
            goto done;
        }
    }
    meter_sdo_request_t *r = &c->requests[c->active];
    meter_sdo_result_t *out = &c->results[c->active];
    CO_SDO_abortCode_t abort = c->overflow ? CO_SDO_AB_DATA_LONG : CO_SDO_AB_NONE;
    uint32_t us = delta > UINT32_MAX / 1000u ? UINT32_MAX : delta * 1000u;
    /* 计时只由上游维护；限制单次增量，避免上游累加器溢出。 */
    if (us > (uint32_t)r->timeout_ms * 1000u)
        us = (uint32_t)r->timeout_ms * 1000u;
    CO_SDO_return_t ret;
    size_t transferred = 0, indicated = 0;
    if (r->operation == METER_SDO_READ)
    {
        ret = CO_SDOclientUpload(&c->client, us, c->overflow, &abort, &indicated, &transferred, NULL);
        out->size += CO_SDOclientUploadBufRead(&c->client, out->payload + out->size, r->size - out->size);
        c->overflow = indicated > r->size || transferred > r->size;
        if (ret == 0 && c->overflow)
        {
            ret = CO_SDO_RT_endedWithClientAbort;
            abort = CO_SDO_AB_DATA_LONG;
        }
    }
    else
    {
        ret = CO_SDOclientDownload(&c->client, us, false, false, &abort, &transferred, NULL);
        out->size = transferred;
    }
    /* 已超时的尚未发出请求被明确取消，为上游下一次发送 abort 释放缓冲。 */
    if (c->client.state == CO_SDO_ST_ABORT && c->tx.bufferFull)
        c->tx.bufferFull = false;
    if (ret <= 0)
        finish(c, ret, abort, now);
done:
    diag_sync(c);
    c->can.port = (meter_can_tx_port_t){0};
}
bool meter_sdo_receive(meter_sdo_channel_t *c, const meter_can_frame_t *f)
{
    if (!c || !c->initialized || !f || f->size != 8 || c->active < 0 || c->retry_wait || c->tx.bufferFull)
        return false;
    return meter_co_receive(&c->can, f);
}
bool meter_sdo_result(const meter_sdo_channel_t *c, uint32_t id, meter_sdo_result_t *out)
{
    if (!c || !out)
        return false;
    for (unsigned i = 0; i < METER_SDO_CAPACITY; ++i)
        if (c->results[i].status != METER_SDO_IDLE && c->results[i].request_id == id)
        {
            *out = c->results[i];
            return true;
        }
    return false;
}
bool meter_sdo_take(meter_sdo_channel_t *c, uint32_t id, meter_sdo_result_t *out)
{
    if (!meter_sdo_result(c, id, out) || out->status == METER_SDO_PENDING)
        return false;
    for (unsigned i = 0; i < METER_SDO_CAPACITY; ++i)
        if (c->results[i].request_id == id)
        {
            c->results[i] = (meter_sdo_result_t){0};
            break;
        }
    return true;
}
void meter_sdo_reset(meter_sdo_channel_t *c)
{
    if (!c || !c->initialized)
        return;
    METER_DIAG_INC(c->diag, sdo, reset);
    METER_DIAG_TRACE(c->diag, METER_TRACE_SDO, SDO_RESET, c->can.now_ms,
                     c->active >= 0 ? c->requests[c->active].request_id : 0, c->count);
    CO_SDOclientClose(&c->client);
    CO_FLAG_CLEAR(c->client.CANrxNew);
    c->tx.bufferFull = false;
    c->active = -1;
    c->head = c->count = 0;
    c->retry_wait = false;
    c->clock_set = false;
    for (unsigned i = 0; i < METER_SDO_CAPACITY; ++i)
        if (c->results[i].status == METER_SDO_PENDING)
        {
            c->results[i].status = METER_SDO_ABORTED;
            c->results[i].abort_code = CO_SDO_AB_DATA_DEV_STATE;
        }
    if (c->diag)
        c->diag->data.sdo.state = METER_SDO_IDLE;
    diag_sync(c);
}
