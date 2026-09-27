#include "sdo_peer.h"
#include <string.h>
static bool server_send(void *ctx, const meter_can_frame_t *frame)
{
    sdo_peer_t *p = ctx;
    if (p->out_count >= 256)
        return false;
    p->outgoing[p->out_count++] = *frame;
    return true;
}
bool sdo_peer_init(sdo_peer_t *p)
{
    memset(p, 0, sizeof(*p));
    p->parameter_a = 250;
    p->parameter_b = 30;
    p->param_count = 2;
    p->params[0] = (OD_obj_record_t){&p->param_count, 0, ODA_SDO_R, 1};
    p->params[1] = (OD_obj_record_t){&p->parameter_a, 1, ODA_SDO_RW, 2};
    p->params[2] = (OD_obj_record_t){&p->parameter_b, 2, ODA_SDO_RW, 2};
    p->entries[0] = (OD_entry_t){0x2000, 3, ODT_REC, p->params, NULL};
    const unsigned sizes[] = {1, 2, 4, 17, 128};
    for (unsigned i = 0; i < 5; ++i)
    {
        for (unsigned j = 0; j < sizes[i]; ++j)
            p->data[i][j] = (uint8_t)(j + 11 + i);
        p->vars[i] = (OD_obj_var_t){p->data[i], ODA_SDO_RW, sizes[i]};
        p->entries[i + 1] = (OD_entry_t){(uint16_t)(0x2100 + i), 1, ODT_VAR, &p->vars[i], NULL};
    }
    p->od = (OD_t){6, p->entries};
    p->can = (CO_CANmodule_t){.rxArray = &p->rx,
                              .txArray = &p->tx,
                              .rxSize = 1,
                              .txSize = 1,
                              .CANnormal = true,
                              .bus = METER_BUS_CAN1,
                              .port = {server_send, p}};
    return CO_SDOserver_init(&p->server, &p->od, NULL, 12, 1000, &p->can, 0, &p->can, 0, NULL) == CO_ERROR_NO;
}
bool sdo_peer_send(void *ctx, const meter_can_frame_t *f)
{
    sdo_peer_t *p = ctx;
    if (p->busy)
    {
        --p->busy;
        return false;
    }
    if (p->sent_count >= 1024 || p->in_count >= 256)
        return false;
    p->sent[p->sent_count++] = *f;
    if (!p->drop)
        p->incoming[p->in_count++] = *f;
    return true;
}
void sdo_peer_step(sdo_peer_t *p)
{
    for (unsigned i = 0; i < p->in_count; ++i)
    {
        (void)meter_co_receive(&p->can, &p->incoming[i]);
        (void)CO_SDOserver_process(&p->server, true, 0, NULL);
    }
    p->in_count = 0;
    (void)CO_SDOserver_process(&p->server, true, 1000, NULL);
}
