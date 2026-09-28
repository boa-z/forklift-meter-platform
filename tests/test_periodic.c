#include "contracts/meter_time.h"
#include "runtime/meter_periodic.h"
#include <assert.h>

/* Reject the whole publication even when the bad value follows a valid change. */
static void test_publication_rejection_is_atomic(void)
{
    meter_tx_value_t stored[2] = {{10, 5u, true}, {20, 5u, true}};
    meter_tx_publication_t publication = {
        .generation = 1u, .published_ms = 5u, .revision = 7u, .count = 2u, .values = stored};
    meter_tx_value_t incoming[2] = {{99, 6u, true}, {20, 4u, true}};

    assert(!meter_tx_publish(&publication, 2u, incoming, 2u, 1u, 6u));
    assert(stored[0].value == 10 && stored[0].sample_ms == 5u);
    assert(stored[1].value == 20 && stored[1].sample_ms == 5u);
    assert(publication.revision == 7u && publication.published_ms == 5u);
    assert(publication.generation == 1u && publication.count == 2u);
    assert(publication.values == stored);

    /* A new session permits older samples, but never samples from the future. */
    incoming[1].sample_ms = 7u;
    assert(!meter_tx_publish(&publication, 2u, incoming, 2u, 2u, 6u));
    assert(publication.generation == 1u && publication.revision == 7u);
    assert(stored[0].value == 10);
    incoming[1].sample_ms = 4u;
    assert(meter_tx_publish(&publication, 2u, incoming, 2u, 2u, 6u));
    assert(publication.generation == 2u && publication.revision == 8u);
    assert(stored[0].value == 99 && stored[1].sample_ms == 4u);
}

static void test_publication_revision_exhaustion(void)
{
    meter_tx_value_t stored = {10, 1u, true};
    meter_tx_value_t incoming = {11, 2u, true};
    meter_tx_publication_t publication = {
        .generation = 1u, .published_ms = 1u, .revision = UINT64_MAX, .count = 1u, .values = &stored};

    assert(!meter_tx_publish(&publication, 1u, &incoming, 1u, 1u, 2u));
    assert(stored.value == 10 && stored.sample_ms == 1u);
    assert(publication.revision == UINT64_MAX && publication.published_ms == 1u);

    /* Refreshing sample time without changing semantics consumes no revision. */
    incoming.value = 10;
    assert(meter_tx_publish(&publication, 1u, &incoming, 1u, 1u, 2u));
    assert(publication.revision == UINT64_MAX && stored.sample_ms == 2u);
    assert(publication.published_ms == 2u);
}

static void test_publication_copy_rejects_partial_overlap(void)
{
    meter_tx_value_t values[3] = {{10, 1u, true}, {20, 2u, true}, {30, 3u, true}};
    meter_tx_publication_t source = {
        .generation = 1u, .published_ms = 2u, .revision = 3u, .count = 2u, .values = values};
    meter_tx_publication_t destination = {
        .generation = 9u, .published_ms = 8u, .revision = 7u, .count = 1u, .values = &values[1]};

    assert(!meter_tx_copy(&destination, 2u, &source));
    assert(destination.values == &values[1] && destination.count == 1u);
    assert(destination.generation == 9u && destination.revision == 7u && destination.published_ms == 8u);
    assert(values[1].value == 20 && values[2].value == 30);
    destination.count = 2u;
    assert(!meter_tx_copy(&source, 2u, &destination));
    /* Adjacent arrays do not overlap. The destination keeps its own storage. */
    destination.values = &values[2];
    source.count = 1u;
    assert(meter_tx_copy(&destination, 1u, &source));
    assert(destination.values == &values[2] && destination.count == 1u);
    assert(destination.revision == 3u && values[2].value == 10);
}

typedef enum
{
    ENCODER_FAIL,
    ENCODER_CHANGE_BUS,
    ENCODER_CHANGE_ID,
    ENCODER_CHANGE_EXTENDED,
    ENCODER_CHANGE_REMOTE,
    ENCODER_OVERSIZE
} encoder_fault_t;
static encoder_fault_t encoder_fault;

