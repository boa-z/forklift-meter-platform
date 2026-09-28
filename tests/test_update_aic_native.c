/* 真实 SDK 解包与摘要实现的 Host 故障注入；Flash 与 ENV 仅为测试替身。 */
#include "platform/rtthread/meter_sha_config.h"
#include "platform/rtthread/meter_update_backend.h"
#include <aic_crc32.h>
#include <assert.h>
#include <boot_param.h>
#include <mbedtls/sha256.h>
#include <rtdevice.h>
#include <stdio.h>
#include <string.h>
static struct rt_mtd_nand_device active = {{1}, 2048, 64, 32, 8, 39};
static struct rt_mtd_nand_device candidate = {{1}, 2048, 64, 32, 40, 71};
static struct rt_mtd_nand_device env_a = {{1}, 2048, 64, 2, 2, 3};
static struct rt_mtd_nand_device env_b = {{1}, 2048, 64, 2, 4, 5};
static unsigned char flash[4194304], raw_env[4096];
static unsigned erases, writes, activations, completed;
static char current[] = "A", next[] = "A", upgrade[] = "0";
static const char *scenario;
static int is(const char *name)
{
    return strcmp(scenario, name) == 0;
}
enum boot_device aic_get_boot_device(void)
{
    return BD_SPINAND;
}
rt_device_t rt_device_find(const char *name)
{
    if (!strcmp(name, "os"))
        return &active.parent;
    if (!strcmp(name, "os_r"))
        return &candidate.parent;
    if (!strcmp(name, "env"))
        return &env_a.parent;
    if (!strcmp(name, "env_r"))
        return &env_b.parent;
    return NULL;
}
int rt_device_open(rt_device_t device, int flags)
{
    (void)device;
    (void)flags;
    return 0;
}
int rt_device_close(rt_device_t device)
{
    (void)device;
    return 0;
}
int rt_mtd_nand_check_block(struct rt_mtd_nand_device *mtd, uint32_t block)
{
    assert(block < mtd->block_total);
    return mtd == &candidate && (is("bad-block") || (is("erase-bad-block") && erases)) ? -1 : 0;
}
int rt_mtd_nand_read(struct rt_mtd_nand_device *mtd, uint32_t page, void *buf, size_t len, void *oob,
                     size_t olen)
{
    (void)oob;
    (void)olen;
    assert((mtd == &env_a || mtd == &env_b) && page < 2 && len == 2048);
    if (is("env-read-failure"))
        return -1;
    memcpy(buf, raw_env + page * 2048, len);
    if (is("invalid-env") || (is("one-valid-env") && mtd == &env_a))
        ((unsigned char *)buf)[10] ^= 1;
    return 0;
}
int fw_env_open(void)
{
    return 0;
}
int fw_env_close(void)
{
    return 0;
}
char *fw_getenv(char *key)
{
    if (!strcmp(key, "osAB_now"))
        return current;
    if (!strcmp(key, "osAB_next"))
        return next;
    if (!strcmp(key, "upgrade_available"))
        return upgrade;
    if (!strcmp(key, "bootcount"))
        return "0";
    if (strstr(key, "rodataAB") || strstr(key, "dataAB"))
        return "A";
    return NULL;
}
void aic_set_upgrade_status(char *file)
{
    assert(!strcmp(file, "d13x_os.itb"));
    completed++;
}
int aic_upgrade_end(void)
{
    assert(completed && writes && !activations);
    activations++;
    if (!is("flush-failure"))
    {
        next[0] = 'B';
        upgrade[0] = '1';
    }
    return 0;
}
int aic_ota_find_part(char *part)
{
    /* 原生解包器绝不能选择当前运行槽、ENV 或数据分区。 */
    assert(!strcmp(part, "os_r"));
    return is("find-failure") ? -1 : 0;
}
int aic_ota_erase_part(void)
{
    erases++;
    memset(flash, 0xff, sizeof(flash));
    return is("erase-failure") ? -1 : 0;
}
int aic_ota_part_write(uint32_t offset, const uint8_t *buf, size_t size)
{
    assert(erases == 1 && size == 4096 && offset == writes * 4096 && offset + size <= sizeof(flash));
    if (is("write-failure"))
        return -1;
    memcpy(flash + offset, buf, size);
    writes++;
    return 0;
}
int aic_ota_part_read(uint32_t offset, uint8_t *buf, size_t size)
{
    assert(offset + size <= writes * 4096);
    if (is("read-failure"))
        return -1;
    memcpy(buf, flash + offset, size);
    /* 原生逐块回读通过后再破坏数据，必须由整体 SHA256 回读发现。 */
    if (is("readback-corruption") && completed)
        buf[0] ^= 1;
    return 0;
}
int main(int argc, char **argv)
{
    assert(argc == 4);
    scenario = argv[2];
    size_t chunk = (size_t)strtoul(argv[3], NULL, 10);
    assert(chunk > 0 && chunk <= 512);
    FILE *f = fopen(argv[1], "rb");
    assert(f);
    assert(fseek(f, 0, SEEK_END) == 0);
    long length = ftell(f);
    assert(length > 0);
    rewind(f);
    uint8_t *bytes = malloc((size_t)length);
    assert(bytes);
    assert(fread(bytes, 1, (size_t)length, f) == (size_t)length);
    fclose(f);
    uint32_t crc = env_crc32(0, raw_env + 5, 4091);
    memcpy(raw_env, &crc, 4);
    if (is("overlap"))
        candidate.block_start = 39;
    if (is("pending-env"))
        next[0] = 'B';
    bool supported = meter_aic_update_prepare();
    if (is("bad-block") || is("invalid-env") || is("overlap") || is("pending-env") || is("env-read-failure"))
    {
        assert(!supported && !erases && !activations);
        free(bytes);
        puts("PASS preflight refusal");
        return 0;
    }
    assert(supported);
    meter_aic_update_t owner = {0};
    meter_update_backend_t b = meter_aic_update_backend(&owner);
    meter_update_manifest_t m = {0};
    strcpy(m.version, "v2");
    m.size = (uint32_t)length;
    assert(mbedtls_sha256_ret(bytes, (size_t)length, m.sha256, 0) == 0);
    if (is("wrong-hash"))
        m.sha256[0] ^= 1;
    assert(b.activate(b.context, &m) == METER_UPDATE_STATE && !activations);
    assert(b.begin(b.context, &m) == METER_UPDATE_OK && !erases);
    meter_update_error_t error = METER_UPDATE_OK;
    for (size_t offset = 0; offset < (size_t)length && error == METER_UPDATE_OK;)
    {
        size_t n = chunk < (size_t)length - offset ? chunk : (size_t)length - offset;
        error = b.write(b.context, bytes + offset, n);
        offset += n;
        if (is("abort") && erases)
        {
            b.abort(b.context);
            assert(!activations);
            free(bytes);
            puts("PASS abort");
            return 0;
        }
    }
    if (error == METER_UPDATE_OK)
        error = b.verify(b.context, &m);
    if (is("success") || is("one-valid-env") || is("flush-failure"))
    {
        assert(error == METER_UPDATE_OK && completed == 1 && writes);
        meter_update_error_t activated = b.activate(b.context, &m);
        assert(activated == (is("flush-failure") ? METER_UPDATE_BACKEND : METER_UPDATE_OK));
        assert(activations == 1);
    }
    else
    {
        assert(error != METER_UPDATE_OK && !activations);
        assert(b.activate(b.context, &m) != METER_UPDATE_OK && !activations);
        if (is("bad-prefix") || is("invalid-fit") || is("unaligned-os"))
            assert(!erases);
    }
    b.abort(b.context);
    assert(meter_aic_update_confirm() != 0);
    free(bytes);
    puts("PASS native backend");
    return 0;
}
