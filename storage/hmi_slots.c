#include "storage/hmi_slots.h"
#include <string.h>

bool hmi_slots_init(hmi_slots_t *s, uint8_t *media, size_t size, uint8_t *scratch, size_t capacity)
{
    if (!s || !media || !scratch || size % 2u || size / 2u > capacity ||
        size / 2u < HMI_RECORD_HEADER_SIZE + 2u * HMI_RECORD_TRAILER_SIZE)
        return false;
    memset(s, 0, sizeof(*s));
    s->media = media;
    s->media_size = size;
    s->scratch = scratch;
    s->scratch_size = capacity;
    s->slot_size = size / 2u;
    s->seal_page = HMI_RECORD_TRAILER_SIZE;
    return true;
}
bool hmi_slots_bind(hmi_slots_t *s, const hmi_nvm_io_t *io)
{
    if (!s || !io || !io->read || !io->write || !io->sync || io->capacity < s->media_size ||
        io->page_size < HMI_RECORD_TRAILER_SIZE || s->slot_size % io->page_size ||
        io->page_size > s->slot_size / 2u)
        return false;
    s->io = io;
    s->seal_page = io->page_size;
    s->writable = false;
    return true;
}
static bool blank(const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; ++i)
        if (p[i] != 0xFFu)
            return false;
    return true;
}
hmi_slots_result_t hmi_slots_scan(hmi_slots_t *s, uint16_t ns, uint16_t schema)
{
    if (!s || !s->media || !ns || !schema)
        return HMI_SLOTS_INVALID;
    s->has_active = false;
    s->writable = false;
    s->degraded = false;
    if (s->io)
    {
        s->last_error = s->io->read(s->io->context, 0u, s->media, s->media_size);
        if (s->last_error != HMI_IO_OK)
            return HMI_SLOTS_IO_ERROR;
    }
    unsigned empty = 0u, valid = 0u;
    bool incompatible = false;
    for (size_t i = 0; i < 2u; ++i)
    {
        const uint8_t *p = s->media + i * s->slot_size;
        if (blank(p, s->slot_size))
        {
            ++empty;
            continue;
        }
        hmi_record_view_t v;
        hmi_record_result_t r = hmi_record_decode(p, s->slot_size - s->seal_page, ns, schema, &v);
        if (r == HMI_RECORD_RESULT_INCOMPATIBLE || (!memcmp(p, "FMP", 3u) && p[4] != HMI_RECORD_VERSION))
        {
            incompatible = true;
            continue;
        }
        if (r != HMI_RECORD_RESULT_OK)
            continue;
        const size_t seal = HMI_RECORD_HEADER_SIZE + v.payload_size;
        if (memcmp(p + seal, p + s->slot_size - s->seal_page, HMI_RECORD_TRAILER_SIZE))
            continue;
        ++valid;
        if (s->has_active && v.sequence == s->active.sequence &&
            (v.type != s->active.type || v.payload_size != s->active.payload_size ||
             memcmp(v.payload, s->active.payload, v.payload_size)))
        {
            s->has_active = false;
            return HMI_SLOTS_CONFLICT;
        }
        if (!s->has_active || v.sequence > s->active.sequence)
        {
            s->has_active = true;
            s->active = v;
            s->active_slot = i;
        }
    }
    if (incompatible)
    {
        s->has_active = false;
        return HMI_SLOTS_INCOMPATIBLE;
    }
    s->degraded = valid == 1u && empty == 0u;
    s->writable = s->has_active || empty == 2u;
    return s->has_active ? HMI_SLOTS_OK : (empty == 2u ? HMI_SLOTS_EMPTY : HMI_SLOTS_NO_VALID_SLOT);
}
static bool write_part(hmi_slots_t *s, size_t offset, const uint8_t *p, size_t n, size_t *remaining)
{
    size_t count = n < *remaining ? n : *remaining;
    if (s->io && count)
    {
        s->last_error = s->io->write(s->io->context, offset, p, count);
        if (s->last_error != HMI_IO_OK)
            return false;
    }
    if (count)
        memcpy(s->media + offset, p, count);
    if (*remaining != SIZE_MAX)
        *remaining -= count;
    return count == n;
}
static bool verify_part(hmi_slots_t *s, size_t offset, const uint8_t *p, size_t n)
{
    if (s->io)
    {
        s->last_error = s->io->sync(s->io->context);
        if (s->last_error != HMI_IO_OK)
            return false;
        s->last_error = s->io->read(s->io->context, offset, s->media + offset, n);
        if (s->last_error != HMI_IO_OK)
            return false;
    }
    if (memcmp(s->media + offset, p, n))
    {
        s->last_error = HMI_IO_VERIFY;
        return false;
    }
    return true;
}
hmi_slots_result_t hmi_slots_commit(hmi_slots_t *s, const hmi_record_view_t *record, size_t budget)
{
    if (!s || !record || !s->writable || record->sequence == 0u ||
        (s->has_active &&
         (record->sequence <= s->active.sequence || record->type != s->active.type ||
          record->product_namespace != s->active.product_namespace || record->schema != s->active.schema)))
        return HMI_SLOTS_INVALID;
    size_t n = 0u;
    if (hmi_record_encode(record, s->scratch, s->scratch_size, &n) != HMI_RECORD_RESULT_OK ||
        n > s->slot_size - s->seal_page)
        return HMI_SLOTS_CAPACITY;
    const size_t target = s->has_active ? 1u - s->active_slot : 0u;
    const size_t offset = target * s->slot_size;
    const size_t seal_offset = offset + s->slot_size - s->seal_page;
    const uint8_t invalid[HMI_RECORD_TRAILER_SIZE] = {0};
    s->last_error = HMI_IO_OK;
    /* seal 独占末页；失效确认之前不能动目标槽的数据页。 */
    if (!write_part(s, seal_offset, invalid, sizeof(invalid), &budget) ||
        !verify_part(s, seal_offset, invalid, sizeof(invalid)) ||
        !write_part(s, offset, s->scratch, n, &budget) || !verify_part(s, offset, s->scratch, n) ||
        !write_part(s, seal_offset, s->scratch + n - HMI_RECORD_TRAILER_SIZE, HMI_RECORD_TRAILER_SIZE,
                    &budget) ||
        !verify_part(s, seal_offset, s->scratch + n - HMI_RECORD_TRAILER_SIZE, HMI_RECORD_TRAILER_SIZE))
    {
        /* 进入写阶段后结果不确定；必须重新扫描，不能凭 RAM 缓存继续下一次提交。 */
        s->writable = false;
        return HMI_SLOTS_WRITE_INTERRUPTED;
    }
    hmi_record_view_t verified;
    if (hmi_record_decode(s->media + offset, n, record->product_namespace, record->schema, &verified) !=
        HMI_RECORD_RESULT_OK)
    {
        s->writable = false;
        return HMI_SLOTS_WRITE_INTERRUPTED;
    }
    s->has_active = true;
    s->active_slot = target;
    s->active = verified;
    s->degraded = false;
    return HMI_SLOTS_OK;
}
bool hmi_slots_active(const hmi_slots_t *s, hmi_record_view_t *record)
{
    if (!s || !record || !s->has_active)
        return false;
    *record = s->active;
    return true;
}

