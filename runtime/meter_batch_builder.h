#ifndef METER_BATCH_BUILDER_H
#define METER_BATCH_BUILDER_H
#include "contracts/meter_batch.h"

/** @brief Protocol 单 owner 的暂存，不是事件总线；静态 storage 由 Product 定容。 */
typedef struct
{
    meter_update_batch_t batch;
    meter_update_t *storage;
    size_t capacity;
    bool active;
    meter_batch_result_t error;
} meter_batch_builder_t;

/** @brief 启动期绑定独占数组；容量非零，不分配、不阻塞；无效参数不改变对象。 */
bool meter_batch_builder_init(meter_batch_builder_t *builder, meter_update_t *storage, size_t capacity);

/** @brief RT-APP-01：开始一帧，metadata 中 updates/count 必须为空/零；已有帧则拒绝。 */
bool meter_batch_builder_begin(meter_batch_builder_t *builder, const meter_update_batch_t *metadata);

/** @brief 作为解码 sink 复制值；溢出/重复/来源不符锁存整批失败，不容许截断后投递。 */
bool meter_batch_builder_add(void *context, const meter_update_t *update);

/**
 * @brief 结束一帧并最多提交一次完整批次；解码失败或容量错误不会调用 submit。
 * @details 仅 Protocol owner 调用，禁止 ISR；同步、有界、不分配；submit 必须立即复制或拒绝。
 * 调用返回后解除 active，拒绝批次不保留；调用方记录 IPC_FULL/数据缺口，不伪报 dispatched。
 * 没有信号的合法帧可正常结束而不提交空消息；元数据在下一次 begin 前有效。
 */
meter_batch_result_t meter_batch_builder_finish(meter_batch_builder_t *builder, bool decoded,
                                               meter_batch_submit_fn_t submit, void *context);
#endif
