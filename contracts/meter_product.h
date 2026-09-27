#ifndef METER_PRODUCT_H
#define METER_PRODUCT_H
#include "contracts/meter_can_frame.h"
#include "contracts/meter_domain.h"
#include "contracts/meter_protocol.h"
typedef bool (*meter_decode_fn_t)(const meter_can_frame_t *frame, meter_update_sink_t sink, void *context);
typedef struct
{
    meter_frame_route_owner_t owner;
    meter_decode_fn_t decode;
    const meter_protocol_adapter_t *adapter;
} meter_protocol_binding_t;
typedef struct
{
    const meter_protocol_binding_t *bindings;
    size_t count;
} meter_protocol_profile_t;
typedef struct
{
    const meter_frame_route_t *entries;
    size_t count;
} meter_route_profile_t;
typedef struct
{
    bool height, weighing, parameter_write, maintenance, language_selection;
} meter_capability_profile_t;
typedef struct
{
    const char *language;
    const char *title;
} meter_locale_profile_t;
typedef struct
{
    const char *manifest;
} meter_resource_profile_t;
typedef struct
{
    bool local_settings;
    bool vehicle_control;
} meter_auth_profile_t;
typedef struct
{
    void *(*create)(void *parent, const meter_ui_actions_t *actions);
    void (*present)(void *ui, const meter_snapshot_t *snapshot, uint32_t elapsed_ms);
    void (*destroy)(void *ui);
} meter_ui_factory_t;
typedef struct
{
    const char *id;
    const meter_capability_profile_t *capabilities;
    const meter_protocol_profile_t *protocols;
    const meter_route_profile_t *routes;
    const meter_ui_factory_t *ui;
    const meter_resource_profile_t *resources;
    const meter_locale_profile_t *locale;
    const meter_auth_profile_t *auth;
    const meter_catalog_t *catalog;
    void (*evaluate)(meter_snapshot_t *snapshot);
} meter_product_t;
const meter_product_t *meter_product_get(void);
#endif
