#ifndef DEMO_PRESENTATION_H
#define DEMO_PRESENTATION_H
#include "contracts/meter_domain.h"
#include "generated/demo_catalog.h"
/* Product-owned values. No borrowed mutable snapshot arrays, LVGL or runtime state. */
typedef struct
{
    float value;
    meter_value_state_t state;
} demo_readout_t;
typedef enum
{
    DEMO_LINK_CONNECTED,
    DEMO_LINK_OFFLINE,
    DEMO_LINK_STALE,
    DEMO_LINK_WAITING
} demo_link_t;
typedef struct
{
    demo_readout_t reading;
    const char *unit;
} demo_monitor_view_t;
typedef struct
{
    uint16_t code;
    bool active;
} demo_fault_view_t;
typedef struct
{
    uint32_t revision;
    meter_profile_t profile;
    meter_language_t language;
    bool imperial;
    uint8_t brightness;
    bool limit_available;
    float limit;
    demo_link_t link;
    demo_readout_t speed, steering, soc, height, load;
    float speed_maximum;
    const char *speed_unit;
    demo_readout_t status[5];
    demo_monitor_view_t monitors[DEMO_MONITOR_SLOTS];
    demo_fault_view_t faults[DEMO_FAULT_SLOTS];
} demo_presentation_t;
/* Pure projection of an already-consistent snapshot. Product catalog is immutable.
 * Call on the receiving owner's copied snapshot; no second publication system. */
void demo_presentation_build(const meter_snapshot_t *snapshot, demo_presentation_t *out);
meter_action_t demo_speed_limit_intent(float value);
#endif
