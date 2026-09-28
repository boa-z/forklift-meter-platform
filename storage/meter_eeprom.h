#ifndef METER_EEPROM_H
#define METER_EEPROM_H
#include "storage/meter_slots.h"
/** @brief 页级端口封装现有 I2C 驱动；wait_ms 在事务外等待，不占总线锁。 */
typedef struct
{
    void *context;
    meter_io_result_t (*read)(void *, size_t, uint8_t *, size_t);
    meter_io_result_t (*write_page)(void *, size_t, const uint8_t *, size_t);
    meter_io_result_t (*ready)(void *);
    void (*wait_ms)(void *, uint32_t);
} meter_eeprom_port_t;
/** @brief Board 明确容量、页、分配区和写周期上界；不推断旧设备布局。 */
typedef struct
{
    meter_eeprom_port_t port;
    size_t device_capacity, base, length, page_size;
    uint32_t write_timeout_ms;
    meter_nvm_io_t io;
} meter_eeprom_t;
/** @brief 创建可靠页后端；初始化不探测、不写入介质。 */
bool meter_eeprom_init(meter_eeprom_t *, const meter_eeprom_port_t *, size_t capacity, size_t base,
                       size_t length, size_t page_size, uint32_t timeout_ms);
#endif
