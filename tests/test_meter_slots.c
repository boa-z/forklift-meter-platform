#include "storage/meter_slots.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(x))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "slots:%d: %s\n", __LINE__, #x);                                                 \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
int main(void)
{
    uint8_t media[512], saved[512], scratch[256];
    memset(media, 255, sizeof(media));
    meter_slots_t s;
    CHECK(meter_slots_init(&s, media, sizeof(media), scratch, sizeof(scratch)));
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_EMPTY);
    const uint8_t a[] = {1, 2, 3}, b[] = {4, 5, 6};
    meter_record_view_t r = {1, 2, 1, 0, 1, a, sizeof(a)};
    CHECK(meter_slots_commit(&s, &r, SIZE_MAX) == METER_SLOTS_OK);
    r.sequence = 2;
    r.payload = b;
    CHECK(meter_slots_commit(&s, &r, SIZE_MAX) == METER_SLOTS_OK);
    memcpy(saved, media, sizeof(media));
    r.sequence = 3;
    r.payload = a;
    const size_t total = METER_RECORD_HEADER_SIZE + sizeof(a) + 3 * METER_RECORD_TRAILER_SIZE;
    for (size_t cut = 0; cut < total; ++cut)
    {
        memcpy(media, saved, sizeof(media));
        CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_OK && s.active.sequence == 2);
        CHECK(meter_slots_commit(&s, &r, cut) == METER_SLOTS_WRITE_INTERRUPTED);
        CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_OK && s.active.sequence == 2);
    }
    CHECK(meter_slots_commit(&s, &r, SIZE_MAX) == METER_SLOTS_OK);
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_OK && s.active.sequence == 3);
    CHECK(meter_slots_scan(&s, 99, 1) == METER_SLOTS_INCOMPATIBLE && !s.writable);
    /* 同代次但数据冲突必须拒绝；不能任意取第一槽。 */
    size_t n;
    r.payload = b;
    CHECK(meter_record_encode(&r, media + 256, 240, &n) == METER_RECORD_RESULT_OK);
    memcpy(media + 496, media + 256 + n - METER_RECORD_TRAILER_SIZE, METER_RECORD_TRAILER_SIZE);
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_CONFLICT);
    memset(media, 0, sizeof(media));
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_NO_VALID_SLOT && !s.writable);
    puts("slots: PASS");
    return 0;
}
