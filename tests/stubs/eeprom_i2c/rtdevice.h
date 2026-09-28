#ifndef TEST_EEPROM_RTDEVICE_H
#define TEST_EEPROM_RTDEVICE_H
#include "rtthread.h"
#define RT_I2C_WR 0u
#define RT_I2C_RD 1u
struct rt_i2c_bus_device
{
    struct rt_mutex lock;
};
struct rt_i2c_msg
{
    uint16_t addr, flags, len;
    uint8_t *buf;
};
rt_size_t rt_i2c_transfer(struct rt_i2c_bus_device *bus, struct rt_i2c_msg *msg, rt_uint32_t count);
#endif
