#include "contracts/meter_product.h"
#include "catalog/catalog.h"
#include "canopen/mixed_canopen.h"
#ifndef METER_HEADLESS
extern const meter_ui_factory_t product_ui;
#define UI_FACTORY &product_ui
#else
#define UI_FACTORY NULL
#endif
extern bool mixed_can0_decode(const meter_can_frame_t *, meter_update_sink_t, void *);
static const meter_protocol_binding_t bindings[] = {{1, mixed_can0_decode, NULL},
                                                    {2, NULL, &mixed_canopen_adapter}};
static const meter_frame_route_t entries[] = {
#include "generated/can/routes.inc"
    {1, 524, false, 2},
    {1, 1420, false, 2},
};
static const meter_route_profile_t routes = {entries, sizeof(entries) / sizeof(entries[0])};
static const meter_protocol_profile_t protocols = {bindings, sizeof(bindings) / sizeof(bindings[0])};
static const meter_capability_profile_t capabilities = {.height = true,
                                                        .weighing = true,
                                                        .parameter_write = true,
                                                        .maintenance = false,
                                                        .language_selection = true};
static const meter_locale_profile_t locale = {"en", "reference-mixed"};
static const meter_resource_profile_t resources = {"assets/README.md"};
static const meter_auth_profile_t auth = {true, true};
static const meter_product_t product = {.id = "reference-mixed",
                                        .protocols = &protocols,
                                        .routes = &routes,
                                        .catalog = &mixed_catalog,
                                        .ui = UI_FACTORY,
                                        .capabilities = &capabilities,
                                        .locale = &locale,
                                        .resources = &resources,
                                        .auth = &auth,
                                        .command_route = mixed_command_route,
                                        .command_route_context = NULL};
/** @brief 返回静态产品定义；CAN0 为 DBC 私有协议，CAN1 为 PDO+SDO（无 NMT/Heartbeat）。 */
const meter_product_t *meter_product_get(void)
{
    mixed_canopen_init(&mixed_canopen_state);
    return &product;
}
