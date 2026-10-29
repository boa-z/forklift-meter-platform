#ifndef HMI_PACKAGE_H
#define HMI_PACKAGE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/** @brief 受限 CRC CPIO 包校验结果；只校验，不执行解包或 Flash 写入。 */
typedef enum
{
    HMI_PACKAGE_OK,
    HMI_PACKAGE_ARGUMENT,
    HMI_PACKAGE_HEADER,
    HMI_PACKAGE_MEMBER,
    HMI_PACKAGE_METADATA,
    HMI_PACKAGE_CAPACITY,
    HMI_PACKAGE_CHECKSUM,
    HMI_PACKAGE_PADDING,
    HMI_PACKAGE_TRUNCATED
} hmi_package_result_t;
/** @brief 调用方根据实际非活动分区提供容量，不从包内信任分区选择。 */
typedef struct
{
    uint32_t package_size, candidate_capacity;
    const char *os_file, *version;
} hmi_package_policy_t;
/** @brief 固定空间解析状态；没有堆分配，不缓存 OS 镜像。 */
typedef struct
{
    uint8_t header[110], name[32], metadata[512];
    char os_file[32], version[32];
    uint32_t package_size, capacity, received, remaining, checksum, expected_checksum;
    uint32_t file_size, os_size, os_offset, trailer_end, filled, name_size, padding;
    unsigned phase, member;
    hmi_package_result_t result;
} hmi_package_guard_t;
/** @brief 初始化受限配置；仅接受 ota_info.bin、指定 OS 文件、TRAILER!!! 的顺序。 */
bool hmi_package_init(hmi_package_guard_t *, const hmi_package_policy_t *);
/** @brief 增量校验任意切块；失败后保持失败，直到重新初始化。 */
hmi_package_result_t hmi_package_feed(hmi_package_guard_t *, const uint8_t *, size_t);
/** @brief 必须收到准确包长、尾标记与对齐后才能报告结构完整。 */
hmi_package_result_t hmi_package_finish(hmi_package_guard_t *);
/** @brief 稳定错误文本，供 CLI 和诊断显示。 */
const char *hmi_package_result_name(hmi_package_result_t);
#endif
