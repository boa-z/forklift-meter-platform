#include "runtime/meter_requests.h"

static size_t find_slot(const meter_requests_t *requests, meter_request_id_t id)
{
    for (size_t i = 0u; i < requests->capacity; ++i)
    {
        if ((requests->slots[i].state != METER_REQUEST_SLOT_FREE) &&
            meter_request_id_equal(requests->slots[i].id, id))
        {
            return i;
        }
    }
    return requests->capacity;
}

bool meter_requests_init(meter_requests_t *requests, meter_request_slot_t *slots,
                         size_t capacity, uint32_t session)
{
    if ((requests == NULL) || (slots == NULL) || (capacity == 0u) || (session == 0u) ||
        (capacity > SIZE_MAX / sizeof(*slots)))
    {
        return false;
    }
    for (size_t i = 0u; i < capacity; ++i)
    {
        slots[i] = (meter_request_slot_t){0};
    }
    *requests = (meter_requests_t){slots, capacity, session, 1u};
    return true;
}

meter_request_admission_t meter_requests_reserve(meter_requests_t *requests, meter_request_id_t *id)
{
    if ((requests == NULL) || (id == NULL) || (requests->slots == NULL) || (requests->session == 0u))
    {
        return METER_REQUEST_INVALID;
    }
    if (requests->next_serial == 0u)
    {
        return METER_REQUEST_EXHAUSTED;
    }
    for (size_t i = 0u; i < requests->capacity; ++i)
    {
        if (requests->slots[i].state == METER_REQUEST_SLOT_FREE)
        {
            meter_request_slot_t *slot = &requests->slots[i];
            slot->id = (meter_request_id_t){requests->session, requests->next_serial};
            slot->state = METER_REQUEST_SLOT_QUEUED;
            *id = slot->id;
            requests->next_serial = requests->next_serial == UINT64_MAX ? 0u : requests->next_serial + 1u;
            return METER_REQUEST_QUEUED;
        }
    }
    return METER_REQUEST_BUSY;
}

bool meter_requests_start(meter_requests_t *requests, meter_request_id_t id)
{
    if (requests == NULL)
    {
        return false;
    }
    const size_t index = find_slot(requests, id);
    if ((index == requests->capacity) || (requests->slots[index].state != METER_REQUEST_SLOT_QUEUED))
    {
        return false;
    }
    requests->slots[index].state = METER_REQUEST_SLOT_ACTIVE;
    return true;
}

bool meter_requests_cancel(meter_requests_t *requests, meter_request_id_t id)
{
    if (requests == NULL)
    {
        return false;
    }
    const size_t index = find_slot(requests, id);
    if ((index == requests->capacity) || (requests->slots[index].state != METER_REQUEST_SLOT_QUEUED))
    {
        return false;
    }
    requests->slots[index].result = (meter_request_result_t){id, METER_RESULT_CANCELLED_BEFORE_IO, 0u, 0};
    requests->slots[index].state = METER_REQUEST_SLOT_TERMINAL;
    return true;
}

bool meter_requests_finish(meter_requests_t *requests, const meter_request_result_t *result)
{
    if ((requests == NULL) || (result == NULL) || (result->code < METER_RESULT_APPLIED) ||
        (result->code > METER_RESULT_SUPERSEDED) || (result->code == METER_RESULT_CANCELLED_BEFORE_IO))
    {
        return false;
    }
    const size_t index = find_slot(requests, result->id);
    if ((index == requests->capacity) || (requests->slots[index].state != METER_REQUEST_SLOT_ACTIVE))
    {
        return false;
    }
    requests->slots[index].result = *result;
    requests->slots[index].state = METER_REQUEST_SLOT_TERMINAL;
    return true;
}

bool meter_requests_query(const meter_requests_t *requests, meter_request_id_t id,
                          meter_request_result_t *out)
{
    if ((requests == NULL) || (out == NULL))
    {
        return false;
    }
    const size_t index = find_slot(requests, id);
    if ((index == requests->capacity) || (requests->slots[index].state != METER_REQUEST_SLOT_TERMINAL))
    {
        return false;
    }
    *out = requests->slots[index].result;
    return true;
}

bool meter_requests_acknowledge(meter_requests_t *requests, meter_request_id_t id)
{
    if (requests == NULL)
    {
        return false;
    }
    const size_t index = find_slot(requests, id);
    if ((index == requests->capacity) || (requests->slots[index].state != METER_REQUEST_SLOT_TERMINAL))
    {
        return false;
    }
    requests->slots[index].state = METER_REQUEST_SLOT_FREE;
    return true;
}
