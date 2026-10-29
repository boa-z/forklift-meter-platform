#include "update/hmi_update.h"
#include <string.h>
/* 版本只允许稳定 ASCII 标识，避免诊断字符串注入或跨端编码歧义。 */
static bool valid_version(const char *version)
{
    for (size_t i = 0; version[i] != '\0'; ++i)
    {
        char c = version[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
              c == '_' || c == '+' || c == '-'))
            return false;
    }
    return version[0] != '\0';
}
static hmi_update_error_t fail(hmi_firmware_update_t *u, hmi_update_error_t e)
{
    if (u->opened)
        u->backend.abort(u->backend.context);
    u->opened = false;
    u->error = e;
    u->state = HMI_UPDATE_FAILED;
    return e;
}
bool hmi_update_init(hmi_firmware_update_t *u, const hmi_update_policy_t *p,
                       const hmi_update_backend_t *b)
{
    if (!u || !p || !b || !p->product || !p->hardware || !p->max_size || !p->timeout_ms ||
        p->timeout_ms >= UINT32_C(0x80000000) || strlen(p->product) >= HMI_UPDATE_ID_SIZE ||
        strlen(p->hardware) >= HMI_UPDATE_ID_SIZE || !b->begin || !b->write || !b->verify || !b->activate ||
        !b->abort)
        return false;
    memset(u, 0, sizeof(*u));
    u->policy = *p;
    u->backend = *b;
    return true;
}
hmi_update_error_t hmi_update_begin(hmi_firmware_update_t *u, const hmi_update_manifest_t *m,
                                        bool admitted, uint32_t now)
{
    if (!u || !m)
        return HMI_UPDATE_FORMAT;
    if (u->state != HMI_UPDATE_IDLE && u->state != HMI_UPDATE_FAILED && u->state != HMI_UPDATE_ABORTED)
        return HMI_UPDATE_STATE;
    if (!admitted)
        return HMI_UPDATE_DENIED;
    if (!memchr(m->product, 0, sizeof(m->product)) || !memchr(m->hardware, 0, sizeof(m->hardware)) ||
        !memchr(m->version, 0, sizeof(m->version)) || !valid_version(m->version))
        return HMI_UPDATE_FORMAT;
    if (strcmp(m->product, u->policy.product) || strcmp(m->hardware, u->policy.hardware))
        return HMI_UPDATE_COMPATIBILITY;
    if (!m->size || m->size > u->policy.max_size)
        return HMI_UPDATE_LENGTH;
    if (u->generation == UINT32_MAX)
        return HMI_UPDATE_SESSION;
    u->generation++;
    u->manifest = *m;
    u->received = 0;
    u->started_ms = now;
    u->last_activity = now;
    u->completed_ms = 0;
    u->error = HMI_UPDATE_OK;
    /* begin 失败也可能留下资源，必须调用幂等 abort 清理。 */
    u->opened = true;
    hmi_update_error_t e = u->backend.begin(u->backend.context, m);
    if (e != HMI_UPDATE_OK)
        return fail(u, e);
    u->state = HMI_UPDATE_DOWNLOADING;
    return HMI_UPDATE_OK;
}
hmi_update_error_t hmi_update_write(hmi_firmware_update_t *u, uint32_t gen, uint32_t offset,
                                        const uint8_t *data, size_t n, uint32_t now)
{
    if (!u || gen != u->generation)
        return HMI_UPDATE_SESSION;
    if (u->state != HMI_UPDATE_DOWNLOADING)
        return HMI_UPDATE_STATE;
    if (offset != u->received)
        return fail(u, HMI_UPDATE_ORDER);
    if (!data || !n || n > HMI_UPDATE_BLOCK_SIZE || n > u->manifest.size - u->received)
        return fail(u, HMI_UPDATE_LENGTH);
    hmi_update_error_t e = u->backend.write(u->backend.context, data, n);
    if (e != HMI_UPDATE_OK)
        return fail(u, e);
    u->received += (uint32_t)n;
    u->last_activity = now;
    if (u->received == u->manifest.size)
        u->state = HMI_UPDATE_TRANSFERRED;
    return HMI_UPDATE_OK;
}
hmi_update_error_t hmi_update_verify(hmi_firmware_update_t *u, uint32_t gen, uint32_t now)
{
    if (!u || gen != u->generation)
        return HMI_UPDATE_SESSION;
    if (u->state != HMI_UPDATE_TRANSFERRED)
        return HMI_UPDATE_STATE;
    u->state = HMI_UPDATE_VERIFYING;
    hmi_update_error_t e = u->backend.verify(u->backend.context, &u->manifest);
    if (e != HMI_UPDATE_OK)
        return fail(u, e);
    u->state = HMI_UPDATE_CANDIDATE;
    u->completed_ms = now;
    return HMI_UPDATE_OK;
}
hmi_update_error_t hmi_update_activate(hmi_firmware_update_t *u, uint32_t gen, bool durable)
{
    if (!u || gen != u->generation)
        return HMI_UPDATE_SESSION;
    if (u->state != HMI_UPDATE_CANDIDATE && u->state != HMI_UPDATE_WAIT_DURABLE)
        return HMI_UPDATE_STATE;
    if (!durable)
    {
        u->state = HMI_UPDATE_WAIT_DURABLE;
        u->error = HMI_UPDATE_NVM;
        return HMI_UPDATE_NVM;
    }
    u->state = HMI_UPDATE_ACTIVATING;
    hmi_update_error_t e = u->backend.activate(u->backend.context, &u->manifest);
    if (e != HMI_UPDATE_OK)
        return fail(u, e);
    u->backend.abort(u->backend.context);
    u->opened = false;
    u->state = HMI_UPDATE_ACTIVATED;
    u->error = HMI_UPDATE_OK;
    return HMI_UPDATE_OK;
}
hmi_update_error_t hmi_update_abort(hmi_firmware_update_t *u, uint32_t gen)
{
    if (!u || gen != u->generation)
        return HMI_UPDATE_SESSION;
    if (u->state == HMI_UPDATE_ACTIVATED || u->state == HMI_UPDATE_CONFIRMED)
        return HMI_UPDATE_STATE;
    if (u->opened)
        u->backend.abort(u->backend.context);
    u->opened = false;
    u->state = HMI_UPDATE_ABORTED;
    u->error = HMI_UPDATE_OK;
    return HMI_UPDATE_OK;
}
void hmi_update_tick(hmi_firmware_update_t *u, uint32_t now)
{
    if (u && (u->state == HMI_UPDATE_DOWNLOADING || u->state == HMI_UPDATE_TRANSFERRED) &&
        (uint32_t)(now - u->last_activity) >= u->policy.timeout_ms)
        (void)fail(u, HMI_UPDATE_TIMEOUT);
}
const char *hmi_update_state_name(hmi_update_state_t state)
{
    static const char *const names[] = {
        "IDLE",       "DOWNLOADING", "TRANSFERRED", "VERIFYING", "CANDIDATE_READY", "WAIT_DURABLE",
        "ACTIVATING", "ACTIVATED",   "CONFIRMED",   "ABORTED",   "FAILED"};
    return (unsigned)state < sizeof(names) / sizeof(names[0]) ? names[state] : "UNKNOWN";
}
