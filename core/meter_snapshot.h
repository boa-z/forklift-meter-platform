#ifndef METER_SNAPSHOT_H
#define METER_SNAPSHOT_H
#include "contracts/meter_domain.h"

/** @brief RT-UI-01：复制失败原因；失败时目标描述符和数组均不变。 */
typedef enum
{
    METER_SNAPSHOT_COPIED,
    METER_SNAPSHOT_INVALID,
    METER_SNAPSHOT_CAPACITY,
    METER_SNAPSHOT_ALIAS
} meter_snapshot_result_t;

/**
 * @brief RT-UI-01：将完整域状态复制到调用方提供的独立数组，不申请内存。
 * @param destination 调用方拥有的快照描述符，成功后引用 storage；不能与源/数组重叠。
 * @param storage 长期有效的可写数组及真实容量；数组之间或与源不允许重叠。
 * @param source 完整源快照；catalog 及其表为只读共享，整个调用期不可变。
 * @return COPIED 表示完成复制；其他结果不修改目标，容量不足为 CAPACITY。
 * @details App 发布或 UI 取副本时由平台短 mutex 保护调用，禁止 ISR 并发调用；
 * 函数同步有界、不等待 I/O、不调用 Product/LVGL，无取消点。参数必须是有效对象。
 * 指针地址比较依赖目标 uintptr_t 的平坦地址模型；RV32 和 Host 均需在构建记录核对。
 */
meter_snapshot_result_t meter_snapshot_copy(meter_snapshot_t *destination,
                                           const meter_core_storage_t *storage,
                                           const meter_snapshot_t *source);
#endif
