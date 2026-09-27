#ifndef METER_SETTINGS_H
#define METER_SETTINGS_H
#include <stddef.h>
#include "core/meter_core.h"
/* 4 字节魔数、4 字节状态字段与末尾校验和，中间是每个参数一个 4 字节浮点，
 * 因此整块大小由产品目录决定，而不是固定的平台容量。 */
#define METER_SETTINGS_OVERHEAD 12u
/**
 * @brief 当前 core 写出设置所需的字节数。
 *
 * 参数超过 255 个时返回 0，表示需要评审新的带版本格式；调用方据此分配静态缓冲区，
 * 不要把返回值当作固定容量。
 */
size_t meter_settings_size(const meter_core_t *core);
/** @brief 编码到调用方缓冲区；size 不足时返回 false 且不写出半截数据。 */
bool meter_settings_encode(const meter_core_t *core, uint8_t *out, size_t size);
/**
 * @brief 校验并整体提交设置数据。
 *
 * 先按文件内的参数个数核对长度与校验和，再逐项校验值域，全部通过后才写入 core，
 * 因此被拒绝的文件不会留下半应用状态。在 UI 线程调用安全：不含阻塞 IO。
 */
bool meter_settings_decode(meter_core_t *core, const uint8_t *data, size_t size);
#endif
