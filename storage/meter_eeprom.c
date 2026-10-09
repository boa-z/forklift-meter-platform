#include "storage/meter_eeprom.h"
static bool range(const meter_eeprom_t *e, size_t offset, size_t size)
{
    return offset <= e->length && size <= e->length - offset;
}
static meter_io_result_t read_bytes(void *ctx, size_t offset, uint8_t *data, size_t size)
{
    meter_eeprom_t *e = ctx;
    if (!range(e, offset, size) || (!data && size))
        return METER_IO_RANGE;
    while (size)
    {
        const size_t n = size < e->page_size ? size : e->page_size;
        const meter_io_result_t r = e->port.read(e->port.context, e->base + offset, data, n);
        if (r != METER_IO_OK)
            return r;
        offset += n;
        data += n;
        size -= n;
    }
    return METER_IO_OK;
}
static meter_io_result_t write_bytes(void *ctx, size_t offset, const uint8_t *data, size_t size)
{
    meter_eeprom_t *e = ctx;
    if (!range(e, offset, size) || (!data && size))
        return METER_IO_RANGE;
    while (size)
    {
        const size_t address = e->base + offset;
        const size_t remaining = e->page_size - address % e->page_size;
        const size_t n = size < remaining ? size : remaining;
        meter_io_result_t r = e->port.write_page(e->port.context, address, data, n);
        if (r != METER_IO_OK)
            return r;
        uint32_t waited = 0u;
        while ((r = e->port.ready(e->port.context)) != METER_IO_OK)
        {
            if (r != METER_IO_ERROR && r != METER_IO_TIMEOUT)
                return r;
            if (waited >= e->write_timeout_ms)
                return METER_IO_TIMEOUT;
            e->port.wait_ms(e->port.context, 1u);
            ++waited;
        }
        offset += n;
        data += n;
        size -= n;
    }
    return METER_IO_OK;
}
static meter_io_result_t sync_bytes(void *ctx)
{
    (void)ctx;
    /* 每页写已检查器件就绪，记录层仍必须读回才能宣称 durable。 */
    return METER_IO_OK;
}
bool meter_eeprom_init(meter_eeprom_t *e, const meter_eeprom_port_t *port, size_t capacity, size_t base,
                       size_t length, size_t page, uint32_t timeout)
{
    if (!e || !port || !port->read || !port->write_page || !port->ready || !port->wait_ms || !page ||
        !length || base > capacity || length > capacity - base || base % page || length % page || !timeout ||
        timeout > 1000u)
        return false;
    *e = (meter_eeprom_t){*port,
                          capacity,
                          base,
                          length,
                          page,
                          timeout,
                          {e, read_bytes, write_bytes, sync_bytes, length, page, "eeprom", true}};
    return true;
}
