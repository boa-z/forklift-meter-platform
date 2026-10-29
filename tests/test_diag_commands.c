#include "platform/common/hmi_diag_commands.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char output[8192];
static void emit(void *ctx, const char *text)
{
    (void)ctx;
    assert(strlen(output) + strlen(text) + 2 < sizeof(output));
    strcat(output, text);
    strcat(output, "\n");
}
static void run(const char *name, const char *arg, const hmi_diag_view_t *v)
{
    const char *argv[] = {"hmi", name, arg};
    hmi_diag_query_t q;
    assert(hmi_diag_query(arg ? 3 : 2, argv, &q));
    output[0] = 0;
    hmi_diag_render(&q, v, emit, NULL);
    assert(*output);
}
int main(void)
{
    hmi_diag_snapshot_t snapshot = {.uptime_ms = 12};
    hmi_build_info_t info = {"demo", "abc", "sdk", "9.1.0", "board", "date", "time", "fw-1", "Product-1.0-V1"};
    hmi_diag_signal_t signal = {.id = 3,
                                  .key = "vehicle.speed",
                                  .unit = "m/s",
                                  .value = {1.25f, 10, HMI_VALUE_VALID, 7},
                                  .age_ms = 2,
                                  .stale_ms = 50};
    hmi_trace_entry_t entries[] = {{UINT32_MAX, HMI_TRACE_CAN, CAN_RX_DROP, 17, 0},
                                     {2, HMI_TRACE_CAN, CAN_ERROR, 17, 1}};
    hmi_diag_view_t view = {.snapshot = &snapshot,
                              .build = &info,
                              .signal = &signal,
                              .entries = entries,
                              .entry_count = 2,
                              .cleared = 2};
    const char *names[] = {"info", "diag", "runtime", "can", "domain", "touch", "trace", "storage"};
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        run(names[i], NULL, &view);
    run("info", NULL, &view);
    assert(strstr(output, "platform : abc") && strstr(output, "uptime_ms : 12"));
    assert(strstr(output, "firmware_version : fw-1") && strstr(output, "display_version : Product-1.0-V1"));
    run("signal", "vehicle.speed", &view);
    assert(strstr(output, "value=1.25") && strstr(output, "state=VALID") && strstr(output, "source=7"));
    run("trace", "dump", &view);
    assert(strstr(output, "4294967295 CAN RX_DROP") < strstr(output, "2 CAN ERROR"));
    run("trace", "clear", &view);
    assert(strstr(output, "cleared=2 result=OK"));
    snapshot.can[1] = (hmi_diag_can_t){.available = true, .rx_seen = true, .last_rx_ms = UINT32_MAX - 2};
    run("can", "1", &view);
    assert(strstr(output, "last_rx_age_ms=15") && strstr(output, "bitrate=unavailable") &&
           !strstr(output, "can0"));
    const char *retired[] = {"hmi", "pdo"};
    hmi_diag_query_t retired_query;
    assert(!hmi_diag_query(2, retired, &retired_query));
    snapshot.storage.available = true;
    snapshot.storage.backend = "eeprom";
    snapshot.storage.state = 5;
    snapshot.storage.ram_revision = UINT64_C(4294967297);
    snapshot.storage.durable_revision = UINT64_C(4294967297);
    run("storage", NULL, &view);
    assert(strstr(output, "state=DURABLE") && strstr(output, "durable_revision=4294967297"));
    hmi_diag_snapshot_t before = snapshot;
    run("diag", NULL, &view);
    assert(memcmp(&before, &snapshot, sizeof(snapshot)) == 0);
    const char *invalid[] = {"hmi", "can", "2"};
    hmi_diag_query_t q;
    assert(!hmi_diag_query(3, invalid, &q));
    invalid[1] = "signal";
    assert(!hmi_diag_query(2, invalid, &q));
    invalid[1] = "storage";
    invalid[2] = "write";
    assert(!hmi_diag_query(3, invalid, &q));
    invalid[1] = "trace";
    invalid[2] = "erase";
    assert(!hmi_diag_query(3, invalid, &q));
    view.signal = NULL;
    run("signal", "missing", &view);
    assert(strstr(output, "signal not found"));
    puts("diagnostics argv dispatch, snapshot rendering, invalid inputs and wrap-safe ages PASS");
    return 0;
}
