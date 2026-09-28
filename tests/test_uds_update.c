#include "protocols/uds/meter_uds.h"
#include <assert.h>
#include <string.h>
static uint32_t now;
uint32_t UDSMillis(void)
{
    return now;
}
static uint8_t request[1024], response[1024];
static size_t request_n, response_n;
static unsigned submits, cancellations;
static uint32_t response_time;
static bool done;
static meter_update_error_t completion_error;
static meter_update_job_t saved;
static UDSErr_t tx(UDSTp_t *p, const uint8_t *d, size_t n, const UDSSDU_t *i)
{
    (void)p;
    (void)i;
    assert(n <= sizeof(response));
    memcpy(response, d, n);
    response_n = n;
    response_time = now;
    return UDS_OK;
}
static UDSErr_t rx(UDSTp_t *p, uint8_t *d, size_t n, size_t *out, UDSSDU_t *i)
{
    (void)p;
    assert(n >= request_n);
    *out = request_n;
    if (request_n)
        memcpy(d, request, request_n);
    request_n = 0;
    memset(i, 0, sizeof(*i));
    return UDS_OK;
}
static UDSErr_t poll(UDSTp_t *p)
{
    (void)p;
    return UDS_OK;
}
static bool submit(void *c, const meter_update_job_t *j)
{
    (void)c;
    saved = *j;
    submits++;
    return true;
}
static bool result(void *c, meter_update_error_t *e, uint32_t *gen, uint32_t *offset)
{
    (void)c;
    if (!done)
        return false;
    *e = completion_error;
    *gen = 1;
    if (saved.kind == METER_UPDATE_JOB_BEGIN)
        *offset = 0;
    else if (saved.kind == METER_UPDATE_JOB_WRITE)
        *offset = saved.offset + (uint32_t)saved.size;
    return true;
}
static size_t info(void *c, uint8_t *d, size_t n)
{
    (void)c;
    assert(n >= 2);
    memcpy(d, "{}", 2);
    return 2;
}
static bool can_reset(void *c)
{
    (void)c;
    return false;
}
static void noop(void *c)
{
    (void)c;
}
static void cancel(void *c)
{
    (void)c;
    cancellations++;
}
static void send(meter_uds_t *u, const uint8_t *d, size_t n)
{
    memcpy(request, d, n);
    request_n = n;
    response_n = 0;
    for (unsigned i = 0; i < 60; ++i)
    {
        now++;
        UDSServerPoll(&u->server);
    }
}
int main(void)
{
    UDSTp_t tp = {.send = tx, .recv = rx, .poll = poll};
    meter_uds_port_t p = {0, submit, result, info, can_reset, noop, cancel};
    meter_uds_t u;
    assert(meter_uds_init(&u, &tp, &p));
    assert(u.server.p2_ms == 1 && u.server.p2_star_ms == 5000);
    uint8_t session[] = {0x10, 2};
    send(&u, session, 2);
    assert(response[0] == 0x50);
    uint8_t short_req[] = {0x34, 0, 0x44, 0};
    send(&u, short_req, sizeof(short_req));
    assert(response[0] == 0x7f && submits == 0);
    uint8_t meta[151] = {0x2e, 0xf1, 0x80};
    memcpy(meta + 3, "synthetic", 10);
    memcpy(meta + 35, "board", 6);
    memcpy(meta + 67, "v2", 3);
    /* 260 字节允许验证标准计数器跨 0xff 回到 0。 */
    meta[3 + 114] = 1;
    meta[3 + 115] = 4;
    send(&u, meta, sizeof(meta));
    assert(response[0] == 0x6e);
    uint8_t download[] = {0x34, 0, 0x44, 0, 0, 0, 0, 0, 0, 1, 4};
    done = false;
    send(&u, download, sizeof(download));
    assert(submits == 1 && response[0] == 0x7f && response[2] == 0x78);
    for (unsigned i = 0; i < 10; i++)
    {
        now++;
        UDSServerPoll(&u.server);
    }
    assert(submits == 1);
    done = true;
    uint32_t completed_at = now;
    for (unsigned i = 0; i < 60; i++)
    {
        now++;
        UDSServerPoll(&u.server);
    }
    assert(response[0] == 0x74 && response_time - completed_at <= 3);
    for (unsigned i = 0; i < 260; i++)
    {
        uint8_t block[] = {0x36, (uint8_t)(i + 1u), 0x5a};
        send(&u, block, 3);
        if (!(response[0] == 0x76 && saved.offset == i && saved.data[0] == 0x5a))
            fprintf(stderr, "i=%u response=%02x %02x %02x offset=%u submit=%u session=%u\n", i, response[0],
                    response[1], response[2], saved.offset, submits, u.server.sessionType);
        assert(response[0] == 0x76 && saved.offset == i && saved.data[0] == 0x5a);
    }
    uint8_t exit_request[] = {0x37};
    send(&u, exit_request, 1);
    assert(response[0] == 0x77 && saved.kind == METER_UPDATE_JOB_VERIFY);
    uint8_t reset[] = {0x11, 1};
    send(&u, reset, 2);
    assert(response[0] == 0x7f && response[2] == 0x22);
    send(&u, download, sizeof(download));
    assert(response[0] == 0x74);
    uint8_t wrong[] = {0x36, 2, 0x55};
    unsigned before = submits;
    send(&u, wrong, 3);
    assert(response[0] == 0x7f && submits == before);
    /* 能力不足必须返回标准条件拒绝，不能建立传输会话。 */
    assert(meter_uds_init(&u, &tp, &p));
    send(&u, session, sizeof(session));
    send(&u, meta, sizeof(meta));
    completion_error = METER_UPDATE_UNSUPPORTED;
    send(&u, download, sizeof(download));
    assert(response[0] == 0x7f && response[2] == 0x22 && !u.server.xferIsActive);
    /* 未完成异步请求超时后必须清除协议 pending，允许新会话重新建立。 */
    assert(meter_uds_init(&u, &tp, &p));
    completion_error = METER_UPDATE_OK;
    send(&u, session, sizeof(session));
    send(&u, meta, sizeof(meta));
    done = false;
    send(&u, download, sizeof(download));
    assert(u.pending);
    unsigned cancelled = cancellations;
    assert(u.server.fn(&u.server, UDS_EVT_SessionTimeout, NULL) == UDS_PositiveResponse);
    assert(!u.pending && !u.manifest_valid && !u.server.xferIsActive && cancellations == cancelled + 1);
    assert(u.server.p2_ms == 1);
    done = true;
    send(&u, session, sizeof(session));
    send(&u, meta, sizeof(meta));
    send(&u, download, sizeof(download));
    assert(response[0] == 0x74);
    uint8_t default_session[] = {0x10, 1};
    send(&u, default_session, sizeof(default_session));
    assert(!u.manifest_valid && !u.pending && cancellations == cancelled + 2);
    return 0;
}
