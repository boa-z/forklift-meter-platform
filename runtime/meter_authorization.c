#include "runtime/meter_authorization.h"
#include "contracts/meter_time.h"
#include <stddef.h>

bool meter_authorization_grant(meter_authorization_t *authorization, uint32_t permissions, uint32_t now_ms,
                               uint32_t lifetime_ms)
{
    if ((authorization == NULL) || (permissions == 0u) || (lifetime_ms == 0u) ||
        (lifetime_ms >= METER_TIME_HALF_RANGE) || (authorization->epoch == UINT64_MAX))
    {
        return false;
    }
    authorization->epoch++;
    authorization->permissions = permissions;
    authorization->expires_ms = now_ms + lifetime_ms;
    return true;
}

void meter_authorization_revoke(meter_authorization_t *authorization)
{
    if (authorization != NULL)
    {
        authorization->permissions = 0u;
    }
}

bool meter_authorization_allows(const meter_authorization_t *authorization, uint32_t required,
                                uint32_t now_ms)
{
    if (required == 0u)
    {
        return true;
    }
    return (authorization != NULL) && (authorization->epoch != 0u) &&
           ((authorization->permissions & required) == required) &&
           !meter_time_reached(now_ms, authorization->expires_ms);
}
