#include "core/meter_core.h"
#include "platform/rtthread/meter_execution_budget.h"
#include "product/demo_storage.h"
#include "product/product.h"
#include "protocol/can/demo_pdo.h"
#include "runtime/meter_periodic.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void set_value(meter_snapshot_t *snapshot, meter_signal_id_t id, float value,
                      meter_value_state_t state, uint32_t now)
{
    size_t index = meter_catalog_index(snapshot->catalog, id);
    assert(index < snapshot->catalog->signal_count);
    snapshot->signals[index] = (meter_value_t){value, now, state, METER_SOURCE_DEMO};
}

static void emit_vector(const char *name, const meter_can_frame_t *frame)
{
    printf("%s ", name);
    for (unsigned i = 0u; i < frame->size; ++i)
        printf("%02x", frame->data[i]);
    printf("\n");
}

static meter_periodic_message_t prepare_at(const meter_periodic_frame_t *definition,
                                           const meter_tx_publication_t *publication, uint32_t now)
{
    meter_periodic_state_t state;
    bool ok = meter_periodic_reset(&state, definition, publication->generation, now - definition->period_ms);
    assert(ok);
    meter_periodic_message_t message;
    ok = meter_periodic_prepare(&state, definition, publication, true, now, &message);
    assert(ok);
    return message;
}