static bool invalid_encoder(const meter_tx_snapshot_t *snapshot, bool fresh, uint32_t wire,
                            meter_can_frame_t *frame, uint32_t *next_wire)
{
    (void)snapshot;
    (void)fresh;
    *next_wire = wire + 1u;
    switch (encoder_fault)
    {
    case ENCODER_FAIL:
        return false;
    case ENCODER_CHANGE_BUS:
        frame->bus = METER_BUS_CAN1;
        break;
    case ENCODER_CHANGE_ID:
        ++frame->id;
        break;
    case ENCODER_CHANGE_EXTENDED:
        frame->extended = !frame->extended;
        break;
    case ENCODER_CHANGE_REMOTE:
        frame->remote = !frame->remote;
        break;
    case ENCODER_OVERSIZE:
        frame->size = 9u;
        break;
    }
    return true;
}

static void test_encoder_rejection_consumes_only_deadline(void)
{
    meter_tx_value_t value = {10, 0u, true};
    meter_tx_publication_t publication = {.generation = 1u, .revision = 1u, .count = 1u, .values = &value};
    const meter_periodic_frame_t definition = {.frame = {.id = 0x100u, .bus = METER_BUS_CAN0},
                                               .period_ms = 10u,
                                               .encode = invalid_encoder,
                                               .value_count = 1u,
                                               .initial_wire = 5u};

    for (unsigned fault = ENCODER_FAIL; fault <= ENCODER_OVERSIZE; ++fault)
    {
        meter_periodic_state_t state;
        meter_periodic_message_t message = {.ticket = 99u, .frame = {.id = 0x321u}};
        encoder_fault = (encoder_fault_t)fault;
        assert(meter_periodic_reset(&state, &definition, 1u, 0u));
        assert(!meter_periodic_prepare(&state, &definition, &publication, true, 10u, &message));
        assert(state.rejected == 1u && state.deadline.next_ms == 20u);
        assert(state.wire == 5u && state.proposed_wire == 0u && state.ticket == 0u && !state.busy);
        assert(message.ticket == 99u && message.frame.id == 0x321u);
        assert(!meter_periodic_prepare(&state, &definition, &publication, true, 10u, &message));
        assert(state.rejected == 1u);
    }
}

static void test_ticket_exhaustion_skips_without_wrap(void)
{
    const meter_periodic_frame_t definition = {.frame = {.id = 0x100u}, .period_ms = 10u};
    meter_periodic_state_t state;
    meter_periodic_message_t message = {.ticket = 99u};

    assert(meter_periodic_reset(&state, &definition, 1u, 0u));
    state.ticket = UINT64_MAX;
    assert(!meter_periodic_prepare(&state, &definition, NULL, true, 10u, &message));
    assert(state.ticket == UINT64_MAX && state.skipped == 1u && state.deadline.next_ms == 20u);
    assert(!state.busy && message.ticket == 99u);
}

