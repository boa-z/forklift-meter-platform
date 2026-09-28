#include "runtime/meter_periodic.h"
#include "contracts/meter_time.h"
#include <assert.h>
static bool encode(const meter_tx_snapshot_t *v, bool fresh, uint32_t wire, meter_can_frame_t *f, uint32_t *next)
{
    f->size = 4u; f->data[0] = (uint8_t)v->values[0].value; f->data[1] = (uint8_t)wire;
    f->data[2] = fresh ? 1u : 0u; f->data[3] = f->data[0] ^ f->data[1] ^ f->data[2];
    *next = (wire + 1u) & 0xffu; return true;
}
int main(void)
{
    meter_tx_value_t data[2] = {{10, 1u, true}, {20, 1u, true}}, published[2] = {0}, local[2] = {0};
    meter_tx_publication_t p = {.values = published}, copy = {.values = local};
    assert(meter_tx_publish(&p, 2u, data, 2u, 1u, 1u) && p.revision == 1u);
    assert(!meter_tx_publish(&p, 1u, data, 2u, 1u, 1u));
    data[0].sample_ms = 2u;
    assert(meter_tx_publish(&p, 2u, data, 2u, 1u, 2u) && p.revision == 1u && p.values[0].sample_ms == 2u);
    assert(meter_tx_publish(&p, 2u, data, 2u, 1u, 10u) && p.values[0].sample_ms == 2u && p.published_ms == 10u);
    data[0].sample_ms = 1u; data[1].value = 99;
    assert(!meter_tx_publish(&p, 2u, data, 2u, 1u, 10u) && p.values[1].value == 20);
    data[0].sample_ms = 2u; data[1].value = 20; data[0].value = 11;
    assert(meter_tx_publish(&p, 2u, data, 2u, 1u, 10u) && p.revision == 2u);
    assert(meter_tx_copy(&copy, 2u, &p));
    assert(!meter_tx_copy(&p, 2u, &p));
    data[0].value = 12; assert(meter_tx_publish(&p, 2u, data, 2u, 1u, 10u));
    assert(copy.values[0].value == 11 && copy.revision == 2u);
    meter_periodic_frame_t d = {.frame = {.id = 0x100u, .bus = METER_BUS_CAN0}, .period_ms = 10u,
        .encode = encode, .value_count = 1u, .max_age_ms = 15u, .freshness = METER_TX_ENCODE_INVALID,
        .commit = METER_TX_COMMIT_DRIVER};
    assert(meter_periodic_valid(&d, 2u));
    meter_periodic_state_t s; meter_periodic_message_t m;
    assert(meter_periodic_reset(&s, &d, 1u, 0u));
    assert(!meter_periodic_prepare(&s, &d, &p, true, 9u, &m));
    assert(meter_periodic_prepare(&s, &d, &p, true, 10u, &m));
    assert(m.frame.data[0] == 12u && m.frame.data[2] == 1u && m.frame.data[3] == 13u);
    /* 队列拒收时不调用 admit，不推进完成型 wire；下一期限重新编码。 */
    assert(s.wire == 0u && !s.busy);
    assert(meter_periodic_prepare(&s, &d, &p, true, 20u, &m) && m.frame.data[2] == 0u);
    assert(meter_periodic_admit(&s, &d, &m) && s.wire == 0u);
    assert(!meter_periodic_prepare(&s, &d, &p, true, 30u, &m));
    meter_periodic_result_t r = {.generation = 1u, .ticket = s.pending_ticket, .success = false};
    assert(meter_periodic_complete(&s, &d, &r) && s.wire == 0u);
    assert(!meter_periodic_complete(&s, &d, &r));
    data[0].sample_ms = 40u; assert(meter_tx_publish(&p, 2u, data, 2u, 1u, 40u));
    assert(meter_periodic_prepare(&s, &d, &p, true, 40u, &m));
    assert(meter_periodic_sendable(&m, 1u, 49u));
    assert(!meter_periodic_sendable(&m, 1u, 50u) && !meter_periodic_sendable(&m, 2u, 41u));
    assert(meter_periodic_admit(&s, &d, &m)); r.ticket = m.ticket; r.success = true;
    assert(meter_periodic_complete(&s, &d, &r) && s.wire == 1u);
    assert(!meter_periodic_admit(&s, &d, &m));
    assert(!meter_periodic_prepare(&s, &d, &p, false, 50u, &m));
    assert(meter_periodic_reset(&s, &d, 2u, 50u) && s.wire == 0u);
    assert(!meter_periodic_prepare(&s, &d, &p, true, 60u, &m));
    assert(meter_tx_publish(&p, 2u, data, 2u, 2u, 60u));
    assert(meter_periodic_prepare(&s, &d, &p, true, 70u, &m));
    assert(meter_periodic_admit(&s, &d, &m));
    assert(!meter_periodic_complete(&s, &d, &r) && s.busy);
    r.generation = 2u; r.ticket = m.ticket;
    assert(meter_periodic_complete(&s, &d, &r));
    d.freshness = METER_TX_SUPPRESS;
    assert(!meter_periodic_prepare(&s, &d, &p, true, 80u, &m));
    d.freshness = METER_TX_HOLD; d.commit = METER_TX_COMMIT_ADMISSION;
    assert(meter_periodic_prepare(&s, &d, &p, true, 90u, &m));
    assert(meter_periodic_admit(&s, &d, &m) && s.wire == 2u);
    r.ticket = m.ticket; r.success = false;
    assert(meter_periodic_complete(&s, &d, &r) && s.wire == 2u);
    /* 独立条目及回绕：跳过旧期限，不补发积压，固定 Product 仍支持。 */
    meter_periodic_frame_t fixed = {.frame = {.id = 0x200u, .size = 1u, .data = {42u}}, .period_ms = 10u};
    meter_periodic_state_t other;
    assert(meter_periodic_reset(&other, &fixed, 3u, UINT32_MAX - 5u));
    assert(meter_periodic_prepare(&other, &fixed, NULL, true, 4u, &m) && m.deadline_ms == 4u);
    assert(m.frame.data[0] == 42u);
    assert(meter_periodic_prepare(&other, &fixed, NULL, true, 39u, &m) && m.deadline_ms == 34u && m.expiry_ms == 44u);
    assert(other.deadline.missed == 2u && !meter_periodic_prepare(&other, &fixed, NULL, true, 39u, &m));
    fixed.period_ms = METER_TIME_HALF_RANGE; assert(!meter_periodic_valid(&fixed, 0u));
    data[0].sample_ms = UINT32_MAX - 1u;
    assert(meter_tx_publish(&p, 2u, data, 2u, 3u, 5u));
    data[0].sample_ms = 6u; assert(!meter_tx_publish(&p, 2u, data, 2u, 3u, 5u));
    return 0;
}
