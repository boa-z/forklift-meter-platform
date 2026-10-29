/* DID v2 线协议契约：固定长度、字节序、会话权限、NRC 与 Manifest 生命周期。 */
#include "protocols/uds/hmi_uds.h"
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
static bool done, reset_allowed;
static hmi_update_error_t completion_error;
static hmi_update_job_t saved;
static hmi_uds_snapshot_t snap;

static UDSErr_t tx(UDSTp_t *p, const uint8_t *d, size_t n, const UDSSDU_t *i)
{
    (void)p;
    (void)i;
    assert(n <= sizeof(response));
    memcpy(response, d, n);
    response_n = n;
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
static bool submit(void *c, const hmi_update_job_t *j)
{
    (void)c;
    saved = *j;
    submits++;
    return true;
}
static bool result(void *c, hmi_update_error_t *e, uint32_t *gen, uint32_t *offset)
{
    (void)c;
    if (!done)
        return false;
    *e = completion_error;
    *gen = 1;
    if (saved.kind == HMI_UPDATE_JOB_BEGIN)
        *offset = 0;
    else if (saved.kind == HMI_UPDATE_JOB_WRITE)
        *offset = saved.offset + (uint32_t)saved.size;
    return true;
}
static bool snapshot_port(void *c, hmi_uds_snapshot_t *out)
{
    (void)c;
    *out = snap;
    return true;
}
static bool can_reset(void *c)
{
    (void)c;
    return reset_allowed;
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
static int maintenance_on = -1;
static void maintenance(void *c, bool on)
{
    (void)c;
    maintenance_on = on;
}

static void send(hmi_uds_t *u, const uint8_t *d, size_t n)
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
/* 让异步 worker 完成并把权威状态交回端口快照。 */
static void complete(hmi_uds_t *u)
{
    done = true;
    if (saved.kind == HMI_UPDATE_JOB_BEGIN)
    {
        snap.manifest = saved.manifest;
        snap.manifest_valid = true;
        snap.state = HMI_UPDATE_DOWNLOADING;
        snap.total = saved.manifest.size;
    }
    else if (saved.kind == HMI_UPDATE_JOB_VERIFY)
    {
        snap.state = HMI_UPDATE_CANDIDATE;
        snap.candidate_verified = true;
        snap.received = snap.total;
    }
    else if (saved.kind == HMI_UPDATE_JOB_ACTIVATE)
    {
        snap.state = HMI_UPDATE_ACTIVATED;
        snap.candidate_verified = true;
    }
    for (unsigned i = 0; i < 60; ++i)
    {
        now++;
        UDSServerPoll(&u->server);
    }
}
static void fill_manifest(uint8_t *out, const char *product, const char *hardware, const char *version,
                          uint32_t size, uint8_t seed)
{
    memset(out, 0, HMI_UDS_MANIFEST_SIZE);
    memcpy(out, product, strlen(product));
    memcpy(out + 32, hardware, strlen(hardware));
    memcpy(out + 64, version, strlen(version));
    out[112] = (uint8_t)(size >> 24);
    out[113] = (uint8_t)(size >> 16);
    out[114] = (uint8_t)(size >> 8);
    out[115] = (uint8_t)size;
    for (unsigned i = 0; i < 32; ++i)
        out[116 + i] = (uint8_t)(seed + i);
}
static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static void setup(void)
{
    now = 0;
    submits = cancellations = 0;
    done = false;
    reset_allowed = false;
    maintenance_on = -1;
    completion_error = HMI_UPDATE_OK;
    memset(&saved, 0, sizeof(saved));
    memset(&snap, 0, sizeof(snap));
    memset(response, 0, sizeof(response));
    response_n = 0;
    snap.state = HMI_UPDATE_IDLE;
    snap.error = HMI_UPDATE_OK;
    snap.backend_supported = true;
    snap.capacity = 4096;
    strcpy(snap.product, "synthetic");
    strcpy(snap.hardware, "reference-board");
    strcpy(snap.version, "v1");
    strcpy(snap.os_file, "d13x_os.itb");
    strcpy(snap.reason, "native_auto_unverified");
}
static void init(hmi_uds_t *u, UDSTp_t *tp)
{
    hmi_uds_port_t p = {0, submit, result, snapshot_port, can_reset, noop, cancel, maintenance};
    assert(hmi_uds_init(u, tp, &p));
}
static void program(hmi_uds_t *u)
{
    const uint8_t session[] = {0x10, 2};
    send(u, session, sizeof(session));
    assert(response[0] == 0x50 && response[1] == 0x02);
}

int main(void)
{
    UDSTp_t tp = {.send = tx, .recv = rx, .poll = poll};
    hmi_uds_t u;
    uint8_t manifest[HMI_UDS_MANIFEST_SIZE];

    /* 1. F180 彻底移除：读写都必须是 RequestOutOfRange。 */
    setup();
    init(&u, &tp);
    {
        const uint8_t read_f180[] = {0x22, 0xf1, 0x80};
        send(&u, read_f180, sizeof(read_f180));
        assert(response[0] == 0x7f && response[1] == 0x22 && response[2] == 0x31);
        uint8_t write_f180[3 + HMI_UDS_MANIFEST_SIZE] = {0x2e, 0xf1, 0x80};
        program(&u);
        send(&u, write_f180, sizeof(write_f180));
        assert(response[0] == 0x7f && response[1] == 0x2e && response[2] == 0x31);
    }

    /* 2. 无 Manifest 时 FD10 读回全零。 */
    setup();
    init(&u, &tp);
    {
        const uint8_t read_manifest[] = {0x22, 0xfd, 0x10};
        send(&u, read_manifest, sizeof(read_manifest));
        assert(response_n == 3u + HMI_UDS_MANIFEST_SIZE && response[0] == 0x62 && response[1] == 0xfd &&
               response[2] == 0x10);
        for (unsigned i = 0; i < HMI_UDS_MANIFEST_SIZE; ++i)
            assert(response[3 + i] == 0);
    }

    /* 3. FD10 148B 写入后必须原样读回。 */
    setup();
    init(&u, &tp);
    {
        const uint8_t read_manifest[] = {0x22, 0xfd, 0x10};
        fill_manifest(manifest, "synthetic", "reference-board", "v2", 260, 0x11);
        uint8_t write[3 + HMI_UDS_MANIFEST_SIZE];
        write[0] = 0x2e;
        write[1] = 0xfd;
        write[2] = 0x10;
        memcpy(write + 3, manifest, sizeof(manifest));
        program(&u);
        send(&u, write, sizeof(write));
        assert(response_n == 3 && response[0] == 0x6e && response[1] == 0xfd && response[2] == 0x10);
        send(&u, read_manifest, sizeof(read_manifest));
        assert(response_n == 3u + HMI_UDS_MANIFEST_SIZE &&
               memcmp(response + 3, manifest, sizeof(manifest)) == 0);
    }

    /* 4. 会话、长度、语法、零长度都必须按契约拒绝。 */
    setup();
    init(&u, &tp);
    {
        uint8_t write[3 + HMI_UDS_MANIFEST_SIZE];
        write[0] = 0x2e;
        write[1] = 0xfd;
        write[2] = 0x10;
        fill_manifest(manifest, "synthetic", "reference-board", "v2", 260, 0x11);
        memcpy(write + 3, manifest, sizeof(manifest));
        send(&u, write, sizeof(write)); /* 默认会话 */
        assert(response[0] == 0x7f && response[1] == 0x2e && response[2] == 0x7f);
        program(&u);
        send(&u, write, sizeof(write) - 1); /* 147B */
        assert(response[0] == 0x7f && response[2] == 0x13);
        fill_manifest(manifest, "synthetic", "reference-board", "v2\"bad", 260, 0x11);
        memcpy(write + 3, manifest, sizeof(manifest));
        send(&u, write, sizeof(write));
        assert(response[0] == 0x7f && response[2] == 0x31);
        fill_manifest(manifest, "synthetic", "reference-board", "v2", 0, 0x11);
        memcpy(write + 3, manifest, sizeof(manifest));
        send(&u, write, sizeof(write));
        assert(response[0] == 0x7f && response[2] == 0x31);
        assert(!u.staged_valid);
    }

    /* 5. 有 worker 候选时禁止覆盖 Manifest 身份。 */
    setup();
    init(&u, &tp);
    {
        uint8_t write[3 + HMI_UDS_MANIFEST_SIZE];
        write[0] = 0x2e;
        write[1] = 0xfd;
        write[2] = 0x10;
        fill_manifest(manifest, "synthetic", "reference-board", "v2", 260, 0x11);
        memcpy(write + 3, manifest, sizeof(manifest));
        program(&u);
        snap.manifest_valid = true;
        snap.state = HMI_UPDATE_DOWNLOADING;
        send(&u, write, sizeof(write));
        assert(response[0] == 0x7f && response[1] == 0x2e && response[2] == 0x22);
    }

    /* 6. 0x34 把 staged 交给 worker；此后 FD10 读 worker 副本。 */
    setup();
    init(&u, &tp);
    {
        const uint8_t read_manifest[] = {0x22, 0xfd, 0x10};
        uint8_t write[3 + HMI_UDS_MANIFEST_SIZE];
        write[0] = 0x2e;
        write[1] = 0xfd;
        write[2] = 0x10;
        fill_manifest(manifest, "synthetic", "reference-board", "v2", 260, 0x11);
        memcpy(write + 3, manifest, sizeof(manifest));
        program(&u);
        send(&u, write, sizeof(write));
        assert(u.staged_valid);
        uint8_t download[] = {0x34, 0x00, 0x44, 0, 0, 0, 0, 0, 0, 1, 4};
        done = false;
        send(&u, download, sizeof(download));
        assert(submits == 1 && response[0] == 0x7f && response[2] == 0x78);
        assert(u.staged_valid); /* 0x78 期间不得提前清除 */
        complete(&u);
        assert(response[0] == 0x74 && !u.staged_valid);
        send(&u, read_manifest, sizeof(read_manifest));
        assert(response_n == 3u + HMI_UDS_MANIFEST_SIZE &&
               memcmp(response + 3, manifest, sizeof(manifest)) == 0);
    }

    /* 7. 0x34 的前置条件：默认会话 → 0x7f；无 staged → 0x22。 */
    setup();
    init(&u, &tp);
    {
        uint8_t download[] = {0x34, 0x00, 0x44, 0, 0, 0, 0, 0, 0, 1, 4};
        send(&u, download, sizeof(download));
        assert(response[0] == 0x7f && response[2] == 0x7f);
        program(&u);
        send(&u, download, sizeof(download));
        assert(response[0] == 0x7f && response[2] == 0x22 && submits == 0);
    }

    /* 8. FD11 恰好 12B，状态/错误编号、flags 与大端整数固定。 */
    setup();
    init(&u, &tp);
    {
        const uint8_t read_status[] = {0x22, 0xfd, 0x11};
        snap.state = HMI_UPDATE_DOWNLOADING;
        snap.error = HMI_UPDATE_NVM;
        snap.received = 0x01020304u;
        snap.total = 0x05060708u;
        snap.maintenance = true;
        snap.exclusive = true;
        snap.admitted = true;
        snap.manifest_valid = true;
        snap.candidate_verified = false;
        send(&u, read_status, sizeof(read_status));
        assert(response_n == 3u + HMI_UDS_STATUS_SIZE && response[0] == 0x62 && response[2] == 0x11);
        assert(response[3] == 1u && response[4] == 9u);
        assert(response[5] ==
               (HMI_UDS_FLAG_BACKEND_SUPPORTED | HMI_UDS_FLAG_MAINTENANCE | HMI_UDS_FLAG_EXCLUSIVE |
                HMI_UDS_FLAG_ADMISSION | HMI_UDS_FLAG_MANIFEST_PRESENT));
        assert(response[6] == 0u);
        assert(be32(response + 7) == 0x01020304u && be32(response + 11) == 0x05060708u);
        snap.state = HMI_UPDATE_CANDIDATE;
        snap.candidate_verified = true;
        snap.received = 260;
        snap.total = 260;
        send(&u, read_status, sizeof(read_status));
        assert(response[3] == 4u && (response[5] & HMI_UDS_FLAG_CANDIDATE_VERIFIED));
    }

    /* 9. FD12 恰好 164B，字符串 NUL 终止并零填充，容量大端。 */
    setup();
    init(&u, &tp);
    {
        const uint8_t read_endpoint[] = {0x22, 0xfd, 0x12};
        snap.capacity = 0x00010000u;
        send(&u, read_endpoint, sizeof(read_endpoint));
        assert(response_n == 3u + HMI_UDS_ENDPOINT_SIZE && response[0] == 0x62 && response[2] == 0x12);
        assert(strcmp((const char *)response + 3, "synthetic") == 0);
        assert(strcmp((const char *)response + 35, "reference-board") == 0);
        assert(strcmp((const char *)response + 67, "v1") == 0);
        assert(strcmp((const char *)response + 115, "d13x_os.itb") == 0);
        assert(be32(response + 131) == 0x00010000u);
        assert(strcmp((const char *)response + 135, "native_auto_unverified") == 0);
        /* 每个字段的尾部必须是零填充，不能残留栈数据。 */
        assert(response[3 + 31] == 0u && response[3 + 63] == 0u && response[3 + 111] == 0u);
        assert(response[3 + 127] == 0u && response[3 + 163] == 0u);
    }

    /* 10. F186 会话、F192 硬件、F195 运行版本。 */
    setup();
    init(&u, &tp);
    {
        const uint8_t read_session[] = {0x22, 0xf1, 0x86};
        send(&u, read_session, sizeof(read_session));
        assert(response_n == 4 && response[0] == 0x62 && response[3] == 0x01);
        program(&u);
        send(&u, read_session, sizeof(read_session));
        assert(response_n == 4 && response[3] == 0x02);
        const uint8_t read_hardware[] = {0x22, 0xf1, 0x92};
        send(&u, read_hardware, sizeof(read_hardware));
        assert(response_n == 3u + HMI_UDS_HARDWARE_SIZE &&
               strcmp((const char *)response + 3, "reference-board") == 0);
        const uint8_t read_version[] = {0x22, 0xf1, 0x95};
        send(&u, read_version, sizeof(read_version));
        assert(response_n == 3u + HMI_UDS_SOFTWARE_SIZE && strcmp((const char *)response + 3, "v1") == 0);
    }

    /* 11. 多 DID 组合读取：FD10 + FD11 + FD12 一次拼包。 */
    setup();
    init(&u, &tp);
    {
        const uint8_t multi[] = {0x22, 0xfd, 0x10, 0xfd, 0x11, 0xfd, 0x12};
        memcpy(snap.manifest.product, "synthetic", 10);
        snap.manifest.size = 260;
        snap.manifest_valid = true;
        send(&u, multi, sizeof(multi));
        assert(response_n ==
               1u + 2u + HMI_UDS_MANIFEST_SIZE + 2u + HMI_UDS_STATUS_SIZE + 2u + HMI_UDS_ENDPOINT_SIZE);
        assert(response[1] == 0xfd && response[2] == 0x10);
        assert(response[3 + HMI_UDS_MANIFEST_SIZE] == 0xfd &&
               response[4 + HMI_UDS_MANIFEST_SIZE] == 0x11);
        assert(response[5 + HMI_UDS_MANIFEST_SIZE + HMI_UDS_STATUS_SIZE] == 0xfd &&
               response[6 + HMI_UDS_MANIFEST_SIZE + HMI_UDS_STATUS_SIZE] == 0x12);
    }

    /* 12. S3 超时清除 staged，但已验证候选继续可读。 */
    setup();
    init(&u, &tp);
    {
        const uint8_t read_manifest[] = {0x22, 0xfd, 0x10};
        uint8_t write[3 + HMI_UDS_MANIFEST_SIZE];
        write[0] = 0x2e;
        write[1] = 0xfd;
        write[2] = 0x10;
        fill_manifest(manifest, "synthetic", "reference-board", "v2", 260, 0x11);
        memcpy(write + 3, manifest, sizeof(manifest));
        program(&u);
        send(&u, write, sizeof(write));
        assert(u.staged_valid);
        assert(u.server.fn(&u.server, UDS_EVT_SessionTimeout, NULL) == UDS_PositiveResponse);
        assert(!u.staged_valid && maintenance_on == 0 && cancellations == 1);
        send(&u, read_manifest, sizeof(read_manifest));
        assert(response_n == 3u + HMI_UDS_MANIFEST_SIZE);
        for (unsigned i = 0; i < HMI_UDS_MANIFEST_SIZE; ++i)
            assert(response[3 + i] == 0);
        /* 候选属于 worker，S3 后必须继续可读。 */
        memcpy(snap.manifest.product, "synthetic", 10);
        snap.manifest_valid = true;
        snap.candidate_verified = true;
        snap.state = HMI_UPDATE_CANDIDATE;
        send(&u, read_manifest, sizeof(read_manifest));
        assert(strcmp((const char *)response + 3, "synthetic") == 0);
    }

    /* 13. FAILED/ABORTED 不得回读旧 worker Manifest；重新 0x2E 后可读新 staged。 */
    setup();
    init(&u, &tp);
    {
        const uint8_t read_manifest[] = {0x22, 0xfd, 0x10};
        uint8_t write[3 + HMI_UDS_MANIFEST_SIZE];
        write[0] = 0x2e;
        write[1] = 0xfd;
        write[2] = 0x10;
        fill_manifest(manifest, "synthetic", "reference-board", "v3", 260, 0x22);
        memcpy(write + 3, manifest, sizeof(manifest));
        memcpy(snap.manifest.product, "stale", 5);
        snap.manifest_valid = false;
        snap.state = HMI_UPDATE_FAILED;
        send(&u, read_manifest, sizeof(read_manifest));
        for (unsigned i = 0; i < HMI_UDS_MANIFEST_SIZE; ++i)
            assert(response[3 + i] == 0);
        program(&u);
        send(&u, write, sizeof(write));
        assert(response[0] == 0x6e);
        send(&u, read_manifest, sizeof(read_manifest));
        assert(strcmp((const char *)response + 3, "synthetic") == 0 &&
               strcmp((const char *)response + 67, "v3") == 0);
    }

    /* 14. 0x36/0x37 异步链路与块计数器回绕保持可用。 */
    setup();
    init(&u, &tp);
    {
        uint8_t write[3 + HMI_UDS_MANIFEST_SIZE];
        write[0] = 0x2e;
        write[1] = 0xfd;
        write[2] = 0x10;
        fill_manifest(manifest, "synthetic", "reference-board", "v2", 260, 0x11);
        memcpy(write + 3, manifest, sizeof(manifest));
        program(&u);
        send(&u, write, sizeof(write));
        uint8_t download[] = {0x34, 0x00, 0x44, 0, 0, 0, 0, 0, 0, 1, 4};
        done = false;
        send(&u, download, sizeof(download));
        assert(response[0] == 0x7f && response[2] == 0x78);
        complete(&u);
        assert(response[0] == 0x74);
        for (unsigned i = 0; i < 260; ++i)
        {
            uint8_t block[] = {0x36, (uint8_t)(i + 1u), 0x5a};
            done = true;
            send(&u, block, 3);
            assert(response[0] == 0x76 && saved.offset == i && saved.data[0] == 0x5a);
        }
        uint8_t exit_request[] = {0x37};
        send(&u, exit_request, 1);
        assert(response[0] == 0x77 && saved.kind == HMI_UPDATE_JOB_VERIFY);
        uint8_t reset[] = {0x11, 1};
        send(&u, reset, 2);
        assert(response[0] == 0x7f && response[2] == 0x22);
        reset_allowed = true;
        send(&u, reset, 2);
        assert(response[0] == 0x51);
    }

    /* 15. 身份字符串超长必须拒绝启动，不能截断。 */
    setup();
    strcpy(snap.reason, "0123456789012345678901234567890123");
    {
        hmi_uds_port_t p = {0, submit, result, snapshot_port, can_reset, noop, cancel, maintenance};
        assert(!hmi_uds_init(&u, &tp, &p));
    }
    return 0;
}
