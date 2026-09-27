#include "protocols/canopen/meter_canopen_profile.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
    // Platform 是 feature-selectable：NMT/Heartbeat 可选，不再全局拒绝。
    meter_canopen_profile_t full = {5, true, true, true, true, true, true, true, true};
    CHECK(meter_canopen_profile_valid(&full));
    // Reference-Mixed 明确选择 PDO RX/TX + SDO Client，无 NMT/Heartbeat。
    meter_canopen_profile_t mixed = {12, true, true, true, false, false, false, false, false};
    CHECK(meter_canopen_profile_valid(&mixed));
    CHECK(meter_canopen_profile_pdo_sdo_only(&mixed));
    CHECK(!meter_canopen_profile_pdo_sdo_only(&full));
    // 空能力或非法 node_id 仍然拒绝。
    meter_canopen_profile_t empty = {5, false, false, false, false, false, false, false, false};
    CHECK(!meter_canopen_profile_valid(&empty));
    meter_canopen_profile_t bad_node = {0, true, false, false, false, false, false, false, false};
    CHECK(!meter_canopen_profile_valid(&bad_node));
    CHECK(!meter_canopen_profile_valid(NULL));
    // 纯 NMT/Heartbeat 产品在平台层同样合法（策略由产品决定）。
    meter_canopen_profile_t nmt_only = {7, false, false, false, false, true, true, false, false};
    CHECK(meter_canopen_profile_valid(&nmt_only));
    return 0;
}
