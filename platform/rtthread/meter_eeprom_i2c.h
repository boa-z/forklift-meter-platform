#ifndef METER_EEPROM_I2C_H
#define METER_EEPROM_I2C_H
#include "storage/meter_eeprom.h"
struct rt_i2c_bus_device;
/** @brief 绑定 Kconfig 配置的两字节寻址 I2C EEPROM（地址、容量、页大小）；不探测、不写入。
 * @param port 输出端口，其回调仅由 NVM worker 调用。
 * @param bus 已注册的 RT-Thread I2C 总线，生命周期覆盖端口。
 */
bool meter_eeprom_i2c_bind(meter_eeprom_port_t *port, struct rt_i2c_bus_device *bus);
#endif