static bool encode(const meter_tx_snapshot_t *v, bool fresh, uint32_t wire, meter_can_frame_t *f,
                   uint32_t *next)
{
    f->size = 4u;
    f->data[0] = (uint8_t)v->values[0].value;
    f->data[1] = (uint8_t)wire;
    f->data[2] = fresh ? 1u : 0u;
    f->data[3] = f->data[0] ^ f->data[1] ^ f->data[2];
    *next = (wire + 1u) & 0xffu;
    return true;
}
static void test_periodic_lifecycle(void)
{
    meter_tx_value_t data[2] = {{10, 1u, true}, {20, 1u, true}}, published[2] = {0}, local[2] = {0};
    meter_tx_publication_t p = {.values = published}, copy = {.values = local};
    assert(meter_tx_publish(&p, 2u, data, 2u, 1u, 1u) && p.revision == 1u);
    assert(!meter_tx_publish(&p, 1u, data, 2u, 1u, 1u));
    data[0].sample_ms = 2u;
    assert(meter_tx_publish(&p, 2u, data, 2u, 1u, 2u) && p.revision == 1u && p.values[0].sample_ms == 2u);
    assert(meter_tx_publish(&p, 2u, data, 2u, 1u, 10u) && p.values[0].sample_ms == 2u &&
           p.published_ms == 10u);
    data[0].sample_ms = 1u;
    data[1].value = 99;
    assert(!meter_tx_publish(&p, 2u, data, 2u, 1u, 10u) && p.values[1].value == 20);
    data[0].sample_ms = 2u;
    data[1].value = 20;
    data[0].value = 11;
    assert(meter_tx_publish(&p, 2u, data, 2u, 1u, 10u) && p.revision == 2u);
    assert(meter_tx_copy(&copy, 2u, &p));
    assert(!meter_tx_copy(&p, 2u, &p));
    data[0].value = 12;
    assert(meter_tx_publish(&p, 2u, data, 2u, 1u, 10u));
    assert(copy.values[0].value == 11 && copy.revision == 2u);
    meter_periodic_frame_t d = {.frame = {.id = 0x100u, .bus = METER_BUS_CAN0},
                                .period_ms = 10u,
                                .encode = encode,
                                .value_count = 1u,
                                .max_age_ms = 15u,
                                .freshness = METER_TX_ENCODE_INVALID,
                                .commit = METER_TX_COMMIT_DRIVER};
    assert(meter_periodic_valid(&d, 2u));
    meter_periodic_state_t s;
    meter_periodic_message_t m;
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
    data[0].sample_ms = 40u;
    assert(meter_tx_publish(&p, 2u, data, 2u, 1u, 40u));
    assert(meter_periodic_prepare(&s, &d, &p, true, 40u, &m));
    assert(meter_periodic_sendable(&m, 1u, 49u));
    assert(!meter_periodic_sendable(&m, 1u, 50u) && !meter_periodic_sendable(&m, 2u, 41u));
    assert(meter_periodic_admit(&s, &d, &m));
    r.ticket = m.ticket;
    r.success = true;
    assert(meter_periodic_complete(&s, &d, &r) && s.wire == 1u);
    assert(!meter_periodic_admit(&s, &d, &m));
    assert(!meter_periodic_prepare(&s, &d, &p, false, 50u, &m));
    assert(meter_periodic_reset(&s, &d, 2u, 50u) && s.wire == 0u);
    assert(!meter_periodic_prepare(&s, &d, &p, true, 60u, &m));
    assert(meter_tx_publish(&p, 2u, data, 2u, 2u, 60u));
    assert(meter_periodic_prepare(&s, &d, &p, true, 70u, &m));
    assert(meter_periodic_admit(&s, &d, &m));
    assert(!meter_periodic_complete(&s, &d, &r) && s.busy);
    r.generation = 2u;
    r.ticket = m.ticket;
    assert(meter_periodic_complete(&s, &d, &r));
    d.freshness = METER_TX_SUPPRESS;
    assert(!meter_periodic_prepare(&s, &d, &p, true, 80u, &m));
    d.freshness = METER_TX_HOLD;
    d.commit = METER_TX_COMMIT_ADMISSION;
    assert(meter_periodic_prepare(&s, &d, &p, true, 90u, &m));
    assert(meter_periodic_admit(&s, &d, &m) && s.wire == 2u);
    r.ticket = m.ticket;
    r.success = false;
    assert(meter_periodic_complete(&s, &d, &r) && s.wire == 2u);
    /* 独立条目及回绕：跳过旧期限，不补发积压，固定 Product 仍支持。 */
    meter_periodic_frame_t fixed = {.frame = {.id = 0x200u, .size = 1u, .data = {42u}}, .period_ms = 10u};
    meter_periodic_state_t other;
    assert(meter_periodic_reset(&other, &fixed, 3u, UINT32_MAX - 5u));
    assert(meter_periodic_prepare(&other, &fixed, NULL, true, 4u, &m) && m.deadline_ms == 4u);
    assert(m.frame.data[0] == 42u);
    assert(meter_periodic_prepare(&other, &fixed, NULL, true, 39u, &m) && m.deadline_ms == 34u &&
           m.expiry_ms == 44u);
    assert(other.deadline.missed == 2u && !meter_periodic_prepare(&other, &fixed, NULL, true, 39u, &m));
    fixed.period_ms = METER_TIME_HALF_RANGE;
    assert(!meter_periodic_valid(&fixed, 0u));
    data[0].sample_ms = UINT32_MAX - 1u;
    assert(meter_tx_publish(&p, 2u, data, 2u, 3u, 5u));
    data[0].sample_ms = 6u;
    assert(!meter_tx_publish(&p, 2u, data, 2u, 3u, 5u));
}
int main(void)
{
    test_publication_rejection_is_atomic();
    test_publication_revision_exhaustion();
    test_publication_copy_rejects_partial_overlap();
    test_encoder_rejection_consumes_only_deadline();
    test_ticket_exhaustion_skips_without_wrap();
    test_periodic_lifecycle();
    return 0;
}
