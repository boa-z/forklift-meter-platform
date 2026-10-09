#include "runtime/meter_periodic.h"
#include "contracts/meter_time.h"
#include "runtime/meter_execution.h"
#include <string.h>

static bool value_arrays_separate(const meter_tx_value_t *left, const meter_tx_value_t *right, size_t count)
{
    if (!count)
        return true;
    if (!left || !right || count > SIZE_MAX / sizeof(*left))
        return false;
    uintptr_t left_address = (uintptr_t)left, right_address = (uintptr_t)right;
    size_t byte_count = count * sizeof(*left);
    return left_address < right_address ? right_address - left_address >= byte_count
                                        : left_address - right_address >= byte_count;
}
bool meter_tx_publish(meter_tx_publication_t *publication, size_t capacity, const meter_tx_value_t *values,
                      size_t count, uint32_t generation, uint32_t now)
{
    if (!publication || !generation || count > capacity ||
        !value_arrays_separate(publication->values, values, count))
        return false;
    /* Validate the complete input before copying; rejection leaves publication intact. */
    bool changed = !publication->revision || publication->count != count;
    for (size_t i = 0u; i < count; ++i)
    {
        if (values[i].valid && now - values[i].sample_ms >= METER_TIME_HALF_RANGE)
            return false;
        if (publication->generation == generation && i < publication->count && publication->values[i].valid &&
            values[i].valid &&
            values[i].sample_ms - publication->values[i].sample_ms >= METER_TIME_HALF_RANGE)
            return false;
        if (i >= publication->count || values[i].value != publication->values[i].value ||
            values[i].valid != publication->values[i].valid)
            changed = true;
    }
    /* Only value/validity/count changes consume a semantic revision, not sample time. */
    if (changed && publication->revision == UINT64_MAX)
        return false;
    if (count)
        memcpy(publication->values, values, count * sizeof(*values));
    if (changed)
        ++publication->revision;
    publication->generation = generation;
    publication->published_ms = now;
    publication->count = count;
    return true;
}
bool meter_tx_copy(meter_tx_publication_t *destination, size_t capacity, const meter_tx_publication_t *source)
{
    if (!destination || !source || source->count > capacity ||
        !value_arrays_separate(destination->values, source->values, source->count))
        return false;
    meter_tx_value_t *destination_values = destination->values;
    if (source->count)
        memcpy(destination_values, source->values, source->count * sizeof(*destination_values));
    *destination = *source;
    destination->values = destination_values;
    return true;
}
bool meter_periodic_valid(const meter_periodic_frame_t *definition, size_t value_count)
{
    return definition && definition->period_ms && definition->period_ms < METER_TIME_HALF_RANGE &&
           (unsigned)definition->frame.bus < METER_BUS_COUNT && definition->frame.size <= 8u &&
           definition->max_age_ms < METER_TIME_HALF_RANGE && definition->expiry_ms <= definition->period_ms &&
           definition->first_value <= value_count &&
           definition->value_count <= value_count - definition->first_value &&
           (unsigned)definition->freshness <= METER_TX_SUPPRESS &&
           (unsigned)definition->backlog <= METER_TX_REPLACE_PENDING &&
           (unsigned)definition->commit <= METER_TX_COMMIT_DRIVER;
}
bool meter_periodic_reset(meter_periodic_state_t *state, const meter_periodic_frame_t *definition,
                          uint32_t generation, uint32_t now)
{
    if (!state || !definition || !generation || !definition->period_ms ||
        definition->period_ms >= METER_TIME_HALF_RANGE)
        return false;
    *state = (meter_periodic_state_t){.generation = generation, .wire = definition->initial_wire};
    return meter_deadline_arm(&state->deadline, now, definition->period_ms);
}
bool meter_periodic_prepare(meter_periodic_state_t *state, const meter_periodic_frame_t *definition,
                            const meter_tx_publication_t *publication, bool allowed, uint32_t now,
                            meter_periodic_message_t *message)
{
    if (!state || !definition || !message || !definition->period_ms)
        return false;
    /* Consume this scheduled opportunity even when policy, freshness or encoding rejects it. */
    uint32_t due = state->deadline.next_ms;
    if (!meter_deadline_take(&state->deadline, now))
        return false;
    /* After a pause, use the latest planned deadline; never replay missed frames. */
    due += ((now - due) / definition->period_ms) * definition->period_ms;
    uint32_t expiry = due + (definition->expiry_ms ? definition->expiry_ms : definition->period_ms);
    if (!allowed || state->busy || meter_time_reached(now, expiry) || state->ticket == UINT64_MAX)
    {
        ++state->skipped;
        return false;
    }
    /* Static frames need no publication; dynamic frames validate their sample window. */
    bool fresh = true;
    if (definition->encode)
    {
        if (!publication || !publication->revision || publication->generation != state->generation ||
            !meter_periodic_valid(definition, publication->count) ||
            (publication->count && !publication->values))
        {
            ++state->rejected;
            return false;
        }
        for (size_t i = definition->first_value; i < definition->first_value + definition->value_count; ++i)
        {
            uint32_t age = now - publication->values[i].sample_ms;
            if (!publication->values[i].valid || age >= METER_TIME_HALF_RANGE ||
                (definition->max_age_ms && age >= definition->max_age_ms))
                fresh = false;
        }
        if (!fresh)
            ++state->stale;
        if (!fresh && definition->freshness == METER_TX_SUPPRESS)
            return false;
    }
    /* Encode locally. A Product encoder may change payload, but not routing identity. */
    meter_can_frame_t frame = definition->frame;
    uint32_t next_wire = state->wire;
    meter_tx_snapshot_t view = {0};
    if (publication)
        view = (meter_tx_snapshot_t){.generation = publication->generation,
                                     .published_ms = publication->published_ms,
                                     .revision = publication->revision,
                                     .count = publication->count,
                                     .values = publication->values};
    if (definition->encode && !definition->encode(&view, fresh, state->wire, &frame, &next_wire))
    {
        ++state->rejected;
        return false;
    }
    if (frame.bus != definition->frame.bus || frame.id != definition->frame.id ||
        frame.extended != definition->frame.extended || frame.remote != definition->frame.remote ||
        frame.size > 8u)
    {
        ++state->rejected;
        return false;
    }
    /* Preparation reserves an identity, not delivery. Commit wire state at the selected stage. */
    state->proposed_wire = next_wire;
    *message = (meter_periodic_message_t){.generation = state->generation,
                                          .ticket = ++state->ticket,
                                          .revision = publication ? publication->revision : 0u,
                                          .published_ms = publication ? publication->published_ms : 0u,
                                          .deadline_ms = due,
                                          .expiry_ms = expiry,
                                          .queued_ms = now,
                                          .frame = frame};
    return true;
}
bool meter_periodic_admit(meter_periodic_state_t *state, const meter_periodic_frame_t *definition,
                          const meter_periodic_message_t *message)
{
    if (!state || !definition || !message || state->busy || !message->ticket ||
        message->generation != state->generation || message->ticket != state->ticket ||
        message->ticket == state->pending_ticket)
        return false;
    state->busy = true;
    state->pending_ticket = message->ticket;
    if (definition->commit == METER_TX_COMMIT_ADMISSION)
        state->wire = state->proposed_wire;
    return true;
}
bool meter_periodic_complete(meter_periodic_state_t *state, const meter_periodic_frame_t *definition,
                             const meter_periodic_result_t *result)
{
    if (!state || !definition || !result || !state->busy || result->generation != state->generation ||
        result->ticket != state->pending_ticket)
        return false;
    state->busy = false;
    if (result->success)
    {
        ++state->completed;
        if (definition->commit == METER_TX_COMMIT_DRIVER)
            state->wire = state->proposed_wire;
    }
    else
        ++state->failed;
    return true;
}
bool meter_periodic_sendable(const meter_periodic_message_t *message, uint32_t generation, uint32_t now)
{
    return message && message->generation == generation && !meter_time_reached(now, message->expiry_ms);
}
