#include "storage/hmi_eeprom.h"
static bool range(const hmi_eeprom_t *e, size_t offset, size_t size)
{
    return offset <= e->length && size <= e->length - offset;
}
static hmi_io_result_t read_bytes(void *ctx, size_t offset, uint8_t *data, size_t size)
{
    hmi_eeprom_t *e = ctx;
    if (!range(e, offset, size) || (!data && size))
        return HMI_IO_RANGE;
    while (size)
    {
        const size_t n = size < e->page_size ? size : e->page_size;
        const hmi_io_result_t r = e->port.read(e->port.context, e->base + offset, data, n);
        if (r != HMI_IO_OK)
            return r;
        offset += n;
        data += n;
        size -= n;
    }
    return HMI_IO_OK;
}
static hmi_io_result_t write_bytes(void *ctx, size_t offset, const uint8_t *data, size_t size)
{
    hmi_eeprom_t *e = ctx;
    if (!range(e, offset, size) || (!data && size))
        return HMI_IO_RANGE;
    while (size)
    {
        const size_t address = e->base + offset;
        const size_t remaining = e->page_size - address % e->page_size;
        const size_t n = size < remaining ? size : remaining;
        hmi_io_result_t r = e->port.write_page(e->port.context, address, data, n);
        if (r != HMI_IO_OK)
            return r;
        uint32_t waited = 0u;
        while ((r = e->port.ready(e->port.context)) != HMI_IO_OK)
        {
            if (r != HMI_IO_ERROR && r != HMI_IO_TIMEOUT)
                return r;
            if (waited >= e->write_timeout_ms)
                return HMI_IO_TIMEOUT;
            e->port.wait_ms(e->port.context, 1u);
            ++waited;
        }
        offset += n;
        data += n;
        size -= n;
    }
    return HMI_IO_OK;
}
static hmi_io_result_t sync_bytes(void *ctx)
{
    (void)ctx;
    /* 每页写已检查器件就绪，记录层仍必须读回才能宣称 durable。 */
    return HMI_IO_OK;
}
bool hmi_eeprom_init(hmi_eeprom_t *e, const hmi_eeprom_port_t *port, size_t capacity, size_t base,
                       size_t length, size_t page, uint32_t timeout)
{
    if (!e || !port || !port->read || !port->write_page || !port->ready || !port->wait_ms || !page ||
        !length || base > capacity || length > capacity - base || base % page || length % page || !timeout ||
        timeout > 1000u)
        return false;
    *e = (hmi_eeprom_t){*port,
                          capacity,
                          base,
                          length,
                          page,
                          timeout,
                          {e, read_bytes, write_bytes, sync_bytes, length, page, "eeprom", true}};
    return true;
}
