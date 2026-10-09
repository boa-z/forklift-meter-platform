#include "runtime/meter_batch_builder.h"

bool meter_batch_builder_init(meter_batch_builder_t *builder, meter_update_t *storage, size_t capacity)
{
    if ((builder == NULL) || (storage == NULL) || (capacity == 0u) ||
        (capacity > SIZE_MAX / sizeof(*storage)))
    {
        return false;
    }
    *builder = (meter_batch_builder_t){0};
    builder->storage = storage;
    builder->capacity = capacity;
    return true;
}

bool meter_batch_builder_begin(meter_batch_builder_t *builder, const meter_update_batch_t *metadata)
{
    if ((builder == NULL) || (metadata == NULL) || (builder->storage == NULL) || builder->active ||
        (metadata->generation == 0u) || (metadata->sequence == 0u) ||
        (metadata->source == METER_SOURCE_NONE) || (metadata->updates != NULL) || (metadata->count != 0u))
    {
        return false;
    }
    builder->batch = *metadata;
    builder->batch.updates = builder->storage;
    builder->active = true;
    builder->error = METER_BATCH_QUEUED;
    return true;
}

bool meter_batch_builder_add(void *context, const meter_update_t *update)
{
    meter_batch_builder_t *builder = context;
    if ((builder == NULL) || !builder->active || (builder->error != METER_BATCH_QUEUED))
    {
        return false;
    }
    if ((update == NULL) || (update->signal == 0u) || (update->value.source != builder->batch.source) ||
        (update->value.state < METER_VALUE_UNKNOWN) || (update->value.state > METER_VALUE_ERROR))
    {
        builder->error = METER_BATCH_INVALID;
        return false;
    }
    for (size_t i = 0u; i < builder->batch.count; ++i)
    {
        if (builder->storage[i].signal == update->signal)
        {
            builder->error = METER_BATCH_INVALID;
            return false;
        }
    }
    if (builder->batch.count == builder->capacity)
    {
        builder->error = METER_BATCH_TOO_LARGE;
        return false;
    }
    builder->storage[builder->batch.count] = *update;
    ++builder->batch.count;
    return true;
}

meter_batch_result_t meter_batch_builder_finish(meter_batch_builder_t *builder, bool decoded,
                                               meter_batch_submit_fn_t submit, void *context)
{
    if ((builder == NULL) || !builder->active)
    {
        return METER_BATCH_INVALID;
    }
    meter_batch_result_t result = builder->error;
    if ((result == METER_BATCH_QUEUED) && !decoded)
    {
        result = METER_BATCH_DECODE_FAILED;
    }
    if ((result == METER_BATCH_QUEUED) && (builder->batch.count != 0u))
    {
        result = submit == NULL ? METER_BATCH_INVALID : submit(context, &builder->batch);
    }
    builder->active = false;
    return result;
}
