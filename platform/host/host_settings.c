#include "platform/host/host_platform.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
/* 宿主以文件模拟板级 EEPROM：写入走临时文件加重命名，读者不会看到半截偏好。 */
bool meter_host_load(meter_core_t *core, const char *path, uint8_t *data, size_t capacity)
{
    if (!path || !data || capacity < meter_settings_size(core))
        return false;
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    size_t n = fread(data, 1, capacity, f);
    bool ok = !ferror(f);
    fclose(f);
    return ok && meter_settings_decode(core, data, n);
}
bool meter_host_save(const meter_core_t *core, const char *path, uint8_t *data, size_t capacity)
{
    if (!path)
        return true;
    char temporary[1024];
    if (strlen(path) > sizeof(temporary) - 5)
        return false;
    size_t length = meter_settings_size(core);
    if (!length || capacity < length || !meter_settings_encode(core, data, length))
        return false;
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    FILE *f = fopen(temporary, "wb");
    if (!f)
        return false;
    bool ok = fwrite(data, 1, length, f) == length;
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
