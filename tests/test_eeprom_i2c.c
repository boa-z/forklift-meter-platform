#include "platform/rtthread/meter_eeprom_i2c.h"
#include <rtdevice.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(x))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "EEPROM:%d: %s\n", __LINE__, #x);                                                \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static struct rt_i2c_bus_device bus;
static uint8_t media[65536];
static size_t pointer;
static unsigned calls, fail_call, fail_from, writes, delay_ms, warnings;
static unsigned between_address_ms, outside_delay_ms;
static bool address_pending;
static bool protect, fail_lock, fail_unlock, corrupt_read;
void test_eeprom_log(const char *format, ...)
{
    (void)format;
    ++warnings;
}
rt_err_t rt_mutex_take(struct rt_mutex *mutex, rt_int32_t timeout)
{
    CHECK(timeout == RT_WAITING_FOREVER);
    if (fail_lock)
        return -1;
    ++mutex->depth;
    return RT_EOK;
}
rt_err_t rt_mutex_release(struct rt_mutex *mutex)
{
    CHECK(mutex->depth > 0u);
    --mutex->depth;
    return fail_unlock && mutex->depth == 0u ? -1 : RT_EOK;
}
rt_err_t rt_thread_mdelay(rt_int32_t ms)
{
    CHECK(ms > 0);
    if (bus.lock.depth == 1u)
    {
        CHECK(ms == 1 && address_pending);
        between_address_ms += (unsigned)ms;
    }
    else
    {
        CHECK(bus.lock.depth == 0u);
        outside_delay_ms += (unsigned)ms;
    }
    delay_ms += (unsigned)ms;
    return RT_EOK;
}
rt_size_t rt_i2c_transfer(struct rt_i2c_bus_device *device, struct rt_i2c_msg *msg, rt_uint32_t count)
{
    CHECK(device == &bus && count == 1u && msg->addr == 0x50u);
    CHECK(rt_mutex_take(&device->lock, RT_WAITING_FOREVER) == RT_EOK);
    ++calls;
    rt_size_t result = 1u;
    if (calls == fail_call || (fail_from != 0u && calls >= fail_from))
        result = 0u;
    else if (msg->flags == RT_I2C_RD)
    {
        CHECK(msg->len == 1u && device->lock.depth == 2u);
        CHECK(address_pending && between_address_ms >= 1u);
        address_pending = false;
        msg->buf[0] = (uint8_t)(media[pointer] ^ (corrupt_read ? 1u : 0u));
    }
    else
    {
        CHECK(msg->flags == RT_I2C_WR && (msg->len == 2u || msg->len == 3u));
        pointer = ((size_t)msg->buf[0] << 8u) | msg->buf[1];
        if (msg->len == 2u)
        {
            CHECK(device->lock.depth == 2u && outside_delay_ms >= 1u);
            address_pending = true;
            between_address_ms = outside_delay_ms = 0u;
        }
        else
        {
            CHECK(device->lock.depth == 1u && outside_delay_ms >= 1u);
            outside_delay_ms = 0u;
            ++writes;
            if (!protect)
                media[pointer] = msg->buf[2];
        }
    }
    (void)rt_mutex_release(&device->lock);
    return result;
}
static void reset(void)
{
    CHECK(bus.lock.depth == 0u);
    memset(media, 0xFF, sizeof(media));
    pointer = 0u;
    calls = fail_call = fail_from = writes = delay_ms = warnings = 0u;
    between_address_ms = outside_delay_ms = 0u;
    address_pending = false;
    protect = fail_lock = fail_unlock = corrupt_read = false;
}
int main(void)
{
    meter_eeprom_port_t port;
    uint8_t data[128], readback[128];
    for (size_t i = 0u; i < sizeof(data); ++i)
        data[i] = (uint8_t)i;
    CHECK(!meter_eeprom_i2c_bind(NULL, &bus));
    CHECK(!meter_eeprom_i2c_bind(&port, NULL));
    CHECK(meter_eeprom_i2c_bind(&port, &bus));
    CHECK(calls == 0u);
    reset();
    CHECK(port.write_page(port.context, 0x1200u, data, sizeof(data)) == METER_IO_OK);
    CHECK(writes == 128u && calls == 384u && delay_ms == 1664u);
    CHECK(media[0x11FF] == 0xFFu && media[0x1280] == 0xFFu);
    CHECK(port.read(port.context, 0x1200u, readback, sizeof(readback)) == METER_IO_OK);
    CHECK(memcmp(data, readback, sizeof(data)) == 0 && bus.lock.depth == 0u);
    CHECK(port.ready(port.context) == METER_IO_OK && writes == 128u);
    port.wait_ms(port.context, 1u);
    CHECK(delay_ms == 1923u);
    reset();
    CHECK(port.write_page(port.context, 65535u, data, 1u) == METER_IO_OK);
    CHECK(media[65535] == data[0]);
    reset();
    CHECK(port.write_page(port.context, 127u, data, 2u) == METER_IO_RANGE);
    CHECK(port.write_page(port.context, 65535u, data, 2u) == METER_IO_RANGE);
    CHECK(port.read(port.context, SIZE_MAX, readback, 1u) == METER_IO_RANGE);
    CHECK(port.read(port.context, 65535u, readback, 2u) == METER_IO_RANGE);
    CHECK(port.read(port.context, 0u, NULL, 1u) == METER_IO_RANGE);
    CHECK(port.write_page(port.context, 0u, NULL, 1u) == METER_IO_RANGE);
    CHECK(port.read(NULL, 0u, readback, 1u) == METER_IO_ERROR);
    CHECK(port.write_page(NULL, 0u, data, 1u) == METER_IO_ERROR);
    CHECK(port.read(port.context, 65536u, NULL, 0u) == METER_IO_OK);
    CHECK(port.write_page(port.context, 65536u, NULL, 0u) == METER_IO_OK);
    CHECK(calls == 0u);
    for (unsigned failure = 1u; failure <= 6u; ++failure)
    {
        reset();
        fail_from = failure;
        CHECK(port.write_page(port.context, 128u, data, 3u) == METER_IO_ERROR);
        CHECK(calls == failure + ((failure == 1u || failure == 4u) ? 0u : 2u));
        CHECK(bus.lock.depth == 0u && warnings > 0u);
        CHECK(media[130] == 0xFFu);
    }
    for (unsigned failure = 1u; failure <= 4u; ++failure)
    {
        reset();
        fail_from = failure;
        CHECK(port.read(port.context, 0u, readback, 3u) == METER_IO_ERROR);
        CHECK(calls == failure + 2u && bus.lock.depth == 0u && warnings == 3u);
    }
    for (unsigned failure = 1u; failure <= 4u; ++failure)
    {
        reset();
        fail_call = failure;
        media[0] = 0x12u;
        media[1] = 0x34u;
        CHECK(port.read(port.context, 0u, readback, 2u) == METER_IO_OK);
        CHECK(readback[0] == 0x12u && readback[1] == 0x34u);
        CHECK(warnings == 1u && writes == 0u && bus.lock.depth == 0u);
    }
    reset();
    fail_call = 3u;
    CHECK(port.write_page(port.context, 0u, data, 1u) == METER_IO_OK);
    CHECK(writes == 1u && calls == 5u && warnings == 1u);
    reset();
    fail_call = 1u;
    CHECK(port.write_page(port.context, 0u, data, 1u) == METER_IO_ERROR);
    CHECK(calls == 1u && delay_ms == 11u);
    reset();
    fail_from = 2u;
    readback[0] = 0xA5u;
    CHECK(port.read(port.context, 0u, readback, 1u) == METER_IO_ERROR);
    CHECK(readback[0] == 0xA5u && calls == 4u);
    reset();
    protect = true;
    CHECK(port.write_page(port.context, 0u, data, 2u) == METER_IO_ERROR);
    CHECK(writes == 1u && media[0] == 0xFFu && calls == 3u);
    reset();
    corrupt_read = true;
    CHECK(port.write_page(port.context, 0u, data, 2u) == METER_IO_ERROR);
    CHECK(writes == 1u && calls == 3u);
    reset();
    fail_lock = true;
    CHECK(port.read(port.context, 0u, readback, 1u) == METER_IO_ERROR);
    CHECK(calls == 0u && bus.lock.depth == 0u);
    reset();
    fail_unlock = true;
    CHECK(port.read(port.context, 0u, readback, 1u) == METER_IO_ERROR);
    CHECK(calls == 2u && bus.lock.depth == 0u);
    reset();
    meter_eeprom_t eeprom;
    CHECK(meter_eeprom_init(&eeprom, &port, 65536u, 0u, 1024u, 128u, 10u));
    CHECK(eeprom.io.write(eeprom.io.context, 120u, data, sizeof(data)) == METER_IO_OK);
    CHECK(memcmp(media + 120u, data, sizeof(data)) == 0);
    CHECK(eeprom.io.write(eeprom.io.context, 1024u, data, 1u) == METER_IO_RANGE);
    CHECK(media[1024] == 0xFFu && bus.lock.depth == 0u);
    puts("EEPROM byte transport: PASS");
    return 0;
}
