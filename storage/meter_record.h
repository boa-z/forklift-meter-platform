#ifndef METER_RECORD_H
#define METER_RECORD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief FMP2 固定记录头；线上的字段按小端序编码，不直接持久化此 struct。 */
#define METER_RECORD_MAGIC UINT32_C(0x32504D46)
#define METER_RECORD_VERSION UINT8_C(2)
#define METER_RECORD_SEAL UINT32_C(0xA55A5AA5)
#define METER_RECORD_HEADER_SIZE 32u
#define METER_RECORD_TRAILER_SIZE 16u

typedef enum
{
    METER_RECORD_STATE_EMPTY,
    METER_RECORD_STATE_VALID,
    METER_RECORD_STATE_CORRUPT,
    METER_RECORD_STATE_INCOMPATIBLE,
    METER_RECORD_STATE_IO_ERROR
} meter_record_state_t;

typedef struct
{
    uint16_t type;
    uint16_t product_namespace;
    uint16_t schema;
    uint16_t flags;
    uint64_t sequence;
    const uint8_t *payload;
    size_t payload_size;
} meter_record_view_t;

/** @brief 记录操作结果；DURABLE 只由 backend 在同步/读回成功后产生。 */
typedef enum
{
    METER_RECORD_RESULT_OK,
    METER_RECORD_RESULT_INVALID,
    METER_RECORD_RESULT_CAPACITY,
    METER_RECORD_RESULT_INCOMPATIBLE,
    METER_RECORD_RESULT_CORRUPT
} meter_record_result_t;

/**
 * @brief 编码完整 FMP2 记录。
 * @details 仅编码内存副本，不做介质 I/O；payload 不可为 NULL（除非 size 为 0）。
 * 输出容量不足或数值溢出时不写 output。CRC-16/CCITT/XMODEM（初值 0）覆盖头（不含 CRC 字段）和 payload。
 */
meter_record_result_t meter_record_encode(const meter_record_view_t *view, uint8_t *output, size_t capacity,
                                          size_t *written);

/**
 * @brief 校验并读取 FMP2 记录。
 * @details 先完整验证 magic/version/长度/CRC/seal，再返回 payload 视图；payload 指向 input，
 * 调用方必须保持 input 生命周期。product/schema 由调用方显式核对，不自动应用记录。
 */
meter_record_result_t meter_record_decode(const uint8_t *input, size_t input_size,
                                          uint16_t expected_product_namespace, uint16_t expected_schema,
                                          meter_record_view_t *view);
#endif
