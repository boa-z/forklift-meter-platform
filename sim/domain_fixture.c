#include "sim/domain_fixture.h"
#include <stdio.h>
#include <math.h>
bool meter_fixture_load(meter_core_t *core, const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file) return false;
    char line[256];
    bool ok = true;
    while (fgets(line, sizeof(line), file))
    {
        unsigned id, state, now, source;
        float value;
        int used = 0;
        if (sscanf(line, "U %u %f %u %u %u %n", &id, &value, &state, &now, &source, &used) != 5 ||
            line[used] || id > 65535 || state > METER_VALUE_ERROR || source > 65535 || !isfinite(value))
        { ok = false; break; }
        meter_update_t u = {(uint16_t)id, {value, now, (meter_value_state_t)state, (uint16_t)source}};
        if (!meter_core_apply(core, &u)) { ok = false; break; }
    }
    ok = ok && !ferror(file);
    fclose(file);
    return ok;
}
