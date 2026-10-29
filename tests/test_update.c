#include "update/hmi_update.h"
#include <assert.h>
#include <string.h>
typedef struct
{
    unsigned begins, writes, aborts, activates;
    hmi_update_error_t result;
} fake_t;
static hmi_update_error_t begin(void *p, const hmi_update_manifest_t *m)
{
    (void)m;
    ((fake_t *)p)->begins++;
    return ((fake_t *)p)->result;
}
static hmi_update_error_t write_block(void *p, const uint8_t *d, size_t n)
{
    (void)d;
    (void)n;
    ((fake_t *)p)->writes++;
    return ((fake_t *)p)->result;
}
static hmi_update_error_t verify(void *p, const hmi_update_manifest_t *m)
{
    (void)m;
    return ((fake_t *)p)->result;
}
static hmi_update_error_t activate(void *p, const hmi_update_manifest_t *m)
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
    hmi_firmware_update_t u;
    hmi_update_backend_t b = {&f, begin, write_block, verify, activate, abort_update};
    hmi_update_policy_t p = {"synthetic", "board", 4096, 1000};
    hmi_update_manifest_t m = {
        .product = "synthetic", .hardware = "board", .version = "v2", .size = 1024, .sha256 = {1}};
    uint8_t block[512] = {0};
    assert(hmi_update_init(&u, &p, &b));
    assert(hmi_update_begin(&u, &m, false, 0) == HMI_UPDATE_DENIED && f.begins == 0);
    m.product[0] = 'x';
    assert(hmi_update_begin(&u, &m, true, 0) == HMI_UPDATE_COMPATIBILITY && f.begins == 0);
    m.product[0] = 's';
    /* 硬件身份与 Product 策略不一致同样拒绝：升级包的身份必须等于设备上报的 F192/FD12。 */
    m.hardware[0] = 'x';
    assert(hmi_update_begin(&u, &m, true, 0) == HMI_UPDATE_COMPATIBILITY && f.begins == 0);
    m.hardware[0] = 'b';
    assert(hmi_update_begin(&u, &m, true, UINT32_MAX - 20) == HMI_UPDATE_OK);
    uint32_t gen = u.generation;
    assert(hmi_update_write(&u, gen - 1, 0, block, 512, 0) == HMI_UPDATE_SESSION && u.received == 0);
    assert(hmi_update_write(&u, gen, 0, block, 512, 0) == HMI_UPDATE_OK);
    assert(hmi_update_verify(&u, gen, 1) == HMI_UPDATE_STATE);
    assert(hmi_update_write(&u, gen, 512, block, 512, 1) == HMI_UPDATE_OK &&
           u.state == HMI_UPDATE_TRANSFERRED);
    assert(hmi_update_activate(&u, gen, true) == HMI_UPDATE_STATE && f.activates == 0);
    assert(hmi_update_verify(&u, gen, 2) == HMI_UPDATE_OK && u.state == HMI_UPDATE_CANDIDATE);
    assert(hmi_update_activate(&u, gen, false) == HMI_UPDATE_NVM && f.activates == 0);
    assert(hmi_update_activate(&u, gen, true) == HMI_UPDATE_OK && f.activates == 1);
    assert(hmi_update_abort(&u, gen) == HMI_UPDATE_STATE);
    assert(hmi_update_init(&u, &p, &b));
    assert(hmi_update_begin(&u, &m, true, 0) == HMI_UPDATE_OK);
    assert(hmi_update_write(&u, u.generation, 1, block, 512, 1) == HMI_UPDATE_ORDER && !u.opened);
    assert(hmi_update_begin(&u, &m, true, 0) == HMI_UPDATE_OK);
    assert(hmi_update_abort(&u, u.generation) == HMI_UPDATE_OK);
    assert(hmi_update_begin(&u, &m, true, UINT32_MAX - 20) == HMI_UPDATE_OK);
    hmi_update_tick(&u, 30);
    assert(u.state == HMI_UPDATE_DOWNLOADING);
    hmi_update_tick(&u, 1000);
    assert(u.error == HMI_UPDATE_TIMEOUT && !u.opened);
    assert(hmi_update_begin(&u, &m, true, 0) == HMI_UPDATE_OK);
    f.result = HMI_UPDATE_BACKEND;
    assert(hmi_update_write(&u, u.generation, 0, block, 512, 1) == HMI_UPDATE_BACKEND);
    f.result = HMI_UPDATE_OK;
    assert(hmi_update_begin(&u, &m, true, 0) == HMI_UPDATE_OK);
    assert(hmi_update_write(&u, u.generation, 0, block, 512, 1) == HMI_UPDATE_OK);
    assert(hmi_update_write(&u, u.generation, 512, block, 512, 2) == HMI_UPDATE_OK);
    f.result = HMI_UPDATE_HASH;
    assert(hmi_update_verify(&u, u.generation, 3) == HMI_UPDATE_HASH);
    assert(hmi_update_activate(&u, u.generation, true) == HMI_UPDATE_STATE && f.activates == 1);
    /* begin 部分失败必须清理；无效版本不得进入后端。 */
    assert(hmi_update_init(&u, &p, &b));
    unsigned before = f.begins;
    strcpy(m.version, "v2\"bad");
    assert(hmi_update_begin(&u, &m, true, 0) == HMI_UPDATE_FORMAT && f.begins == before);
    strcpy(m.version, "v2");
    f.result = HMI_UPDATE_BACKEND;
    unsigned aborted = f.aborts;
    assert(hmi_update_begin(&u, &m, true, 0) == HMI_UPDATE_BACKEND);
    assert(!u.opened && f.aborts == aborted + 1);
    f.result = HMI_UPDATE_OK;
    assert(hmi_update_begin(&u, &m, true, 0) == HMI_UPDATE_OK);
    assert(hmi_update_write(&u, u.generation, 0, block, 512, 1) == HMI_UPDATE_OK);
    assert(hmi_update_write(&u, u.generation, 512, block, 512, 2) == HMI_UPDATE_OK);
    assert(hmi_update_verify(&u, u.generation, 3) == HMI_UPDATE_OK);
    f.result = HMI_UPDATE_BACKEND;
    assert(hmi_update_activate(&u, u.generation, true) == HMI_UPDATE_BACKEND);
    assert(u.state == HMI_UPDATE_FAILED && !u.opened);
    assert(hmi_update_abort(&u, u.generation - 1) == HMI_UPDATE_SESSION);
    f.result = HMI_UPDATE_OK;
    u.generation = UINT32_MAX;
    assert(hmi_update_begin(&u, &m, true, 4) == HMI_UPDATE_SESSION);
    /* WAIT_DURABLE 仍可被显式 Abort 销毁；会话超时走的是 port 的取消路径，不经过这里。 */
    assert(hmi_update_init(&u, &p, &b));
    assert(hmi_update_begin(&u, &m, true, 0) == HMI_UPDATE_OK);
    gen = u.generation;
    assert(hmi_update_write(&u, gen, 0, block, 512, 1) == HMI_UPDATE_OK);
    assert(hmi_update_write(&u, gen, 512, block, 512, 2) == HMI_UPDATE_OK);
    assert(hmi_update_verify(&u, gen, 3) == HMI_UPDATE_OK);
    assert(hmi_update_activate(&u, gen, false) == HMI_UPDATE_NVM && u.state == HMI_UPDATE_WAIT_DURABLE);
    assert(hmi_update_abort(&u, gen) == HMI_UPDATE_OK && u.state == HMI_UPDATE_ABORTED && !u.opened);
    return 0;
}
