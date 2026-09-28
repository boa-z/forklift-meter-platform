#include "core/meter_settings.h"
#include "runtime/meter_runtime.h"
#include "runtime/meter_requests.h"
#include "storage/meter_record.h"
#include "update/meter_package.h"
#include <string.h>
/** @brief libFuzzer 单线程入口；每次输入重新初始化全部可变状态。 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 4096u) return 0;
    meter_record_view_t record;
    (void)meter_record_decode(data, size, 1u, 1u, &record);
    static const meter_signal_def_t signal[] = {{1u, "a", "", 100u}};
    static const meter_parameter_def_t parameter[] = {{1u, "b", "", 0.0f, 100.0f, 50.0f}};
    static const meter_catalog_t catalog = {.signals = signal, .signal_count = 1u,
        .parameters = parameter, .parameter_count = 1u};
    meter_value_t values[1]; float parameters[1]; meter_core_t core;
    meter_core_storage_t storage = {.signals = values, .signal_capacity = 1u,
        .parameters = parameters, .parameter_capacity = 1u};
    if (!meter_core_init(&core, &catalog, &storage)) return 0;
    (void)meter_settings_decode(&core, data, size);
    meter_package_guard_t guard;
    meter_package_policy_t policy = {.package_size = (uint32_t)size, .candidate_capacity = 4096u,
        .os_file = "d13x_os.itb", .version = "fuzz"};
    if (meter_package_init(&guard, &policy))
    {
        size_t split = size ? data[0] % size : 0u;
        (void)meter_package_feed(&guard, data, split);
        (void)meter_package_feed(&guard, data + split, size - split);
        (void)meter_package_finish(&guard);
    }
    meter_request_slot_t slots[4]; meter_requests_t requests;
    if (!meter_requests_init(&requests, slots, 4u, 1u)) return 0;
    meter_request_id_t id = {1u, 1u}; meter_request_result_t result = {0};
    for (size_t i = 0u; i < size; ++i)
    {
        switch (data[i] % 6u)
        {
        case 0u: (void)meter_requests_reserve(&requests, &id); break;
        case 1u: (void)meter_requests_start(&requests, id); break;
        case 2u: (void)meter_requests_cancel(&requests, id); break;
        case 3u: result.id = id; result.code = METER_RESULT_APPLIED; (void)meter_requests_finish(&requests, &result); break;
        case 4u: (void)meter_requests_query(&requests, id, &result); break;
        case 5u: (void)meter_requests_acknowledge(&requests, id); break;
        default: break;
        }
    }
    const meter_product_t *product = meter_product_get();
    meter_runtime_t runtime;
    if (meter_runtime_init(&runtime, product, meter_core_apply, &core))
    {
        meter_runtime_connection(&runtime, true);
        for (size_t i = 0u; i + 10u <= size; i += 10u)
        {
            meter_can_frame_t frame = {.bus = METER_BUS_CAN0,
                .id = ((uint32_t)data[i] << 8u) | data[i + 1u], .size = 8u};
            memcpy(frame.data, data + i + 2u, 8u);
            (void)meter_runtime_push(&runtime, &frame);
            (void)meter_runtime_poll(&runtime, 1u);
        }
    }
    return 0;
}
