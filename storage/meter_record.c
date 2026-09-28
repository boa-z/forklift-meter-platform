#include "storage/meter_record.h"
#include "third_party/CANopenNode/301/crc16-ccitt.h"
#include <string.h>

static void put16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
}
static void put32(uint8_t *data, uint32_t value)
{
    put16(data, (uint16_t)value);
    put16(data + 2u, (uint16_t)(value >> 16u));
}
static void put64(uint8_t *data, uint64_t value)
{
    put32(data, (uint32_t)value);
    put32(data + 4u, (uint32_t)(value >> 32u));
}
static uint16_t get16(const uint8_t *data)
{
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8u);
}
static uint32_t get32(const uint8_t *data)
{
    return (uint32_t)get16(data) | ((uint32_t)get16(data + 2u) << 16u);
}
static uint64_t get64(const uint8_t *data)
{
    return (uint64_t)get32(data) | ((uint64_t)get32(data + 4u) << 32u);
}

static bool size_valid(size_t payload_size)
{
    return payload_size <= UINT32_MAX &&
           payload_size <= (SIZE_MAX - METER_RECORD_HEADER_SIZE - METER_RECORD_TRAILER_SIZE);
}

meter_record_result_t meter_record_encode(const meter_record_view_t *view, uint8_t *output, size_t capacity,
                                          size_t *written)
{
    if ((view == NULL) || (output == NULL) || (written == NULL) || (view->type == 0u) ||
        (view->product_namespace == 0u) || (view->schema == 0u) || (view->flags != 0u) ||
        ((view->payload_size != 0u) && (view->payload == NULL)) || !size_valid(view->payload_size))
    {
        return METER_RECORD_RESULT_INVALID;
    }
    const size_t total = METER_RECORD_HEADER_SIZE + view->payload_size + METER_RECORD_TRAILER_SIZE;
    if (capacity < total)
    {
        return METER_RECORD_RESULT_CAPACITY;
    }
    memset(output, 0, total);
    put32(output, METER_RECORD_MAGIC);
    output[4] = METER_RECORD_VERSION;
    output[5] = 0u;
    put16(output + 6u, view->type);
    put16(output + 8u, view->product_namespace);
    put16(output + 10u, view->schema);
    put16(output + 12u, view->flags);
    put64(output + 14u, view->sequence);
    put32(output + 22u, (uint32_t)view->payload_size);
    if (view->payload_size != 0u)
    {
        memcpy(output + METER_RECORD_HEADER_SIZE, view->payload, view->payload_size);
    }
    const uint16_t crc = crc16_ccitt(output, 26u, 0u);
    const uint16_t payload_crc = crc16_ccitt(output + METER_RECORD_HEADER_SIZE, view->payload_size, crc);
    put16(output + 26u, payload_crc);
    /* 提交标记绑定代次、长度和完整性，不能复用上一代的固定 magic。 */
    uint8_t *seal = output + total - METER_RECORD_TRAILER_SIZE;
    put64(seal, view->sequence);
    put32(seal + 8u, (uint32_t)view->payload_size);
    put16(seal + 12u, payload_crc);
    put16(seal + 14u, (uint16_t)~payload_crc);
    *written = total;
    return METER_RECORD_RESULT_OK;
}

meter_record_result_t meter_record_decode(const uint8_t *input, size_t input_size,
                                          uint16_t expected_product_namespace, uint16_t expected_schema,
                                          meter_record_view_t *view)
{
    if ((input == NULL) || (view == NULL) ||
        (input_size < METER_RECORD_HEADER_SIZE + METER_RECORD_TRAILER_SIZE))
    {
        return METER_RECORD_RESULT_INVALID;
    }
    if ((get32(input) != METER_RECORD_MAGIC) || (input[4] != METER_RECORD_VERSION))
    {
        return METER_RECORD_RESULT_CORRUPT;
    }
    const uint32_t payload_size = get32(input + 22u);
    if (!size_valid(payload_size) ||
        (size_t)payload_size > input_size - METER_RECORD_HEADER_SIZE - METER_RECORD_TRAILER_SIZE)
    {
        return METER_RECORD_RESULT_CORRUPT;
    }
    const size_t total = METER_RECORD_HEADER_SIZE + (size_t)payload_size + METER_RECORD_TRAILER_SIZE;
    const uint16_t expected_crc =
        crc16_ccitt(input + METER_RECORD_HEADER_SIZE, payload_size, crc16_ccitt(input, 26u, 0u));
    const uint8_t *seal = input + total - METER_RECORD_TRAILER_SIZE;
    if (input[5] != 0u || get32(input + 28u) != 0u || get16(input + 6u) == 0u || get16(input + 12u) != 0u ||
        get16(input + 26u) != expected_crc || get64(seal) != get64(input + 14u) ||
        get32(seal + 8u) != payload_size || get16(seal + 12u) != expected_crc ||
        (uint16_t)(get16(seal + 14u) ^ expected_crc) != UINT16_MAX)
    {
        return METER_RECORD_RESULT_CORRUPT;
    }
    if ((get16(input + 8u) != expected_product_namespace) || (get16(input + 10u) != expected_schema))
    {
        return METER_RECORD_RESULT_INCOMPATIBLE;
    }
    *view = (meter_record_view_t){get16(input + 6u),  get16(input + 8u),  get16(input + 10u),
                                  get16(input + 12u), get64(input + 14u), input + METER_RECORD_HEADER_SIZE,
                                  payload_size};
    return METER_RECORD_RESULT_OK;
}
