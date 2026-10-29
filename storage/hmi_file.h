#ifndef HMI_FILE_H
#define HMI_FILE_H
#include "storage/hmi_slots.h"
/** @brief 文件系统保证由 Board 声明；双文件不自动提供 FAT/NFTL 元数据掉电原子性。 */
typedef struct
{
    char prefix[192];
    size_t slot_size;
    hmi_nvm_io_t io;
} hmi_file_t;
/** @brief 绑定已挂载路径，不建目录、不格式化、不自动切换后端；每槽使用独立版本文件。 */
bool hmi_file_init(hmi_file_t *, const char *prefix, size_t slot_size, const char *backend_name,
                     bool proven_power_safe);
#endif
