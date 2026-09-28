#ifndef METER_FILE_H
#define METER_FILE_H
#include "storage/meter_slots.h"
/** @brief 文件系统保证由 Board 声明；双文件不自动提供 FAT/NFTL 元数据掉电原子性。 */
typedef struct
{
    char prefix[192];
    size_t slot_size;
    meter_nvm_io_t io;
} meter_file_t;
/** @brief 绑定已挂载路径，不建目录、不格式化、不自动切换后端；每槽使用独立版本文件。 */
bool meter_file_init(meter_file_t *, const char *prefix, size_t slot_size, const char *backend_name,
                     bool proven_power_safe);
#endif
