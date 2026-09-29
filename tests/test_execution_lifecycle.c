/* One fresh process per scenario; production objects are never reset behind running owners. */
#include "platform/rtthread/meter_execution_port.c"
#include "tests/stubs/execution/fixture.h"
#include <assert.h>
#include <stdlib.h>

static const meter_signal_def_t signals[] = {{.id = 1u}};
static const meter_catalog_t catalog = {.signals = signals, .signal_count = 1u};
static const meter_protocol_profile_t protocols = {0};
static const meter_route_profile_t routes = {0};
static const meter_auth_profile_t auth = {.local_settings = true};
static meter_storage_profile_t storage_profile;
static const meter_product_t product = {.catalog = &catalog,
                                        .protocols = &protocols,
                                        .routes = &routes,
                                        .auth = &auth,
                                        .storage = &storage_profile};
static meter_core_t core;
static meter_value_t domain[1], presentation[1], diagnostic[1];
static meter_diagnostics_t diagnostics;
static unsigned waits;

static void finish_failed_start(void)
{
    assert(shared.state == METER_EXEC_STOPPING);
    assert(shared.stop_requested);
    if (waits == 0u)
    {
        assert(shared.protocol_done);
        assert(shared.tx_done[0] == (fail_startup == 2u || !nvm_start_ok));
        assert(shared.tx_done[1] == (fail_startup <= 3u || !nvm_start_ok));
    }
    assert(waits++ < 4u);
    /* Only workers that actually started need their completion acknowledgements. */
    shared.protocol_done = true;
    shared.tx_done[0] = true;
    shared.tx_done[1] = true;
    nvm_stopped = true;
    if (shared.ui_release)
        meter_execution_ui_stopped();
}

static void durability_steps(void)
{
    assert(shared.state == METER_EXEC_STOPPING);
    assert(!meter_execution_stopped());
    assert(state_lock.depth == 0u && view_lock.depth == 0u && tx_publication_lock.depth == 0u);
    ++waits;
    fake_now += 1000u; /* Waiting does not turn into forced cleanup even beyond ordinary timeouts. */
    if (waits <= 6u)
        assert(!shared.ui_release && close_calls == 0u);
    switch (waits)
    {
    case 1u:
        assert(flush_calls == 0u);
        nvm_ready = true;
        break;
    case 2u:
        assert(flush_calls == 1u && stop_calls == 0u);
        flush_ticket = 7u;
        break;
    case 3u:
        assert(flush_calls == 2u && stop_calls == 0u);
        barrier_ready = true;
        break;
    case 4u:
        assert(stop_calls == 1u);
        nvm_stopped = true;
        break;
    case 5u:
        shared.protocol_done = true;
        shared.tx_done[0] = true;
        break;
    case 6u:
        shared.tx_done[1] = true;
        break;
    case 7u:
        assert(shared.ui_release && close_calls == 1u && !shared.ui_done);
        meter_execution_ui_stopped();
        break;
    default:
        assert(false);
    }
}

static void begin_durability_stop(void)
{
    assert(startup_calls == 4u && settings_boot_calls == 1u);
    nvm_ready = false;
    meter_execution_stop();
    app_wait_hook = durability_steps;
}
static void finish_boot_stop(void)
{
    assert(shared.state == METER_EXEC_STOPPING);
    shared.protocol_done = true;
    shared.tx_done[0] = true;
    shared.tx_done[1] = true;
    nvm_stopped = true;
    nvm_ready = true;
    if (shared.ui_release) meter_execution_ui_stopped();
    assert(waits++ < 8u);
}
static void stop_after_boot(void)
{
    assert(settings_boot_calls == 1u && boot_rate == METER_CAN_RATE_250K);
    assert(startup_calls == 4u);
    meter_execution_stop();
    app_wait_hook = finish_boot_stop;
}
static void complete_boot_scan(void)
{
    assert(startup_calls == 1u && settings_boot_calls == 0u);
    assert(state_lock.depth == 0u);
    core.snapshot.can_rate = METER_CAN_RATE_250K;
    nvm_ready = true;
    app_wait_hook = stop_after_boot;
}
static void cancel_boot_scan(void)
{
    assert(startup_calls == 1u && settings_boot_calls == 0u);
    meter_execution_stop();
    app_wait_hook = finish_boot_stop;
}
int main(int argc, char **argv)
{
    assert(argc == 3);
    lifecycle_test = true;
    const unsigned failure = (unsigned)strtoul(argv[2], NULL, 10);
    meter_core_storage_t bound = {.signals = domain, .signal_capacity = 1u};
    assert(meter_core_init(&core, &catalog, &bound));
    meter_diagnostics_init(&diagnostics);
    meter_execution_config_t settings = {
        .product = &product,
        .core = &core,
        .public_diagnostics = &diagnostics,
        .published_storage = {.signals = presentation, .signal_capacity = 1u},
        .diagnostic_storage = {.signals = diagnostic, .signal_capacity = 1u}};
    if (strcmp(argv[1], "init") == 0)
    {
        fail_init = failure;
        assert(!meter_execution_start(&settings));
        assert(init_calls == failure && startup_calls == 0u && wake_bindings == 0u);
        assert(!initialized); /* Earlier initialized native objects are retained: D-03, not rollback. */
        return 0;
    }
    if (strcmp(argv[1], "app-start") == 0)
    {
        fail_startup = 1u;
        assert(!meter_execution_start(&settings));
        assert(init_calls == 21u && initialized && wake_bindings == 1u);
        assert(shared.state == METER_EXEC_FAILED && startup_calls == 1u);
        assert(!meter_execution_start(&settings));
        return 0;
    }
    if (strcmp(argv[1], "durability") == 0 || strncmp(argv[1], "boot-", 5) == 0)
        storage_profile.enabled = true;
    if (strcmp(argv[1], "nvm-start") == 0)
    {
        storage_profile.enabled = true;
        nvm_start_ok = false;
    }
    assert(meter_execution_start(&settings));
    assert(init_calls == 21u && startup_calls == 1u);
    assert(!meter_execution_start(&settings));
    if (strcmp(argv[1], "worker") == 0)
        fail_startup = failure + 1u;
    app_wait_hook = finish_failed_start;
    if (strcmp(argv[1], "durability") == 0)
    {
        shared.protocol_ready = true;
        meter_execution_ui_stopped();
        assert(!shared.ui_done);
        app_wait_hook = begin_durability_stop;
    }
    if (strcmp(argv[1], "boot-settings") == 0 || strcmp(argv[1], "boot-stop") == 0)
    {
        nvm_ready = false;
        app_wait_hook = strcmp(argv[1], "boot-settings") == 0 ? complete_boot_scan : cancel_boot_scan;
    }
    app_entry(NULL);
    assert(meter_execution_stopped() && shared.ui_done && close_calls == 1u);
    assert(!meter_execution_start(&settings)); /* STOPPED is not a restart contract. */
    if (strcmp(argv[1], "worker") == 0)
        assert(startup_calls == failure + 1u);
    if (strcmp(argv[1], "nvm-start") == 0)
        assert(startup_calls == 1u);
    if (strcmp(argv[1], "durability") == 0)
        assert(waits == 7u && flush_calls == 2u);
    if (strcmp(argv[1], "boot-stop") == 0)
        assert(startup_calls == 1u && settings_boot_calls == 0u);
    return 0;
}

uint32_t meter_board_now_ms(void)
{
    return rt_tick_get_millisecond();
}