/* 只能由 worker 执行；不暴露任意地址写入口，不清除有效业务记录。 */
hmi_slots_result_t hmi_slots_initialize_empty(hmi_slots_t *s)
{
    if (!s || !s->io || !s->io->read || !s->io->write || !s->io->sync || s->has_active || !s->io->page_size ||
        s->io->page_size > s->scratch_size || s->last_error != HMI_IO_OK)
        return HMI_SLOTS_INVALID;
    s->writable = false;
    const size_t page = s->io->page_size;
    for (size_t offset = 0; offset < s->media_size; offset += page)
    {
        size_t size = s->media_size - offset;
        if (size > page)
            size = page;
        memset(s->scratch, 0xff, size);
        s->last_error = s->io->write(s->io->context, offset, s->scratch, size);
        if (s->last_error != HMI_IO_OK)
            return HMI_SLOTS_WRITE_INTERRUPTED;
        s->last_error = s->io->sync(s->io->context);
        if (s->last_error != HMI_IO_OK)
            return HMI_SLOTS_WRITE_INTERRUPTED;
        s->last_error = s->io->read(s->io->context, offset, s->media + offset, size);
        if (s->last_error != HMI_IO_OK)
            return HMI_SLOTS_WRITE_INTERRUPTED;
        if (memcmp(s->scratch, s->media + offset, size))
        {
            s->last_error = HMI_IO_VERIFY;
            return HMI_SLOTS_WRITE_INTERRUPTED;
        }
    }
    return HMI_SLOTS_EMPTY;
}
