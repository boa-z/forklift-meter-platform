#ifndef METER_PROFILE_H
#define METER_PROFILE_H
#include <stdbool.h>
#include <stdint.h>
/* Product-defined stable family and normalized feature vocabulary, never raw CAN bits.
 * Separate from static build capabilities and transport connection generation.
 * Zero is unknown/unconfirmed; identical refreshes do not advance generation. */
typedef struct
{
    uint32_t generation;
    uint16_t family;
    uint64_t capabilities;
    bool confirmed;
} meter_profile_t;
static inline bool meter_profile_matches(const meter_profile_t *profile, uint32_t generation)
{
    return profile && profile->confirmed && profile->family != 0u && generation != 0u &&
           profile->generation == generation;
}
#endif
