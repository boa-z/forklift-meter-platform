#include "platform/host/host_settings.h"
#include "storage/hmi_file.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>
/* 槽大小与净负载上限取自实板配置（platform/rtthread/hmi_nvm_port.c 的 512/256），seal 页仍由后端 page 决定。
   若按当前设置长度推导槽大小，参数目录增删会让旧介质里的 seal 位置漂移，升级后扫描不到有效槽，
   Host 回归就复现不了实板"目录变更后仍读取旧记录"的行为。 */
#define HMI_HOST_NVM_SLOT_SIZE 512u
#define HMI_HOST_NVM_PAYLOAD_SIZE 256u
struct hmi_host_nvm
{
    hmi_core_t *core;
    hmi_nvm_service_t service;
    hmi_slots_t slots;
    hmi_file_t file;
    SDL_Thread *thread;
    SDL_mutex *lock;
    SDL_sem *wake;
    uint8_t *allocation, *encoded;
    hmi_nvm_job_t job;
    hmi_slots_result_t result;
    hmi_record_view_t record;
    bool load, done, outstanding, stop, allow_parameter_extension;
};
static int run(void *ctx)
{
    hmi_host_nvm_t *h = ctx;
    for (;;)
    {
        if (SDL_SemWait(h->wake) != 0)
            return 1;
        SDL_LockMutex(h->lock);
        bool stop = h->stop, load = h->load;
        hmi_nvm_job_t job = h->job;
        SDL_UnlockMutex(h->lock);
        if (stop)
            return 0;
        hmi_record_view_t record = {0};
        hmi_slots_result_t result;
        if (load)
        {
            result = hmi_slots_scan(&h->slots, job.record.product_namespace, job.record.schema);
            if (result == HMI_SLOTS_OK)
                (void)hmi_slots_active(&h->slots, &record);
            else
                record.sequence = h->slots.max_sequence;
        }
        else
            result = hmi_slots_commit(&h->slots, &job.record, SIZE_MAX);
        SDL_LockMutex(h->lock);
        h->record = record;
        h->result = result;
        h->done = true;
        SDL_UnlockMutex(h->lock);
    }
}
hmi_host_nvm_t *hmi_host_nvm_open(hmi_core_t *core, const char *path, uint16_t ns, uint16_t schema)
{
    const hmi_storage_profile_t profile = {.enabled = true, .record_type = 1u,
        .product_namespace = ns, .schema = schema, .debounce_ms = 500u, .max_delay_ms = 3000u};
    return hmi_host_nvm_open_profile(core, path, &profile, false);
}
hmi_host_nvm_t *hmi_host_nvm_open_profile(hmi_core_t *core, const char *path,
                                             const hmi_storage_profile_t *profile, bool allow_parameter_extension)
{
    if (!core || !path || !profile || !profile->enabled)
        return NULL;
    const size_t capacity = hmi_settings_size(core);
    if (!capacity || capacity > HMI_HOST_NVM_PAYLOAD_SIZE)
        return NULL;
    const size_t slot = HMI_HOST_NVM_SLOT_SIZE;
    hmi_host_nvm_t *h = calloc(1, sizeof(*h));
    if (!h)
        return NULL;
    h->allocation = calloc(1, 3u * capacity + 3u * slot);
    h->lock = SDL_CreateMutex();
    h->wake = SDL_CreateSemaphore(0);
    if (!h->allocation || !h->lock || !h->wake)
        goto failed;
    h->core = core;
    h->allow_parameter_extension = allow_parameter_extension;
    h->encoded = h->allocation + 2u * capacity;
    if (!hmi_nvm_init(&h->service, h->allocation, h->allocation + capacity, capacity,
                        profile->record_type, profile->product_namespace, profile->schema,
                        profile->debounce_ms, profile->max_delay_ms) ||
        !hmi_file_init(&h->file, path, slot, "host-file", false) ||
        !hmi_slots_init(&h->slots, h->allocation + 3u * capacity, 2u * slot,
                          h->allocation + 3u * capacity + 2u * slot, slot) ||
        !hmi_slots_bind(&h->slots, &h->file.io))
        goto failed;
    h->service.always_writable = profile->always_writable;
    h->slots.force_writable = profile->always_writable;
    h->load = true;
    h->outstanding = true;
    h->job.generation = h->service.generation;
    h->job.record.product_namespace = profile->product_namespace;
    h->job.record.schema = profile->schema;
    h->thread = SDL_CreateThread(run, "hmi-nvm", h);
    if (!h->thread)
        goto failed;
    SDL_SemPost(h->wake);
    return h;
failed:
    if (h->wake)
        SDL_DestroySemaphore(h->wake);
    if (h->lock)
        SDL_DestroyMutex(h->lock);
    free(h->allocation);
    free(h);
    return NULL;
}
bool hmi_host_nvm_changed(hmi_host_nvm_t *h, uint32_t now)
{
    if (!h)
        return true;
    return hmi_settings_encode(h->core, h->encoded, h->service.capacity) &&
           hmi_nvm_observe(&h->service, h->encoded, hmi_settings_size(h->core), now);
}
void hmi_host_nvm_poll(hmi_host_nvm_t *h, uint32_t now)
{
    if (!h)
        return;
    SDL_LockMutex(h->lock);
    bool done = h->done, load = h->load;
    hmi_slots_result_t result = h->result;
    hmi_record_view_t record = h->record;
    hmi_nvm_job_t completed = h->job;
    if (done)
        h->done = false;
    SDL_UnlockMutex(h->lock);
    if (done)
    {
        h->outstanding = false;
        if (load)
        {
            if (result == HMI_SLOTS_OK &&
                (record.type != h->service.type ||
                 !(h->allow_parameter_extension
                     ? hmi_settings_decode_append_only(h->core, record.payload, record.payload_size)
                     : hmi_settings_decode(h->core, record.payload, record.payload_size))))
                result = HMI_SLOTS_INCOMPATIBLE;
            (void)hmi_nvm_loaded(&h->service, completed.generation, result, &record);
        }
        else
            (void)hmi_nvm_complete(&h->service, completed.generation, completed.revision, result);
    }
    hmi_nvm_job_t job;
    if (!h->outstanding && hmi_nvm_take(&h->service, now, &job))
    {
        SDL_LockMutex(h->lock);
        h->load = false;
        h->job = job;
        SDL_UnlockMutex(h->lock);
        h->outstanding = true;
        SDL_SemPost(h->wake);
    }
}
const hmi_nvm_service_t *hmi_host_nvm_status(const hmi_host_nvm_t *h)
{
    return h ? &h->service : NULL;
}
bool hmi_host_nvm_close(hmi_host_nvm_t *h, uint32_t timeout)
{
    if (!h)
        return true;
    (void)hmi_host_nvm_changed(h, SDL_GetTicks());
    hmi_nvm_request_save(&h->service);
    const uint32_t start = SDL_GetTicks();
    while (h->outstanding || h->service.dirty)
    {
        hmi_host_nvm_poll(h, SDL_GetTicks());
        if (!h->outstanding && (!h->service.writable || !h->service.dirty))
            break;
        if ((uint32_t)(SDL_GetTicks() - start) >= timeout)
            return false;
        SDL_Delay(1);
    }
    const bool durable = hmi_nvm_barrier(&h->service, h->service.ram_revision);
    SDL_LockMutex(h->lock);
    h->stop = true;
    SDL_UnlockMutex(h->lock);
    SDL_SemPost(h->wake);
    SDL_WaitThread(h->thread, NULL);
    SDL_DestroySemaphore(h->wake);
    SDL_DestroyMutex(h->lock);
    free(h->allocation);
    free(h);
    return durable;
}
