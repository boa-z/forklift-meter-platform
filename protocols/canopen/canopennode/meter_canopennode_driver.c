#include "meter_canopennode_driver.h"
#include "diagnostics/meter_diagnostics.h"
#include <string.h>

CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t *m, uint16_t i, uint16_t id, uint16_t mask, bool_t rtr,
                                    void *object, void (*callback)(void *, void *))
{
    if (!m || i >= m->rxSize || rtr || !callback)
        return CO_ERROR_ILLEGAL_ARGUMENT;
    m->rxArray[i] = (CO_CANrx_t){id, mask, object, callback};
    return CO_ERROR_NO;
}
CO_CANtx_t *CO_CANtxBufferInit(CO_CANmodule_t *m, uint16_t i, uint16_t id, bool_t rtr, uint8_t size,
                               bool_t sync)
{
    if (!m || i >= m->txSize || rtr || size > 8)
        return NULL;
    CO_CANtx_t *b = &m->txArray[i];
    if (b->bufferFull)
        return NULL;
    memset(b, 0, sizeof(*b));
    b->ident = id;
    b->DLC = size;
    b->syncFlag = sync;
    return b;
}
static bool send_buffer(CO_CANmodule_t *m, CO_CANtx_t *b)
{
    meter_can_frame_t f = {.bus = m->bus, .id = b->ident, .size = b->DLC, .timestamp_ms = m->now_ms};
    memcpy(f.data, b->data, b->DLC);
    if (!m->port.send || !m->port.send(m->port.context, &f))
    {
        meter_diagnostics_can(m->diag, m->bus, METER_CAN_TX_BUSY, m->now_ms);
        METER_DIAG_INC(m->diag, sdo, tx_busy);
        return false;
    }
    meter_diagnostics_can(m->diag, m->bus, METER_CAN_TX, m->now_ms);
    b->bufferFull = false;
    return true;
}
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *m, CO_CANtx_t *b)
{
    if (!m || !b)
        return CO_ERROR_ILLEGAL_ARGUMENT;
    if (b->bufferFull)
        return CO_ERROR_TX_OVERFLOW;
    /* 端口忙时保留上游原始缓冲，CANopenNode 将阻止覆盖并等待 flush。 */
    b->bufferFull = true;
    (void)send_buffer(m, b);
    return CO_ERROR_NO;
}
bool meter_co_flush(CO_CANmodule_t *m)
{
    if (!m)
        return false;
    for (uint16_t i = 0; i < m->txSize; ++i)
        if (m->txArray[i].bufferFull && !send_buffer(m, &m->txArray[i]))
            return false;
    return true;
}
bool meter_co_receive(CO_CANmodule_t *m, const meter_can_frame_t *f)
{
    if (!m || !f || f->bus != m->bus || f->remote || f->extended || f->size > 8 || f->id > 0x7ff)
        return false;
    for (uint16_t i = 0; i < m->rxSize; ++i)
    {
        CO_CANrx_t *rx = &m->rxArray[i];
        if (rx->CANrx_callback && ((f->id ^ rx->ident) & rx->mask) == 0)
        {
            rx->CANrx_callback(rx->object, (void *)f);
            return true;
        }
    }
    return false;
}
