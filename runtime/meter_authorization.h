#ifndef METER_AUTHORIZATION_H
#define METER_AUTHORIZATION_H
#include <stdbool.h>
#include <stdint.h>

/** App-owned grant. Zero initialization denies protected operations. No credentials are stored here. */
typedef struct
{
    uint64_t epoch;
    uint32_t permissions;
    uint32_t expires_ms;
} meter_authorization_t;

/** Replace a grant; a new epoch invalidates requests admitted under an earlier grant.
 * Lifetime must be nonzero and below the monotonic half range. Invalid input leaves the grant intact.
 * Epoch exhaustion denies renewal; revocation remains available. Never reinitialize a live grant. */
bool meter_authorization_grant(meter_authorization_t *authorization, uint32_t permissions, uint32_t now_ms,
                               uint32_t lifetime_ms);
/** Immediately deny protected operations; does not cancel or undo backend I/O. */
void meter_authorization_revoke(meter_authorization_t *authorization);
/** Pure check. Required mask zero is public access; all nonzero bits must be granted.
 * App serializes all calls; observe grants within half a clock cycle of expiry. */
bool meter_authorization_allows(const meter_authorization_t *authorization, uint32_t required,
                                uint32_t now_ms);
#endif
