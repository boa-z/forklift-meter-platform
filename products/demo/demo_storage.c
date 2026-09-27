#include "products/demo/demo_storage.h"
#include <stddef.h>
#define SLOTS(array) (sizeof(array) / sizeof((array)[0]))
meter_core_storage_t demo_domain_bind(demo_domain_store_t *store)
{
    return (meter_core_storage_t){store->signals, SLOTS(store->signals), store->parameters,
                                  SLOTS(store->parameters), store->faults, SLOTS(store->faults)};
}
