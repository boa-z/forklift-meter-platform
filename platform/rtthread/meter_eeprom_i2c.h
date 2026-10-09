#ifndef METER_EEPROM_I2C_H
#define METER_EEPROM_I2C_H
#include "storage/meter_eeprom.h"
struct rt_i2c_bus_device;
/** @brief 绑定 reference-board 的 0x50 EEPROM 字节兼容端口；不探测、不写入。
 * @param port 输出端口，其回调仅由 NVM worker 调用。
 * @param bus 已注册的 RT-Thread I2C 总线，生命周期覆盖端口。
 * @return 参数有效时返回 true。容量为 64 KiB，物理页为 128 字节。
 */
bool meter_eeprom_i2c_bind(meter_eeprom_port_t *port, struct rt_i2c_bus_device *bus);
#endif
