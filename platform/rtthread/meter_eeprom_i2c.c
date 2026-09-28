#include "platform/rtthread/meter_eeprom_i2c.h"
#include <rtdevice.h>
#include <rtthread.h>
#define LOG_TAG "meter.nvm"
#define LOG_LVL LOG_LVL_WARNING
#include <ulog.h>

#define EEPROM_ADDRESS 0x50u
#define EEPROM_CAPACITY 65536u
#define EEPROM_PAGE_SIZE 128u
#define EEPROM_WRITE_MS 10
#define EEPROM_SETTLE_MS 1
#define EEPROM_READ_ATTEMPTS 3u

static bool valid_range(size_t address, size_t size)
{
    return address <= EEPROM_CAPACITY && size <= EEPROM_CAPACITY - address;
}

static meter_io_result_t read_byte(struct rt_i2c_bus_device *bus, size_t offset, uint8_t *data)
{
    for (unsigned attempt = 0u; attempt < EEPROM_READ_ATTEMPTS; ++attempt)
    {
        uint8_t bytes[2] = {(uint8_t)(offset >> 8u), (uint8_t)offset};
        uint8_t received_byte = 0u;
        struct rt_i2c_msg addr = {EEPROM_ADDRESS, RT_I2C_WR, 2, bytes};
        struct rt_i2c_msg value = {EEPROM_ADDRESS, RT_I2C_RD, 1, &received_byte};
        /* 给上一事务的控制器关闭/STOP 留出间隔；失败只重试完整地址/读取对。 */
        rt_thread_mdelay(EEPROM_SETTLE_MS);
        if (rt_mutex_take(&bus->lock, RT_WAITING_FOREVER) != RT_EOK)
            return METER_IO_ERROR;
        const rt_size_t sent = rt_i2c_transfer(bus, &addr, 1);
        rt_size_t received = 0u;
        if (sent == 1u)
        {
            /* 地址和读取不能被插入其他访问，因此此处短暂持递归总线锁。 */
            rt_thread_mdelay(EEPROM_SETTLE_MS);
            received = rt_i2c_transfer(bus, &value, 1);
        }
        const rt_err_t released = rt_mutex_release(&bus->lock);
        if (released != RT_EOK)
            return METER_IO_ERROR;
        if (sent == 1u && received == 1u)
        {
            *data = received_byte;
            return METER_IO_OK;
        }
        LOG_W("EEPROM read addr=0x%04x attempt=%u/%u address=%u read=%u", (unsigned)offset, attempt + 1u,
              EEPROM_READ_ATTEMPTS, (unsigned)sent, (unsigned)received);
    }
    return METER_IO_ERROR;
}

static meter_io_result_t read_bytes(void *context, size_t address, uint8_t *data, size_t size)
{
    struct rt_i2c_bus_device *bus = context;
    if (!bus)
        return METER_IO_ERROR;
    if (!valid_range(address, size) || (!data && size))
        return METER_IO_RANGE;
    for (size_t i = 0u; i < size; ++i)
    {
        const meter_io_result_t result = read_byte(bus, address + i, data + i);
        if (result != METER_IO_OK)
            return result;
    }
    return METER_IO_OK;
}

static meter_io_result_t write_page(void *context, size_t address, const uint8_t *data, size_t size)
{
    struct rt_i2c_bus_device *bus = context;
    if (!bus)
        return METER_IO_ERROR;
    if (!valid_range(address, size) || (!data && size) ||
        size > EEPROM_PAGE_SIZE - address % EEPROM_PAGE_SIZE)
        return METER_IO_RANGE;
    for (size_t i = 0u; i < size; ++i)
    {
        const size_t offset = address + i;
        uint8_t bytes[3] = {(uint8_t)(offset >> 8u), (uint8_t)offset, data[i]};
        struct rt_i2c_msg msg = {EEPROM_ADDRESS, RT_I2C_WR, 3, bytes};
        /* 写入不重放；短传输交由上层 UNCERTAIN 和显式协调处理。 */
        rt_thread_mdelay(EEPROM_SETTLE_MS);
        const rt_size_t sent = rt_i2c_transfer(bus, &msg, 1);
        /* 即使驱动报告失败，也等待可能已经开始的写周期；此时不持总线锁。 */
        rt_thread_mdelay(EEPROM_WRITE_MS);
        if (sent != 1u)
        {
            LOG_W("EEPROM byte write addr=0x%04x messages=%u/1", (unsigned)offset, (unsigned)sent);
            return METER_IO_ERROR;
        }
        uint8_t actual = 0u;
        const meter_io_result_t result = read_bytes(context, offset, &actual, 1u);
        /* 驱动成功不代表写入成功；WP 或吞掉的 NACK 在读回不一致时立即终止。 */
        if (result != METER_IO_OK || actual != data[i])
        {
            LOG_W("EEPROM byte verify addr=0x%04x result=%u", (unsigned)offset, (unsigned)result);
            return METER_IO_ERROR;
        }
    }
    return METER_IO_OK;
}

static meter_io_result_t ready(void *context)
{
    uint8_t value;
    return read_bytes(context, 0u, &value, 1u);
}

static void wait_ms(void *context, uint32_t ms)
{
    (void)context;
    rt_thread_mdelay((rt_int32_t)ms);
}

bool meter_eeprom_i2c_bind(meter_eeprom_port_t *port, struct rt_i2c_bus_device *bus)
{
    if (!port || !bus)
        return false;
    *port = (meter_eeprom_port_t){bus, read_bytes, write_page, ready, wait_ms};
    return true;
}
