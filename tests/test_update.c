#include "update/meter_update.h"
#include <assert.h>
#include <string.h>
typedef struct
{
    unsigned begins, writes, aborts, activates;
    meter_update_error_t result;
} fake_t;
static meter_update_error_t begin(void *p, const meter_update_manifest_t *m)
{
    (void)m;
    ((fake_t *)p)->begins++;
    return ((fake_t *)p)->result;
}
static meter_update_error_t write_block(void *p, const uint8_t *d, size_t n)
{
    (void)d;
    (void)n;
    ((fake_t *)p)->writes++;
    return ((fake_t *)p)->result;
}
static meter_update_error_t verify(void *p, const meter_update_manifest_t *m)
{
    (void)m;
    return ((fake_t *)p)->result;
}
static meter_update_error_t activate(void *p, const meter_update_manifest_t *m)
{
    (void)m;
    ((fake_t *)p)->activates++;
    return ((fake_t *)p)->result;
}
static void abort_update(void *p)
{
    ((fake_t *)p)->aborts++;
}
int main(void)
{
    fake_t f = {0};
    meter_firmware_update_t u;
    meter_update_backend_t b = {&f, begin, write_block, verify, activate, abort_update};
    meter_update_policy_t p = {"synthetic", "board", 4096, 1000};
    meter_update_manifest_t m = {
        .product = "synthetic", .hardware = "board", .version = "v2", .size = 1024, .sha256 = {1}};
    uint8_t block[512] = {0};
    assert(meter_update_init(&u, &p, &b));
    assert(meter_update_begin(&u, &m, false, 0) == METER_UPDATE_DENIED && f.begins == 0);
    m.product[0] = 'x';
    assert(meter_update_begin(&u, &m, true, 0) == METER_UPDATE_COMPATIBILITY && f.begins == 0);
    m.product[0] = 's';
    assert(meter_update_begin(&u, &m, true, UINT32_MAX - 20) == METER_UPDATE_OK);
    uint32_t gen = u.generation;
    assert(meter_update_write(&u, gen - 1, 0, block, 512, 0) == METER_UPDATE_SESSION && u.received == 0);
    assert(meter_update_write(&u, gen, 0, block, 512, 0) == METER_UPDATE_OK);
    assert(meter_update_verify(&u, gen, 1) == METER_UPDATE_STATE);
    assert(meter_update_write(&u, gen, 512, block, 512, 1) == METER_UPDATE_OK &&
           u.state == METER_UPDATE_TRANSFERRED);
    assert(meter_update_activate(&u, gen, true) == METER_UPDATE_STATE && f.activates == 0);
    assert(meter_update_verify(&u, gen, 2) == METER_UPDATE_OK && u.state == METER_UPDATE_CANDIDATE);
    assert(meter_update_activate(&u, gen, false) == METER_UPDATE_NVM && f.activates == 0);
    assert(meter_update_activate(&u, gen, true) == METER_UPDATE_OK && f.activates == 1);
    assert(meter_update_abort(&u, gen) == METER_UPDATE_STATE);
    assert(meter_update_init(&u, &p, &b));
    assert(meter_update_begin(&u, &m, true, 0) == METER_UPDATE_OK);
    assert(meter_update_write(&u, u.generation, 1, block, 512, 1) == METER_UPDATE_ORDER && !u.opened);
    assert(meter_update_begin(&u, &m, true, 0) == METER_UPDATE_OK);
    assert(meter_update_abort(&u, u.generation) == METER_UPDATE_OK);
    assert(meter_update_begin(&u, &m, true, UINT32_MAX - 20) == METER_UPDATE_OK);
    meter_update_tick(&u, 30);
    assert(u.state == METER_UPDATE_DOWNLOADING);
    meter_update_tick(&u, 1000);
    assert(u.error == METER_UPDATE_TIMEOUT && !u.opened);
    assert(meter_update_begin(&u, &m, true, 0) == METER_UPDATE_OK);
    f.result = METER_UPDATE_BACKEND;
    assert(meter_update_write(&u, u.generation, 0, block, 512, 1) == METER_UPDATE_BACKEND);
    f.result = METER_UPDATE_OK;
    assert(meter_update_begin(&u, &m, true, 0) == METER_UPDATE_OK);
    assert(meter_update_write(&u, u.generation, 0, block, 512, 1) == METER_UPDATE_OK);
    assert(meter_update_write(&u, u.generation, 512, block, 512, 2) == METER_UPDATE_OK);
    f.result = METER_UPDATE_HASH;
    assert(meter_update_verify(&u, u.generation, 3) == METER_UPDATE_HASH);
    assert(meter_update_activate(&u, u.generation, true) == METER_UPDATE_STATE && f.activates == 1);
    /* begin 部分失败必须清理；无效版本不得进入后端。 */
    assert(meter_update_init(&u, &p, &b));
    unsigned before = f.begins;
    strcpy(m.version, "v2\"bad");
    assert(meter_update_begin(&u, &m, true, 0) == METER_UPDATE_FORMAT && f.begins == before);
    strcpy(m.version, "v2");
    f.result = METER_UPDATE_BACKEND;
    unsigned aborted = f.aborts;
    assert(meter_update_begin(&u, &m, true, 0) == METER_UPDATE_BACKEND);
    assert(!u.opened && f.aborts == aborted + 1);
    f.result = METER_UPDATE_OK;
    assert(meter_update_begin(&u, &m, true, 0) == METER_UPDATE_OK);
    assert(meter_update_write(&u, u.generation, 0, block, 512, 1) == METER_UPDATE_OK);
    assert(meter_update_write(&u, u.generation, 512, block, 512, 2) == METER_UPDATE_OK);
    assert(meter_update_verify(&u, u.generation, 3) == METER_UPDATE_OK);
    f.result = METER_UPDATE_BACKEND;
    assert(meter_update_activate(&u, u.generation, true) == METER_UPDATE_BACKEND);
    assert(u.state == METER_UPDATE_FAILED && !u.opened);
    assert(meter_update_abort(&u, u.generation - 1) == METER_UPDATE_SESSION);
    f.result = METER_UPDATE_OK;
    u.generation = UINT32_MAX;
    assert(meter_update_begin(&u, &m, true, 4) == METER_UPDATE_SESSION);
    return 0;
}
