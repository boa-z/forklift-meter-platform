#include "storage/meter_eeprom.h"
#include "storage/meter_file.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(x))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "backend:%d: %s\n", __LINE__, #x);                                               \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
typedef struct
{
    uint8_t bytes[1024];
    unsigned writes, waits;
    bool protect, nack, busy, fail_sync;
} fake_t;
static meter_io_result_t rd(void *c, size_t o, uint8_t *p, size_t n)
{
    fake_t *f = c;
    if (f->nack)
        return METER_IO_ERROR;
    memcpy(p, f->bytes + o, n);
    return METER_IO_OK;
}
static meter_io_result_t wr(void *c, size_t o, const uint8_t *p, size_t n)
{
    fake_t *f = c;
    if (n > 128 || o % 128 + n > 128)
        return METER_IO_RANGE;
    if (f->nack)
        return METER_IO_ERROR;
    ++f->writes;
    if (!f->protect)
        memcpy(f->bytes + o, p, n);
    return METER_IO_OK;
}
static meter_io_result_t ready(void *c)
{
    return ((fake_t *)c)->busy ? METER_IO_ERROR : METER_IO_OK;
}
static void wait_ms(void *c, uint32_t ms)
{
    ((fake_t *)c)->waits += ms;
}
static meter_io_result_t bad_sync(void *c)
{
    (void)c;
    return METER_IO_ERROR;
}
int main(void)
{
    fake_t f = {0};
    memset(f.bytes, 255, sizeof(f.bytes));
    meter_eeprom_port_t p = {&f, rd, wr, ready, wait_ms};
    meter_eeprom_t e;
    CHECK(meter_eeprom_init(&e, &p, 1024, 0, 1024, 128, 5));
    uint8_t data[200];
    memset(data, 17, sizeof(data));
    CHECK(e.io.write(e.io.context, 120, data, 200) == METER_IO_OK && f.writes == 3);
    CHECK(e.io.write(e.io.context, SIZE_MAX, data, 2) == METER_IO_RANGE);
    CHECK(e.io.read(e.io.context, 1024, NULL, 0) == METER_IO_OK);
    f.busy = true;
    CHECK(e.io.write(e.io.context, 0, data, 1) == METER_IO_TIMEOUT && f.waits == 5);
    f.busy = false;
    memset(f.bytes, 255, sizeof(f.bytes));
    uint8_t media[1024], scratch[512];
    meter_slots_t s;
    CHECK(meter_slots_init(&s, media, sizeof(media), scratch, sizeof(scratch)) &&
          meter_slots_bind(&s, &e.io));
    memset(f.bytes, 0, sizeof(f.bytes));
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_NO_VALID_SLOT);
    f.protect = true;
    CHECK(meter_slots_initialize_empty(&s) == METER_SLOTS_WRITE_INTERRUPTED &&
          s.last_error == METER_IO_VERIFY);
    f.protect = false;
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_NO_VALID_SLOT);
    CHECK(meter_slots_initialize_empty(&s) == METER_SLOTS_EMPTY);
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_EMPTY);
    meter_record_view_t r = {1, 2, 1, 0, 1, data, 100};
    CHECK(meter_slots_commit(&s, &r, SIZE_MAX) == METER_SLOTS_OK);
    CHECK(meter_slots_initialize_empty(&s) == METER_SLOTS_INVALID);
    f.protect = true;
    r.sequence = 2;
    CHECK(meter_slots_commit(&s, &r, SIZE_MAX) == METER_SLOTS_WRITE_INTERRUPTED &&
          s.last_error == METER_IO_VERIFY);
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_OK && s.active.sequence == 1);
    f.protect = false;
    f.nack = true;
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_IO_ERROR);
    f.nack = false;
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_OK);
    e.io.sync = bad_sync;
    CHECK(meter_slots_commit(&s, &r, SIZE_MAX) == METER_SLOTS_WRITE_INTERRUPTED);
    e.io.sync = NULL;
    meter_file_t file;
    remove("nvm-test.0");
    remove("nvm-test.1");
    CHECK(!meter_file_init(&file, "nvm-test", 512, "fatfs", true));
    CHECK(meter_file_init(&file, "nvm-test", 512, "host-file", false));
    CHECK(meter_slots_bind(&s, &file.io));
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_EMPTY);
    CHECK(meter_slots_commit(&s, &r, SIZE_MAX) == METER_SLOTS_OK);
    r.sequence = 3;
    data[0] = 99;
    CHECK(meter_slots_commit(&s, &r, 40) == METER_SLOTS_WRITE_INTERRUPTED);
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_OK && s.active.sequence == 2 &&
          s.active.payload[0] == 17);
    CHECK(meter_slots_commit(&s, &r, SIZE_MAX) == METER_SLOTS_OK);
    memset(media, 0, sizeof(media));
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_OK && s.active.sequence == 3 &&
          s.active.payload[0] == 99);
    remove("nvm-test.0");
    remove("nvm-test.1");
    CHECK(meter_file_init(&file, "no-such-directory/nvm", 512, "host-file", false));
    CHECK(meter_slots_scan(&s, 2, 1) == METER_SLOTS_EMPTY);
    CHECK(meter_slots_commit(&s, &r, SIZE_MAX) == METER_SLOTS_WRITE_INTERRUPTED);
    puts("backends: PASS");
    return 0;
}
