#include "platform/host/host_platform.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
bool meter_host_load(meter_core_t *core, const char *path)
{
    if (!path)
        return false;
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    uint8_t data[METER_SETTINGS_SIZE + 1];
    size_t n = fread(data, 1, sizeof(data), f);
    bool ok = !ferror(f);
    fclose(f);
    return ok && meter_settings_decode(core, data, n);
}
bool meter_host_save(const meter_core_t *core, const char *path)
{
    if (!path)
        return true;
    char temporary[1024];
    if (strlen(path) > sizeof(temporary) - 5)
        return false;
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    uint8_t data[METER_SETTINGS_SIZE];
    if (!meter_settings_encode(core, data))
        return false;
    FILE *f = fopen(temporary, "wb");
    if (!f)
        return false;
    bool ok = fwrite(data, 1, sizeof(data), f) == sizeof(data);
    if (fflush(f) != 0)
        ok = false;
    if (fclose(f) != 0)
        ok = false;
    if (!ok)
    {
        remove(temporary);
        return false;
    }
#ifdef _WIN32
    ok = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = rename(temporary, path) == 0;
#endif
    if (!ok)
        remove(temporary);
    return ok;
}
