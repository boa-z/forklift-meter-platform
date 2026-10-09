#include "core/meter_snapshot.h"
#include "runtime/meter_batch_builder.h"
#include "runtime/meter_requests.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                                                     \
    do                                                                                                       \
    {                                                                                                        \
        if (!(condition))                                                                                    \
        {                                                                                                    \
            fprintf(stderr, "framework-contracts:%d: %s\n", __LINE__, #condition);                         \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)

static const meter_signal_def_t signals[] = {{.id = 1u, .key = "speed", .unit = "km/h", .stale_ms = 750u}};
static const meter_parameter_def_t parameters[] = {{.id = 1u, .key = "brightness", .unit = "%", .min = 10.0f, .max = 100.0f, .initial = 50.0f}};
static const meter_fault_def_t faults[] = {{1u, "fault", "fault"}};
static const meter_catalog_t catalog = {signals, 1u, parameters, 1u, NULL, 0u, faults, 1u, NULL, NULL};

static int snapshot_contract(void)
{
    meter_value_t source_signals[1] = {{25.0f, 10u, METER_VALUE_VALID, METER_SOURCE_DEMO}};
    float source_parameters[1] = {50.0f};
    meter_fault_state_t source_faults[1] = {{1u, false}};
    meter_snapshot_t source = {&catalog, source_signals, source_parameters, source_faults,
                               1u, 4u, true, false, METER_LANGUAGE_EN, 50u};
    meter_value_t target_signals[1] = {{0}};
    float target_parameters[1] = {0.0f};
    meter_fault_state_t target_faults[1] = {{0}};
    meter_core_storage_t storage = {target_signals, 1u, target_parameters, 1u, target_faults, 1u};
    meter_snapshot_t target = {0};
    CHECK(meter_snapshot_copy(&target, &storage, &source) == METER_SNAPSHOT_COPIED);
    CHECK(target.signals != source.signals && target.signals[0].value == 25.0f);
    CHECK(target.parameters != source.parameters && target.parameters[0] == 50.0f);
    CHECK(target.faults != source.faults && target.faults[0].id == 1u);
    target_signals[0].value = 99.0f;
    CHECK(source_signals[0].value == 25.0f);
    meter_core_storage_t short_storage = storage;
    short_storage.signal_capacity = 0u;
    CHECK(meter_snapshot_copy(&target, &short_storage, &source) == METER_SNAPSHOT_CAPACITY);
    return 0;
}

static meter_batch_result_t accepted;
static size_t submit_count;
static meter_batch_result_t submit_batch(void *context, const meter_update_batch_t *batch)
{
    (void)context;
    ++submit_count;
    CHECK(batch->count == 1u);
    CHECK(batch->updates[0].signal == 1u);
    accepted = METER_BATCH_QUEUED;
    return accepted;
}

static int batch_contract(void)
{
    meter_update_t updates[2] = {{0}};
    meter_batch_builder_t builder;
    CHECK(meter_batch_builder_init(&builder, updates, 2u));
    meter_update_batch_t metadata = {1u, 1u, 20u, 0u, METER_SOURCE_DEMO, NULL, 0u};
    CHECK(meter_batch_builder_begin(&builder, &metadata));
    meter_update_t update = {1u, {25.0f, 20u, METER_VALUE_VALID, METER_SOURCE_DEMO}};
    CHECK(meter_batch_builder_add(&builder, &update));
    CHECK(meter_batch_builder_finish(&builder, true, submit_batch, NULL) == METER_BATCH_QUEUED);
    CHECK(submit_count == 1u);
    CHECK(meter_batch_builder_begin(&builder, &metadata));
    CHECK(meter_batch_builder_finish(&builder, true, submit_batch, NULL) == METER_BATCH_QUEUED);
    return 0;
}

static int request_contract(void)
{
    meter_request_slot_t slots[2];
    meter_requests_t requests;
    meter_request_id_t first, second;
    CHECK(meter_requests_init(&requests, slots, 2u, 7u));
    CHECK(meter_requests_reserve(&requests, &first) == METER_REQUEST_QUEUED);
    CHECK(meter_requests_reserve(&requests, &second) == METER_REQUEST_QUEUED);
    meter_request_id_t third;
    CHECK(meter_requests_reserve(&requests, &third) == METER_REQUEST_BUSY);
    CHECK(meter_requests_cancel(&requests, first));
    meter_request_result_t result;
    CHECK(meter_requests_query(&requests, first, &result));
    CHECK(result.code == METER_RESULT_CANCELLED_BEFORE_IO);
    CHECK(!meter_requests_start(&requests, first));
    CHECK(meter_requests_acknowledge(&requests, first));
    CHECK(meter_requests_start(&requests, second));
    meter_request_result_t completion = {second, METER_RESULT_DRIVER_COMPLETED, 0u, 0};
    CHECK(meter_requests_finish(&requests, &completion));
    CHECK(meter_requests_query(&requests, second, &result) && result.code == METER_RESULT_DRIVER_COMPLETED);
    CHECK(meter_requests_acknowledge(&requests, second));
    return 0;
}

int main(void)
{
    int result = snapshot_contract();
    if (result == 0)
        result = batch_contract();
    if (result == 0)
        result = request_contract();
    puts(result == 0 ? "framework-contracts: PASS" : "framework-contracts: FAIL");
    return result;
}
