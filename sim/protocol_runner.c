#include "core/meter_core.h"
#include "runtime/meter_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Host 专用流式工具；按目录分配存储，固件不编译此文件。 */
static void quoted(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p)
        if (*p == '"' || *p == '\\') printf("\\%c", *p);
        else if (*p < 32) printf("\\u%04x", *p);
        else putchar(*p);
    putchar('"');
}
int main(int argc, char **argv)
{
    const meter_product_t *p = meter_product_get();
    const meter_catalog_t *cat = p->catalog;
    meter_core_storage_t storage = {
        calloc(cat->signal_count, sizeof(meter_value_t)), cat->signal_count,
        calloc(cat->parameter_count ? cat->parameter_count : 1, sizeof(float)), cat->parameter_count,
        calloc(cat->fault_count ? cat->fault_count : 1, sizeof(meter_fault_state_t)), cat->fault_count};
    meter_core_t core;
    meter_runtime_t runtime;
    if (!storage.signals || !storage.parameters || !storage.faults ||
        !meter_core_init_with_settings(&core, cat, &storage, p->initial_settings) ||
        !meter_runtime_init(&runtime, p, meter_core_apply, &core)) return 3;
    bool catalog_only = argc == 2 && !strcmp(argv[1], "--catalog");
    if (argc > 1 && !catalog_only) return 2;
    meter_runtime_connection(&runtime, true);
    meter_core_connection(&core, true, runtime.generation);
    char line[256], hex[32];
    unsigned now = 0, bus, id, extended, size, source, state;
    float value;
    while (!catalog_only && fgets(line, sizeof(line), stdin))
    {
        int used = 0;
        if (line[0] == 'F')
        {
            if (sscanf(line, "F %u %u %x %u %u %31s %n", &now, &bus, &id, &extended, &size, hex, &used) != 6 ||
                line[used] || bus > 1 || extended > 1 || size > 8 ||
                (size ? strlen(hex) != size * 2 : strcmp(hex, "-"))) return 2;
            meter_can_frame_t f = {.bus=(meter_bus_role_t)bus, .id=id, .timestamp_ms=now, .extended=extended, .size=(uint8_t)size};
            for (unsigned i = 0; i < size; ++i)
            {
                char pair[] = {hex[2*i], hex[2*i+1], 0}, *end;
                unsigned long b = strtoul(pair, &end, 16);
                if (*end || b > 255) return 2;
                f.data[i] = (uint8_t)b;
            }
            if (!meter_runtime_push(&runtime, &f)) return 2;
            meter_runtime_poll(&runtime, 1);
        }
        else if (line[0] == 'T')
        {
            if (sscanf(line, "T %u %n", &now, &used) != 1 || line[used]) return 2;
        }
        else if (line[0] == 'U')
        {
            if (sscanf(line, "U %u %f %u %u %u %n", &id, &value, &state, &now, &source, &used) != 5 ||
                line[used] || id > 65535 || source > 65535 || state > METER_VALUE_ERROR) return 2;
            meter_update_t u = {(uint16_t)id, {value, now, (meter_value_state_t)state, (uint16_t)source}};
            if (!meter_core_apply(&core, &u)) return 2;
        }
        else return 2;
        if (!meter_runtime_process(&runtime, now)) return 4;
        meter_core_tick(&core, now);
    }
    if (ferror(stdin)) return 2;
    if (p->evaluate) p->evaluate(&core.snapshot);
    printf("{\"product\":"); quoted(p->id);
    printf(",\"revision\":%u,\"dispatched\":%u,\"unrouted\":%u,\"decode_failed\":%u,\"signals\":{",
           (unsigned)core.snapshot.revision, (unsigned)runtime.diagnostics.dispatched,
           (unsigned)runtime.diagnostics.unrouted, (unsigned)runtime.diagnostics.decode_failed);
    for (size_t i = 0; i < cat->signal_count; ++i)
    {
        const meter_value_t v = core.snapshot.signals[i];
        const char *states[] = {"unknown", "valid", "stale", "error"};
        if (i) putchar(',');
        quoted(cat->signals[i].key);
        printf(":{\"id\":%u,\"value\":%.9g,\"state\":\"%s\",\"timestamp_ms\":%u,\"source\":%u,\"stale_ms\":%u,\"unit\":",
               cat->signals[i].id, (double)v.value, states[v.state], (unsigned)v.timestamp_ms,
               v.source, (unsigned)cat->signals[i].stale_ms);
        quoted(cat->signals[i].unit ? cat->signals[i].unit : ""); putchar('}');
    }
    puts("}}");
    free(storage.signals); free(storage.parameters); free(storage.faults);
    return runtime.diagnostics.decode_failed ? 4 : 0;
}
