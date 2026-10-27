#include "protocols/common/meter_frame_router.h"
bool meter_frame_valid(const meter_can_frame_t *f)
{
    return f && (unsigned)f->bus < METER_BUS_COUNT && !f->remote && f->size <= 8 &&
           f->id <= (f->extended ? 0x1fffffffu : 0x7ffu);
}
bool meter_routes_valid(const meter_route_profile_t *routes)
{
    if (!routes || (routes->count && !routes->entries))
        return false;
    for (size_t i = 0; i < routes->count; ++i)
    {
        const meter_frame_route_t *a = &routes->entries[i];
        if ((unsigned)a->bus >= METER_BUS_COUNT ||
            (a->id != METER_CAN_ID_ANY && a->id > (a->extended ? 0x1fffffffu : 0x7ffu)))
            return false;
        for (size_t j = 0; j < i; ++j)
        {
            const meter_frame_route_t *b = &routes->entries[j];
            if (a->bus == b->bus && a->id == b->id && a->extended == b->extended)
                return false;
        }
    }
    return true;
}
const meter_frame_route_t *meter_route_lookup(const meter_route_profile_t *routes,
                                              const meter_can_frame_t *frame)
{
    if (!routes || !meter_frame_valid(frame))
        return NULL;
    const meter_frame_route_t *fallback = NULL;
    for (size_t i = 0; i < routes->count; ++i)
    {
        const meter_frame_route_t *r = &routes->entries[i];
        if (r->bus == frame->bus && r->extended == frame->extended)
        {
            if (r->id == frame->id)
                return r;
            if (r->id == METER_CAN_ID_ANY)
                fallback = r;
        }
    }
    return fallback;
}