int main(void)
{
    const meter_product_t *product = meter_product_get();
    assert(product->periodic_count == 4u && product->tx_value_count == DEMO_TX_VALUE_COUNT);
    assert(product->periodic_count <= METER_BOARD_PERIODIC_SLOTS);
    assert(product->tx_value_count <= METER_BOARD_TX_VALUES);
    demo_domain_store_t storage;
    meter_core_storage_t binding = demo_domain_bind(&storage);
    meter_core_t core;
    bool ok = meter_core_init(&core, product->catalog, &binding);
    assert(ok);
    meter_snapshot_t *domain = &core.snapshot;
    const meter_signal_id_t ids[] = {METER_SPEED, METER_SOC,     METER_HEIGHT,   METER_LOAD,   METER_SEAT,
                                     METER_BRAKE, METER_NEUTRAL, METER_CHARGING, METER_WARNING};
    const float nominal[] = {12.5f, 75.0f, 1.25f, 780.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    for (unsigned i = 0u; i < sizeof(ids) / sizeof(ids[0]); ++i)
        set_value(domain, ids[i], nominal[i], METER_VALUE_VALID, 1000u);
    domain->brightness = 31u;
    meter_tx_value_t samples[DEMO_TX_VALUE_COUNT];
    ok = product->tx_sample(domain, 1100u, samples, DEMO_TX_VALUE_COUNT);
    assert(ok && samples[DEMO_TX_BRIGHTNESS].sample_ms == 1100u);
    for (unsigned i = DEMO_TX_SPEED; i < DEMO_TX_VALUE_COUNT; ++i)
        assert(samples[i].valid && samples[i].sample_ms == 1000u);
    meter_tx_snapshot_t view = {
        .generation = 0x12345678u, .revision = 0x1234u, .count = DEMO_TX_VALUE_COUNT, .values = samples};
    uint32_t next = 0u;
    meter_can_frame_t frame = product->periodic[2].frame;
    ok = demo_pdo_encode(&view, true, 127u, &frame, &next);
    const uint8_t motion[] = {0xe2, 4, 0xe2, 4, 0x0c, 3, 75, 255};
    assert(ok && next == 0u && frame.size == 8u && memcmp(frame.data, motion, 8u) == 0);
    emit_vector("motion", &frame);
    frame = product->periodic[3].frame;
    ok = demo_pdo_encode(&view, true, 255u, &frame, &next);
    const uint8_t status[] = {21, 1, 255, 1, 0x78, 0x56, 0x34, 0x12};
    assert(ok && next == 0u && memcmp(frame.data, status, 8u) == 0);
    emit_vector("status", &frame);

    /* 扩展样本数组后，旧帧的周期、位布局及提交策略仍受黄金向量保护。 */
    const uint8_t legacy[][8] = {{31, 0, 1, 255, 0x34, 0x12, 224, 39},
                                 {0xe2, 4, 1, 255, 0x34, 0x12, 0x1d, 35}};
    for (unsigned i = 0u; i < 2u; ++i)
    {
        frame = product->periodic[i].frame;
        ok = product->periodic[i].encode(&view, true, 255u, &frame, &next);
        assert(ok && next == 0u && memcmp(frame.data, legacy[i], 8u) == 0);
    }
    assert(product->periodic[0].frame.id == 0x3c0u && product->periodic[0].period_ms == 50u);
    assert(product->periodic[0].critical && product->periodic[0].commit == METER_TX_COMMIT_DRIVER);
    assert(product->periodic[1].frame.id == 0x2f0u && product->periodic[1].period_ms == 100u);
    assert(product->periodic[1].commit == METER_TX_COMMIT_ADMISSION);

    /* 非法入口不产生部分输出，也不推进 wire 状态。 */
    frame = product->periodic[2].frame;
    frame.id = 0x123u;
    meter_can_frame_t before = frame;
    next = 42u;
    ok = demo_pdo_encode(&view, true, 0u, &frame, &next);
    assert(!ok && next == 42u && memcmp(&frame, &before, sizeof(frame)) == 0);
    ok = demo_pdo_encode(NULL, true, 0u, &frame, &next);
    assert(!ok);
    ok = demo_pdo_encode(&view, true, 0u, NULL, &next);
    assert(!ok);
    ok = product->tx_sample(domain, 1100u, samples, DEMO_TX_VALUE_COUNT - 1u);
    assert(!ok && samples[DEMO_TX_SPEED].value == 1250);

    /* 每种不可用状态和非有限数必须先于整数转换被拒绝。 */
    const float invalid[] = {-1.0f, 101.0f, NAN, INFINITY};
    for (unsigned i = 0u; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        set_value(domain, METER_SOC, invalid[i], METER_VALUE_VALID, 900u);
        ok = product->tx_sample(domain, 1100u, samples, DEMO_TX_VALUE_COUNT);
        assert(ok && !samples[DEMO_TX_SOC].valid && samples[DEMO_TX_SOC].value == 0);
        assert(samples[DEMO_TX_SOC].sample_ms == 900u);
    }
    const meter_value_state_t unavailable[] = {METER_VALUE_UNKNOWN, METER_VALUE_STALE, METER_VALUE_ERROR};
    for (unsigned i = 0u; i < sizeof(unavailable) / sizeof(unavailable[0]); ++i)
    {
        set_value(domain, METER_SOC, 75.0f, unavailable[i], 1000u);
        ok = product->tx_sample(domain, 1100u, samples, DEMO_TX_VALUE_COUNT);
        assert(ok && !samples[DEMO_TX_SOC].valid);
    }
    set_value(domain, METER_SEAT, 0.5f, METER_VALUE_VALID, 1000u);
    ok = product->tx_sample(domain, 1100u, samples, DEMO_TX_VALUE_COUNT);
    assert(ok && !samples[DEMO_TX_SEAT].valid);
    for (unsigned i = 0u; i < sizeof(ids) / sizeof(ids[0]); ++i)
        set_value(domain, ids[i], nominal[i], METER_VALUE_VALID, 1000u);
    ok = product->tx_sample(domain, 1100u, samples, DEMO_TX_VALUE_COUNT);
    assert(ok);
    meter_tx_value_t copied[DEMO_TX_VALUE_COUNT];
    meter_tx_publication_t publication = {.values = copied};
    ok = meter_tx_publish(&publication, DEMO_TX_VALUE_COUNT, samples, DEMO_TX_VALUE_COUNT, 1u, 1100u);
    assert(ok);
    const meter_periodic_frame_t *pdo = &product->periodic[2];
    meter_periodic_message_t message = prepare_at(pdo, &publication, 1499u);
    assert(message.frame.data[7] == 1u);
    message = prepare_at(pdo, &publication, 1500u);
    const uint8_t zero[8] = {0};
    assert(memcmp(message.frame.data, zero, 8u) == 0);
    emit_vector("stale_motion", &message.frame);
    /* App 重复发布不能刷新输入时间；状态组与运动组独立判定。 */
    ok = product->tx_sample(domain, 1600u, samples, DEMO_TX_VALUE_COUNT);
    assert(ok);
    ok = meter_tx_publish(&publication, DEMO_TX_VALUE_COUNT, samples, DEMO_TX_VALUE_COUNT, 1u, 1600u);
    assert(ok);
    message = prepare_at(pdo, &publication, 1600u);
    assert(message.frame.data[7] == 0u);
    for (unsigned i = 0u; i < 4u; ++i)
        set_value(domain, ids[i], 0.0f, METER_VALUE_VALID, 1600u);
    ok = product->tx_sample(domain, 1600u, samples, DEMO_TX_VALUE_COUNT);
    assert(ok);
    ok = meter_tx_publish(&publication, DEMO_TX_VALUE_COUNT, samples, DEMO_TX_VALUE_COUNT, 1u, 1600u);
    assert(ok);
    message = prepare_at(pdo, &publication, 1700u);
    assert(message.frame.data[0] == 0u && message.frame.data[7] == 1u);
    emit_vector("zero_motion", &message.frame);
    message = prepare_at(&product->periodic[3], &publication, 1700u);
    assert(message.frame.data[0] == 0u && message.frame.data[1] == 0u);
    emit_vector("stale_status", &message.frame);

    /* 新 PDO 仍由既有调度器拥有：期限前不发，驱动失败不推进计数。 */
    meter_periodic_state_t state;
    ok = meter_periodic_reset(&state, pdo, 1u, 1600u);
    assert(ok);
    ok = meter_periodic_prepare(&state, pdo, &publication, true, 1699u, &message);
    assert(!ok);
    ok = meter_periodic_prepare(&state, pdo, &publication, true, 1700u, &message);
    assert(ok);
    ok = meter_periodic_admit(&state, pdo, &message);
    assert(ok && state.wire == 0u);
    meter_periodic_result_t result = {.generation = 1u, .ticket = message.ticket, .success = false};
    ok = meter_periodic_complete(&state, pdo, &result);
    assert(ok && state.wire == 0u);
    ok = meter_periodic_prepare(&state, pdo, &publication, true, 1800u, &message);
    assert(ok && message.frame.data[7] == 1u);
    ok = meter_periodic_admit(&state, pdo, &message);
    assert(ok);
    result.ticket = message.ticket;
    result.success = true;
    ok = meter_periodic_complete(&state, pdo, &result);
    assert(ok && state.wire == 1u);
    meter_mode_policy_t maintenance = product->mode_policy(METER_MODE_UPDATE_MAINTENANCE);
    assert(!pdo->critical && !maintenance.ordinary_tx);
    return 0;
}
