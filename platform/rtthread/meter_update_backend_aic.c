#include "platform/rtthread/meter_sha_config.h"
#include "platform/rtthread/meter_update_backend.h"
#include "update/meter_package.h"
#include <absystem_os.h>
#include <aic_crc32.h>
#include <boot_param.h>
#include <burn.h>
#include <env.h>
#include <libfdt.h>
#include <mbedtls/sha256.h>
#include <ota.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <string.h>

/* 单一 Update worker 独占厂商的全局安装器；仅支持已经审计的无坏块 SPI NAND OS 分区对。
 * 分区几何（页、块、块数）在启动时从 MTD 设备读出，只要求 A/B 两个分区几何相同且互不重叠。 */
static struct
{
    meter_package_guard_t guard;
    mbedtls_sha256_context package_hash, image_hash;
    uint8_t prefix[1536], block[4096];
    ALIGN(64) uint8_t readback[4096];
    uint32_t capacity, buffered, written, prefix_size;
    struct rt_mtd_nand_device *candidate;
    char active[2], next[2], rodata[2], data[2];
    bool supported, native_open, verified;
} state;

/* 先独立验证 Flash ENV CRC，禁止厂商默认值回退被误认为有效槽位。 */
static bool valid_env_copy(const char *name)
{
    rt_device_t device = rt_device_find(name);
    if (!device || device->type != RT_Device_Class_MTD)
        return false;
    struct rt_mtd_nand_device *mtd = (struct rt_mtd_nand_device *)device;
    /* ENV 副本占块首的若干整页，一次读进 readback（大小即 ENV 大小）。 */
    const uint32_t page_size = mtd->page_size;
    if (!page_size || page_size > sizeof(state.readback) || sizeof(state.readback) % page_size ||
        sizeof(state.readback) / page_size > mtd->pages_per_block || !mtd->block_total ||
        rt_device_open(device, RT_DEVICE_OFLAG_RDONLY) != RT_EOK)
        return false;
    bool valid = false;
    for (uint32_t block = 0; block < mtd->block_total; ++block)
    {
        if (rt_mtd_nand_check_block(mtd, block) != RT_EOK)
            continue;
        uint32_t page = block * mtd->pages_per_block;
        bool read = true;
        for (uint32_t offset = 0; read && offset < sizeof(state.readback); offset += page_size, ++page)
            read = rt_mtd_nand_read(mtd, page, state.readback + offset, page_size, RT_NULL, 0) >= 0;
        if (read)
        {
            uint32_t expected;
            memcpy(&expected, state.readback, sizeof(expected));
            valid = expected == env_crc32(0, state.readback + 5, sizeof(state.readback) - 5u);
        }
        break;
    }
    rt_device_close(device);
    return valid;
}
static bool valid_environment(void)
{
#if defined(AIC_SYS_REDUNDAND_ENVIRONMENT) && AIC_ENV_SIZE == 4096
    bool first = valid_env_copy(AIC_ENV_PART_NAME);
    bool second = valid_env_copy(AIC_ENV_REDUNDAND_PART_NAME);
    return first || second;
#else
    return false;
#endif
}
static bool value_equals(char *key, const char *expected)
{
    const char *value = fw_getenv(key);
    return value && !strcmp(value, expected);
}
static bool slot(char *key, char output[2])
{
    const char *value = fw_getenv(key);
    if (!value || (strcmp(value, "A") && strcmp(value, "B")))
        return false;
    output[0] = value[0];
    output[1] = 0;
    return true;
}
static bool environment(bool activated)
{
    if (!valid_environment() || fw_env_open())
        return false;
    bool ok = value_equals("osAB_now", state.active) &&
              value_equals("osAB_next", activated ? state.next : state.active) &&
              value_equals("rodataAB_now", state.rodata) && value_equals("rodataAB_next", state.rodata) &&
              value_equals("dataAB_now", state.data) && value_equals("dataAB_next", state.data) &&
              value_equals("upgrade_available", activated ? "1" : "0");
    if (activated)
        ok = ok && value_equals("bootcount", "0");
    fw_env_close();
    return ok;
}
static bool clean_blocks(void)
{
    if (!state.candidate)
        return false;
    for (uint32_t i = 0; i < state.candidate->block_total; ++i)
        if (rt_mtd_nand_check_block(state.candidate, i) != RT_EOK)
            return false;
    return true;
}
bool meter_aic_update_prepare(void)
{
    state.supported = false;
    state.capacity = 0;
    if (aic_get_boot_device() != BD_SPINAND || !valid_environment() || fw_env_open())
        return false;
    bool ok = slot("osAB_now", state.active) && slot("rodataAB_now", state.rodata) &&
              slot("dataAB_now", state.data);
    fw_env_close();
    if (!ok)
        return false;
    state.next[0] = state.active[0] == 'A' ? 'B' : 'A';
    state.next[1] = 0;
    rt_device_t candidate = rt_device_find(state.next[0] == 'B' ? "os_r" : "os");
    rt_device_t active = rt_device_find(state.active[0] == 'A' ? "os" : "os_r");
    if (!candidate || !active || candidate == active || candidate->type != RT_Device_Class_MTD ||
        active->type != RT_Device_Class_MTD)
        return false;
    state.candidate = (struct rt_mtd_nand_device *)candidate;
    struct rt_mtd_nand_device *running = (struct rt_mtd_nand_device *)active;
    const struct rt_mtd_nand_device *c = state.candidate;
    if (!c->page_size || !c->pages_per_block || !c->block_total || running->page_size != c->page_size ||
        running->pages_per_block != c->pages_per_block || running->block_total != c->block_total ||
        c->block_end - c->block_start != c->block_total - 1u ||
        running->block_end - running->block_start != running->block_total - 1u ||
        !(c->block_end < running->block_start || running->block_end < c->block_start) ||
        (uint64_t)c->block_total * c->pages_per_block * c->page_size > UINT32_MAX)
        return false;
    state.capacity = c->block_total * c->pages_per_block * c->page_size;
    state.supported = environment(false) && clean_blocks();
    return state.supported;
}
uint32_t meter_aic_update_capacity(void)
{
    return state.capacity;
}
bool meter_aic_update_supported(void)
{
    return state.supported;
}
const char *meter_aic_update_reason(void)
{
    /* 线协议定长 32B，CAN 与 UART 共用同一字符串，不保留长名称。 */
    return state.supported ? "native_auto_unverified" : "env_or_slot_invalid";
}
int meter_aic_update_confirm(void)
{
    return -1;
}
static void release(void *context)
{
    (void)context;
    if (state.native_open)
        ota_deinit();
    state.native_open = false;
    state.verified = false;
    mbedtls_sha256_free(&state.package_hash);
    mbedtls_sha256_free(&state.image_hash);
}
static meter_update_error_t begin(void *context, const meter_update_manifest_t *manifest)
{
    (void)context;
    if (!state.supported || !environment(false) || !clean_blocks())
        return METER_UPDATE_UNSUPPORTED;
    meter_package_policy_t policy = {manifest->size, state.capacity, METER_UPDATE_OS_FILE, manifest->version};
    if (!meter_package_init(&state.guard, &policy))
        return METER_UPDATE_FORMAT;
    state.buffered = state.written = state.prefix_size = 0;
    state.verified = false;
    mbedtls_sha256_init(&state.package_hash);
    mbedtls_sha256_init(&state.image_hash);
    if (mbedtls_sha256_starts_ret(&state.package_hash, 0) || mbedtls_sha256_starts_ret(&state.image_hash, 0))
        return METER_UPDATE_HASH;
    return METER_UPDATE_OK;
}
static meter_update_error_t image_bytes(const uint8_t *data, size_t size)
{
    if (mbedtls_sha256_update_ret(&state.image_hash, data, size))
        return METER_UPDATE_HASH;
    while (size)
    {
        size_t count = sizeof(state.block) - state.buffered;
        if (count > size)
            count = size;
        memcpy(state.block + state.buffered, data, count);
        state.buffered += (uint32_t)count;
        data += count;
        size -= count;
        if (state.buffered == sizeof(state.block))
        {
            /* 不允许厂商坏块跳转越过候选边界；发现任何坏块立即终止会话。 */
            if (!clean_blocks() || state.written > state.capacity - sizeof(state.block) ||
                ota_shard_download_fun((char *)state.block, sizeof(state.block)))
                return METER_UPDATE_BACKEND;
            state.written += sizeof(state.block);
            state.buffered = 0;
        }
    }
    return METER_UPDATE_OK;
}
static meter_update_error_t write_block(void *context, const uint8_t *data, size_t size)
{
    (void)context;
    uint32_t start = state.guard.received;
    if (meter_package_feed(&state.guard, data, size) != METER_PACKAGE_OK)
        return METER_UPDATE_FORMAT;
    if (mbedtls_sha256_update_ret(&state.package_hash, data, size))
        return METER_UPDATE_HASH;
    if (!state.native_open)
    {
        if (size > sizeof(state.prefix) - state.prefix_size)
            return METER_UPDATE_FORMAT;
        memcpy(state.prefix + state.prefix_size, data, size);
        state.prefix_size += (uint32_t)size;
        if (!state.guard.os_offset || state.prefix_size < state.guard.os_offset + sizeof(struct fdt_header))
            return METER_UPDATE_OK;
        const void *fit = state.prefix + state.guard.os_offset;
        if (!state.guard.os_size || state.guard.os_size % sizeof(state.block) || fdt_check_header(fit) ||
            fdt_totalsize(fit) > state.guard.os_size || !environment(false))
            return METER_UPDATE_FORMAT;
        if (ota_init())
            return METER_UPDATE_BACKEND;
        state.native_open = true;
        /* 仅在整个受限元数据与 OS 头已经验证后，允许厂商选择并擦除候选分区。 */
        if (ota_shard_download_fun((char *)state.prefix, state.guard.os_offset) || !clean_blocks())
            return METER_UPDATE_BACKEND;
        return image_bytes(state.prefix + state.guard.os_offset, state.prefix_size - state.guard.os_offset);
    }
    uint32_t end = state.guard.os_offset + state.guard.os_size;
    if (start >= end)
        return METER_UPDATE_OK;
    size_t count = size < end - start ? size : end - start;
    return image_bytes(data, count);
}
static meter_update_error_t verify(void *context, const meter_update_manifest_t *manifest)
{
    (void)context;
    uint8_t package_digest[32], sent_digest[32], installed_digest[32];
    if (!state.native_open || state.buffered || state.written != state.guard.os_size ||
        meter_package_finish(&state.guard) != METER_PACKAGE_OK || !environment(false))
        return METER_UPDATE_FORMAT;
    if (mbedtls_sha256_finish_ret(&state.package_hash, package_digest) ||
        memcmp(package_digest, manifest->sha256, 32) ||
        mbedtls_sha256_finish_ret(&state.image_hash, sent_digest))
        return METER_UPDATE_HASH;
    mbedtls_sha256_context installed;
    mbedtls_sha256_init(&installed);
    int result = mbedtls_sha256_starts_ret(&installed, 0);
    for (uint32_t offset = 0; !result && offset < state.written; offset += sizeof(state.readback))
    {
        result = clean_blocks() ? aic_ota_part_read(offset, state.readback, sizeof(state.readback)) : -1;
        if (!result)
            result = mbedtls_sha256_update_ret(&installed, state.readback, sizeof(state.readback));
    }
    if (!result)
        result = mbedtls_sha256_finish_ret(&installed, installed_digest);
    mbedtls_sha256_free(&installed);
    if (result || memcmp(sent_digest, installed_digest, 32))
        return METER_UPDATE_HASH;
    state.verified = true;
    return METER_UPDATE_OK;
}
static meter_update_error_t activate(void *context, const meter_update_manifest_t *manifest)
{
    (void)context;
    (void)manifest;
    if (!state.verified || !environment(false))
        return METER_UPDATE_STATE;
    /* 原生接口不传递 flush 失败；关闭并重新从 Flash 读取 ENV 验证实际持久化结果。 */
    if (aic_upgrade_end() || !environment(true))
        return METER_UPDATE_BACKEND;
    return METER_UPDATE_OK;
}
meter_update_backend_t meter_aic_update_backend(void)
{
    return (meter_update_backend_t){NULL, begin, write_block, verify, activate, release};
}

#ifndef AIC_NFTL_SUPPORT
/** @brief 未启用 NFTL 的 OS-only 组合拒绝厂商可选数据分区重建入口。 */
rt_err_t rt_spinand_init_nftl(rt_device_t device)
{
    (void)device;
    return -RT_ENOSYS;
}
#endif
