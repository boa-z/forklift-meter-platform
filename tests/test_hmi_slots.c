#include "storage/hmi_slots.h"
#include <stdio.h>
#include <string.h>
#include "check.h"
int main(void)
{
    uint8_t media[512], saved[512], scratch[256];
    memset(media, 255, sizeof(media));
    hmi_slots_t s;
    CHECK(hmi_slots_init(&s, media, sizeof(media), scratch, sizeof(scratch)));
    CHECK(hmi_slots_scan(&s, 2, 1) == HMI_SLOTS_EMPTY);
    const uint8_t a[] = {1, 2, 3}, b[] = {4, 5, 6};
    hmi_record_view_t r = {1, 2, 1, 0, 1, a, sizeof(a)};
    CHECK(hmi_slots_commit(&s, &r, SIZE_MAX) == HMI_SLOTS_OK);
    r.sequence = 2;
    r.payload = b;
    CHECK(hmi_slots_commit(&s, &r, SIZE_MAX) == HMI_SLOTS_OK);
    memcpy(saved, media, sizeof(media));
    r.sequence = 3;
    r.payload = a;
    const size_t total = HMI_RECORD_HEADER_SIZE + sizeof(a) + 3 * HMI_RECORD_TRAILER_SIZE;
    for (size_t cut = 0; cut < total; ++cut)
    {
        memcpy(media, saved, sizeof(media));
        CHECK(hmi_slots_scan(&s, 2, 1) == HMI_SLOTS_OK && s.active.sequence == 2);
        CHECK(hmi_slots_commit(&s, &r, cut) == HMI_SLOTS_WRITE_INTERRUPTED);
        CHECK(hmi_slots_scan(&s, 2, 1) == HMI_SLOTS_OK && s.active.sequence == 2);
    }
    CHECK(hmi_slots_commit(&s, &r, SIZE_MAX) == HMI_SLOTS_OK);
    CHECK(hmi_slots_scan(&s, 2, 1) == HMI_SLOTS_OK && s.active.sequence == 3);
    CHECK(hmi_slots_scan(&s, 99, 1) == HMI_SLOTS_INCOMPATIBLE && !s.writable);
    /* 同代次但数据冲突必须拒绝；不能任意取第一槽。 */
    size_t n;
    r.payload = b;
    CHECK(hmi_record_encode(&r, media + 256, 240, &n) == HMI_RECORD_RESULT_OK);
    memcpy(media + 496, media + 256 + n - HMI_RECORD_TRAILER_SIZE, HMI_RECORD_TRAILER_SIZE);
    CHECK(hmi_slots_scan(&s, 2, 1) == HMI_SLOTS_CONFLICT);
    memset(media, 0, sizeof(media));
    CHECK(hmi_slots_scan(&s, 2, 1) == HMI_SLOTS_NO_VALID_SLOT && !s.writable);
    puts("slots: PASS");
    return 0;
}
