#include "protocols/common/hmi_frame_router.h"
bool hmi_frame_valid(const hmi_can_frame_t *f)
{
    return f && (unsigned)f->bus < HMI_BUS_COUNT && !f->remote && f->size <= 8 &&
           f->id <= (f->extended ? 0x1fffffffu : 0x7ffu);
}
bool hmi_routes_valid(const hmi_route_profile_t *routes)
{
    if (!routes || (routes->count && !routes->entries))
        return false;
    for (size_t i = 0; i < routes->count; ++i)
    {
        const hmi_frame_route_t *a = &routes->entries[i];
        if ((unsigned)a->bus >= HMI_BUS_COUNT ||
            (a->id != HMI_CAN_ID_ANY && a->id > (a->extended ? 0x1fffffffu : 0x7ffu)))
            return false;
        for (size_t j = 0; j < i; ++j)
        {
            const hmi_frame_route_t *b = &routes->entries[j];
            if (a->bus == b->bus && a->id == b->id && a->extended == b->extended)
                return false;
        }
    }
    return true;
}
const hmi_frame_route_t *hmi_route_lookup(const hmi_route_profile_t *routes,
                                              const hmi_can_frame_t *frame)
{
    if (!routes || !hmi_frame_valid(frame))
        return NULL;
    const hmi_frame_route_t *fallback = NULL;
    for (size_t i = 0; i < routes->count; ++i)
    {
        const hmi_frame_route_t *r = &routes->entries[i];
        if (r->bus == frame->bus && r->extended == frame->extended)
        {
            if (r->id == frame->id)
                return r;
            if (r->id == HMI_CAN_ID_ANY)
                fallback = r;
        }
    }
    return fallback;
}
