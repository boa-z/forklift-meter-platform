#include "sdo_peer.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(x))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "%d: %s\n", __LINE__, #x);                                                       \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
static meter_sdo_channel_t channel;
static sdo_peer_t peer;
static uint32_t now;
static meter_diagnostics_t diag;
static meter_sdo_request_t request(uint32_t id, unsigned item, meter_sdo_operation_t operation)
{
    const unsigned sizes[] = {1, 2, 4, 17, 128};
    meter_sdo_request_t r = {.request_id = id,
                             .node_id = 12,
                             .index = (uint16_t)(0x2100 + item),
                             .operation = operation,
                             .size = sizes[item],
                             .timeout_ms = 20,
                             .retry_delay_ms = 3};
    for (size_t i = 0; i < r.size; ++i)
        r.payload[i] = (uint8_t)(200 - i);
    return r;
}
static void pump(void)
{
    meter_can_tx_port_t tx = {sdo_peer_send, &peer};
    meter_sdo_process(&channel, now++, &tx);
    sdo_peer_step(&peer);
    for (unsigned i = 0; i < peer.out_count; ++i)
        (void)meter_sdo_receive(&channel, &peer.outgoing[i]);
    peer.out_count = 0;
}
static meter_sdo_result_t run(uint32_t id)
{
    meter_sdo_result_t result = {0};
    for (unsigned i = 0; i < 1000; ++i)
    {
        pump();
        if (meter_sdo_result(&channel, id, &result) && result.status != METER_SDO_PENDING)
            break;
    }
    return result;
}
static int transfers(void)
{
    CHECK(sdo_peer_init(&peer));
    CHECK(meter_sdo_init(&channel, METER_BUS_CAN1));
    meter_diagnostics_init(&diag);
    meter_sdo_bind_diagnostics(&channel, &diag);
    now = 0;
    for (unsigned item = 0; item < 5; ++item)
    {
        meter_sdo_request_t r = request(item * 2 + 1, item, METER_SDO_READ);
        CHECK(meter_sdo_submit(&channel, &r));
        meter_sdo_result_t out = run(r.request_id);
        CHECK(out.status == METER_SDO_SUCCESS && out.size == r.size && out.attempts == 1);
        CHECK(memcmp(out.payload, peer.data[item], out.size) == 0);
        CHECK(meter_sdo_take(&channel, r.request_id, &out));
        r = request(item * 2 + 2, item, METER_SDO_WRITE);
        unsigned first = peer.sent_count;
        CHECK(meter_sdo_submit(&channel, &r));
        out = run(r.request_id);
        CHECK(out.status == METER_SDO_SUCCESS && out.size == r.size);
        CHECK(memcmp(peer.data[item], r.payload, r.size) == 0);
        if (item == 1)
            CHECK(peer.sent[first].data[0] == 0x2b);
        CHECK(meter_sdo_take(&channel, r.request_id, &out));
    }
    CHECK(diag.data.sdo.queued == 10 && diag.data.sdo.started == 10);
    CHECK(diag.data.sdo.completed == 10 && diag.data.sdo.timeout == 0);
    CHECK(diag.data.can[1].tx > 10 && diag.trace.count > 20);
    puts("expedited read/write 1/2/4; segmented read/write 17/128; standard 0x2B PASS");
    return 0;
}
static int failures(void)
{
    CHECK(sdo_peer_init(&peer));
    CHECK(meter_sdo_init(&channel, METER_BUS_CAN1));
    meter_diagnostics_init(&diag);
    meter_sdo_bind_diagnostics(&channel, &diag);
    now = 0;
    meter_sdo_request_t r = request(1, 0, METER_SDO_READ);
    r.index = 0x3333;
    CHECK(meter_sdo_submit(&channel, &r));
    meter_sdo_result_t out = run(1);
    CHECK(out.status == METER_SDO_ABORTED && out.abort_code == CO_SDO_AB_NOT_EXIST);
    CHECK(meter_sdo_take(&channel, 1, &out));
    r = request(2, 4, METER_SDO_READ);
    r.size = 4;
    CHECK(meter_sdo_submit(&channel, &r));
    out = run(2);
    CHECK(out.status == METER_SDO_ABORTED && out.abort_code == CO_SDO_AB_DATA_LONG);
    CHECK(meter_sdo_take(&channel, 2, &out));
    peer.drop = true;
    r = request(3, 0, METER_SDO_READ);
    r.retry_count = 2;
    CHECK(meter_sdo_submit(&channel, &r));
    out = run(3);
    CHECK(out.status == METER_SDO_TIMEOUT && out.abort_code == CO_SDO_AB_TIMEOUT && out.attempts == 3);
    meter_trace_entry_t history[METER_TRACE_CAPACITY];
    size_t count = meter_trace_snapshot(&diag.trace, history, METER_TRACE_CAPACITY);
    unsigned exhausted = 0;
    for (size_t i = 0; i < count; ++i)
        if (history[i].module == METER_TRACE_SDO && history[i].event == SDO_FAILED && history[i].arg0 == 3)
            exhausted++;
    CHECK(exhausted == 1);
    CHECK(diag.data.sdo.timeout == 3 && diag.data.sdo.retry == 2);
    CHECK(diag.data.sdo.aborted == 2 && diag.data.sdo.last_abort == CO_SDO_AB_TIMEOUT);
    CHECK(meter_sdo_take(&channel, 3, &out));
    /* 写入重试必须保持操作、长度、载荷，不能退化为读请求。 */
    r = request(4, 1, METER_SDO_WRITE);
    r.retry_count = 1;
    CHECK(meter_sdo_submit(&channel, &r));
    unsigned first = peer.sent_count;
    for (unsigned i = 0; i < 100 && !channel.retry_wait; ++i)
        pump();
    CHECK(channel.retry_wait);
    peer.drop = false;
    out = run(4);
    CHECK(out.status == METER_SDO_SUCCESS && out.attempts == 2);
    unsigned writes = 0;
    for (unsigned i = first; i < peer.sent_count; ++i)
        if (peer.sent[i].data[0] != 0x80)
        {
            CHECK(peer.sent[i].data[0] == 0x2b && peer.sent[i].data[4] == r.payload[0]);
            ++writes;
        }
    CHECK(writes == 2);
    CHECK(meter_sdo_take(&channel, 4, &out));
    peer.busy = 3;
    r = request(5, 3, METER_SDO_WRITE);
    CHECK(meter_sdo_submit(&channel, &r));
    first = peer.sent_count;
    pump();
    CHECK(channel.tx.bufferFull && peer.sent_count == first);
    uint8_t pending[8];
    memcpy(pending, channel.tx.data, 8);
    pump();
    CHECK(channel.tx.bufferFull && memcmp(pending, channel.tx.data, 8) == 0);
    out = run(5);
    CHECK(out.status == METER_SDO_SUCCESS && out.attempts == 1);
    CHECK(meter_sdo_take(&channel, 5, &out));
    /* 错误 COB-ID、长度、远程帧不进入上游；错误索引由上游报告。 */
    peer.drop = true;
    r = request(6, 0, METER_SDO_READ);
    CHECK(meter_sdo_submit(&channel, &r));
    pump();
    meter_can_frame_t wrong = {
        .bus = METER_BUS_CAN1, .id = 0x58d, .size = 8, .data = {0x4f, 0x00, 0x22, 0, 9, 0, 0, 0}};
    CHECK(!meter_sdo_receive(&channel, &wrong));
    wrong.id = 0x58c;
    wrong.size = 7;
    CHECK(!meter_sdo_receive(&channel, &wrong));
    wrong.size = 8;
    wrong.remote = true;
    CHECK(!meter_sdo_receive(&channel, &wrong));
    wrong.remote = false;
    CHECK(meter_sdo_receive(&channel, &wrong));
    out = run(6);
    CHECK(out.status == METER_SDO_ABORTED && out.abort_code == CO_SDO_AB_PRAM_INCOMPAT);
    CHECK(!meter_sdo_receive(&channel, &wrong));
    CHECK(meter_sdo_take(&channel, 6, &out));
    /* 容量包含在途、排队与未领取结果；复位应留下可观察的取消结果。 */
    for (unsigned i = 0; i < METER_SDO_CAPACITY; ++i)
    {
        r = request(10 + i, 0, METER_SDO_READ);
        CHECK(meter_sdo_submit(&channel, &r));
    }
    CHECK(!meter_sdo_submit(&channel, &r));
    r.request_id = 99;
    CHECK(!meter_sdo_submit(&channel, &r));
    pump();
    meter_sdo_reset(&channel);
    CHECK(!meter_sdo_receive(&channel, &wrong));
    for (unsigned i = 0; i < METER_SDO_CAPACITY; ++i)
    {
        CHECK(meter_sdo_take(&channel, 10 + i, &out));
        CHECK(out.status == METER_SDO_ABORTED);
    }
    peer.drop = false;
    CHECK(sdo_peer_init(&peer));
    r = request(20, 2, METER_SDO_READ);
    CHECK(meter_sdo_submit(&channel, &r));
    out = run(20);
    CHECK(out.status == METER_SDO_SUCCESS);
    puts("abort/overflow/timeout/retry/write retry/TX busy/wrong-late response/reset/queue full PASS");
    return 0;
}
static int queue_and_transport_edges(void)
{
    CHECK(sdo_peer_init(&peer));
    CHECK(meter_sdo_init(&channel, METER_BUS_CAN1));
    meter_diagnostics_init(&diag);
    meter_sdo_bind_diagnostics(&channel, &diag);
    now = 0;
    meter_sdo_result_t out;
    /* 相同对象允许不同请求排队，但线上只能串行，并严格保持顺序。 */
    for (unsigned i = 0; i < METER_SDO_CAPACITY; ++i)
    {
        meter_sdo_request_t r = request(100 + i, 1, METER_SDO_WRITE);
        r.payload[0] = (uint8_t)i;
        CHECK(meter_sdo_submit(&channel, &r));
    }
    for (unsigned i = 0; i < METER_SDO_CAPACITY; ++i)
    {
        out = run(100 + i);
        CHECK(out.status == METER_SDO_SUCCESS && out.attempts == 1);
        CHECK(peer.sent_count == i + 1 && peer.sent[i].data[4] == i);
    }
    meter_sdo_request_t r = request(200, 0, METER_SDO_READ);
    CHECK(!meter_sdo_submit(&channel, &r));
    for (unsigned i = 0; i < METER_SDO_CAPACITY; ++i)
        CHECK(meter_sdo_take(&channel, 100 + i, &out));
    /* 持续 TX busy 也必须由上游超时，恢复时不得发出已过期写请求。 */
    CHECK(sdo_peer_init(&peer));
    peer.busy = 100;
    r = request(201, 1, METER_SDO_WRITE);
    CHECK(meter_sdo_submit(&channel, &r));
    out = run(201);
    CHECK(out.status == METER_SDO_TIMEOUT && channel.tx.bufferFull);
    CHECK(peer.sent_count == 0);
    peer.busy = 0;
    pump();
    CHECK(peer.sent_count == 1 && peer.sent[0].data[0] == 0x80);
    CHECK(meter_sdo_take(&channel, 201, &out));
    /* abort 重试为显式策略，客户端不会把 abort 默认当成功。 */
    r = request(202, 0, METER_SDO_READ);
    r.index = 0x3333;
    r.retry_abort = true;
    r.retry_count = 1;
    CHECK(meter_sdo_submit(&channel, &r));
    out = run(202);
    CHECK(out.status == METER_SDO_ABORTED && out.attempts == 2 && out.abort_code == CO_SDO_AB_NOT_EXIST);
    CHECK(meter_sdo_take(&channel, 202, &out));
    /* ms 时钟回绕不破坏超时与重试延迟。 */
    CHECK(meter_sdo_init(&channel, METER_BUS_CAN1));
    CHECK(sdo_peer_init(&peer));
    now = UINT32_MAX - 10;
    peer.drop = true;
    r = request(203, 0, METER_SDO_READ);
    r.retry_count = 1;
    CHECK(meter_sdo_submit(&channel, &r));
    for (unsigned i = 0; i < 100 && !channel.retry_wait; ++i)
        pump();
    CHECK(channel.retry_wait);
    meter_can_frame_t late = {
        .bus = METER_BUS_CAN1, .id = 0x58c, .size = 8, .data = {0x4f, 0, 0x21, 0, 9, 0, 0, 0}};
    CHECK(!meter_sdo_receive(&channel, &late));
    peer.drop = false;
    out = run(203);
    CHECK(out.status == METER_SDO_SUCCESS && out.attempts == 2);
    CHECK(meter_sdo_take(&channel, 203, &out));
    puts("FIFO/same-object requests/retained capacity/busy timeout/abort retry/clock wrap PASS");
    return 0;
}
int main(void)
{
    CHECK(transfers() == 0);
    CHECK(failures() == 0);
    CHECK(queue_and_transport_edges() == 0);
    return 0;
}
