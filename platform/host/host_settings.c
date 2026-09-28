#include "platform/host/host_settings.h"
#include "storage/meter_file.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>
struct meter_host_nvm
{
    meter_core_t *core;
    meter_nvm_service_t service;
    meter_slots_t slots;
    meter_file_t file;
    SDL_Thread *thread;
    SDL_mutex *lock;
    SDL_sem *wake;
    uint8_t *allocation, *encoded;
    meter_nvm_job_t job;
    meter_slots_result_t result;
    meter_record_view_t record;
    bool load, done, outstanding, stop;
};
static int run(void *ctx)
{
    meter_host_nvm_t *h = ctx;
    for (;;)
    {
        if (SDL_SemWait(h->wake) != 0)
            return 1;
        SDL_LockMutex(h->lock);
        bool stop = h->stop, load = h->load;
        meter_nvm_job_t job = h->job;
        SDL_UnlockMutex(h->lock);
        if (stop)
            return 0;
        meter_record_view_t record = {0};
        meter_slots_result_t result;
        if (load)
        {
            result = meter_slots_scan(&h->slots, job.record.product_namespace, job.record.schema);
            if (result == METER_SLOTS_OK)
                (void)meter_slots_active(&h->slots, &record);
        }
        else
            result = meter_slots_commit(&h->slots, &job.record, SIZE_MAX);
        SDL_LockMutex(h->lock);
        h->record = record;
        h->result = result;
        h->done = true;
        SDL_UnlockMutex(h->lock);
    }
}
meter_host_nvm_t *meter_host_nvm_open(meter_core_t *core, const char *path, uint16_t ns, uint16_t schema)
{
    if (!core || !path)
        return NULL;
    const size_t capacity = meter_settings_size(core);
    const size_t slot =
        ((capacity + METER_RECORD_HEADER_SIZE + METER_RECORD_TRAILER_SIZE + 127u) / 128u + 1u) * 128u;
    meter_host_nvm_t *h = calloc(1, sizeof(*h));
    if (!h)
        return NULL;
    h->allocation = calloc(1, 3u * capacity + 3u * slot);
    h->lock = SDL_CreateMutex();
    h->wake = SDL_CreateSemaphore(0);
    if (!h->allocation || !h->lock || !h->wake)
        goto failed;
    h->core = core;
    h->encoded = h->allocation + 2u * capacity;
    if (!meter_nvm_init(&h->service, h->allocation, h->allocation + capacity, capacity, 1u, ns, schema, 500u,
                        3000u) ||
        !meter_file_init(&h->file, path, slot, "host-file", false) ||
        !meter_slots_init(&h->slots, h->allocation + 3u * capacity, 2u * slot,
                          h->allocation + 3u * capacity + 2u * slot, slot) ||
        !meter_slots_bind(&h->slots, &h->file.io))
        goto failed;
    h->load = true;
    h->outstanding = true;
    h->job.generation = h->service.generation;
    h->job.record.product_namespace = ns;
    h->job.record.schema = schema;
    h->thread = SDL_CreateThread(run, "meter-nvm", h);
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
bool meter_host_nvm_changed(meter_host_nvm_t *h, uint32_t now)
{
    if (!h)
        return true;
    return meter_settings_encode(h->core, h->encoded, h->service.capacity) &&
           meter_nvm_observe(&h->service, h->encoded, meter_settings_size(h->core), now);
}
void meter_host_nvm_poll(meter_host_nvm_t *h, uint32_t now)
{
    if (!h)
        return;
    SDL_LockMutex(h->lock);
    bool done = h->done, load = h->load;
    meter_slots_result_t result = h->result;
    meter_record_view_t record = h->record;
    meter_nvm_job_t completed = h->job;
    if (done)
        h->done = false;
    SDL_UnlockMutex(h->lock);
    if (done)
    {
        h->outstanding = false;
        if (load)
        {
            if (result == METER_SLOTS_OK &&
                (record.type != h->service.type ||
                 !meter_settings_decode(h->core, record.payload, record.payload_size)))
                result = METER_SLOTS_INCOMPATIBLE;
            (void)meter_nvm_loaded(&h->service, completed.generation, result, &record);
            (void)meter_host_nvm_changed(h, now);
        }
        else
            (void)meter_nvm_complete(&h->service, completed.generation, completed.revision, result);
    }
    meter_nvm_job_t job;
    if (!h->outstanding && meter_nvm_take(&h->service, now, &job))
    {
        SDL_LockMutex(h->lock);
        h->load = false;
        h->job = job;
        SDL_UnlockMutex(h->lock);
        h->outstanding = true;
        SDL_SemPost(h->wake);
    }
}
const meter_nvm_service_t *meter_host_nvm_status(const meter_host_nvm_t *h)
{
    return h ? &h->service : NULL;
}
bool meter_host_nvm_close(meter_host_nvm_t *h, uint32_t timeout)
{
    if (!h)
        return true;
    meter_nvm_request_save(&h->service);
    const uint32_t start = SDL_GetTicks();
    while (h->outstanding || h->service.dirty)
    {
        meter_host_nvm_poll(h, SDL_GetTicks());
        if (!h->outstanding && (!h->service.writable || !h->service.dirty))
            break;
        if ((uint32_t)(SDL_GetTicks() - start) >= timeout)
            return false;
        SDL_Delay(1);
    }
    const bool durable = meter_nvm_barrier(&h->service, h->service.ram_revision);
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
