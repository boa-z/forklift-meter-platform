#include "protocols/uds/hmi_uds.h"
#include "util.h"
#include <string.h>
/* 线协议编号必须与 C 枚举解耦：表用指定初始化器，重排枚举不会改变线上字节。 */
static const uint8_t state_code[] = {
    [HMI_UPDATE_IDLE] = 0u,       [HMI_UPDATE_DOWNLOADING] = 1u, [HMI_UPDATE_TRANSFERRED] = 2u,
    [HMI_UPDATE_VERIFYING] = 3u,  [HMI_UPDATE_CANDIDATE] = 4u,   [HMI_UPDATE_WAIT_DURABLE] = 5u,
    [HMI_UPDATE_ACTIVATING] = 6u, [HMI_UPDATE_ACTIVATED] = 7u,   [HMI_UPDATE_CONFIRMED] = 8u,
    [HMI_UPDATE_ABORTED] = 9u,    [HMI_UPDATE_FAILED] = 10u,
};
static const uint8_t error_code[] = {
    [HMI_UPDATE_OK] = 0u,           [HMI_UPDATE_STATE] = 1u,    [HMI_UPDATE_COMPATIBILITY] = 2u,
    [HMI_UPDATE_DENIED] = 3u,       [HMI_UPDATE_LENGTH] = 4u,   [HMI_UPDATE_ORDER] = 5u,
    [HMI_UPDATE_BACKEND] = 6u,      [HMI_UPDATE_HASH] = 7u,     [HMI_UPDATE_TIMEOUT] = 8u,
    [HMI_UPDATE_NVM] = 9u,          [HMI_UPDATE_SESSION] = 10u, [HMI_UPDATE_FORMAT] = 11u,
    [HMI_UPDATE_UNSUPPORTED] = 12u,
};
_Static_assert(sizeof(state_code) == HMI_UPDATE_FAILED + 1u, "state wire table must cover every state");
_Static_assert(sizeof(error_code) == HMI_UPDATE_UNSUPPORTED + 1u,
               "error wire table must cover every error");
static UDSErr_t map_error(hmi_update_error_t e)
{
    switch (e)
    {
    case HMI_UPDATE_OK:
        return UDS_PositiveResponse;
    case HMI_UPDATE_COMPATIBILITY:
    case HMI_UPDATE_FORMAT:
    case HMI_UPDATE_LENGTH:
        return UDS_NRC_RequestOutOfRange;
    case HMI_UPDATE_DENIED:
    case HMI_UPDATE_NVM:
    case HMI_UPDATE_UNSUPPORTED:
        return UDS_NRC_ConditionsNotCorrect;
    case HMI_UPDATE_ORDER:
    case HMI_UPDATE_SESSION:
    case HMI_UPDATE_STATE:
        return UDS_NRC_RequestSequenceError;
    default:
        return UDS_NRC_GeneralProgrammingFailure;
    }
}
static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}
/* 定长字段只允许 ASCII 且必须 NUL 终止；超长在 init 阶段已被拒绝，这里只做防御。 */
static bool put_text(uint8_t *dst, size_t capacity, const char *src)
{
    size_t n = strlen(src);
    if (n >= capacity)
        return false;
    memset(dst, 0, capacity);
    memcpy(dst, src, n);
    return true;
}
static bool valid_ident(const char *s, size_t capacity)
{
    size_t n = 0;
    while (n < capacity && s[n] != '\0')
    {
        char c = s[n];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
              c == '_' || c == '+' || c == '-'))
            return false;
        ++n;
    }
    return n != 0 && n < capacity;
}
static bool encode_manifest(uint8_t *out, const hmi_update_manifest_t *m)
{
    if (!put_text(out, 32u, m->product) || !put_text(out + 32u, 32u, m->hardware) ||
        !put_text(out + 64u, 48u, m->version))
        return false;
    put_be32(out + 112u, m->size);
    memcpy(out + 116u, m->sha256, 32u);
    return true;
}
/* 0x2E 只做语法与字段合法性；Product/Hardware 匹配、容量与准入由 0x34 决定。 */
static bool parse_manifest(const uint8_t *p, hmi_update_manifest_t *m)
{
    memset(m, 0, sizeof(*m));
    memcpy(m->product, p, 32u);
    memcpy(m->hardware, p + 32u, 32u);
    memcpy(m->version, p + 64u, 48u);
    m->size = ((uint32_t)p[112] << 24) | ((uint32_t)p[113] << 16) | ((uint32_t)p[114] << 8) | p[115];
    memcpy(m->sha256, p + 116u, 32u);
    return valid_ident(m->product, sizeof(m->product)) && valid_ident(m->hardware, sizeof(m->hardware)) &&
           valid_ident(m->version, sizeof(m->version)) && m->size != 0u;
}
static UDSErr_t snapshot(hmi_uds_t *u, hmi_uds_snapshot_t *s)
{
    return u->port.snapshot(u->port.context, s) ? UDS_PositiveResponse : UDS_NRC_ConditionsNotCorrect;
}
static UDSErr_t read_manifest(hmi_uds_t *u, UDSServer_t *srv, UDSRDBIArgs_t *a)
{
    uint8_t out[HMI_UDS_MANIFEST_SIZE];
    hmi_uds_snapshot_t s;
    UDSErr_t e = snapshot(u, &s);
    if (e != UDS_PositiveResponse)
        return e;
    /* worker 权威候选优先；staged 只覆盖"已提交、尚未 Begin"的窗口。 */
    const hmi_update_manifest_t *m = s.manifest_valid ? &s.manifest : (u->staged_valid ? &u->staged : NULL);
    if (!m)
        memset(out, 0, sizeof(out));
    else if (!encode_manifest(out, m))
        return UDS_NRC_ConditionsNotCorrect;
    return a->copy(srv, out, (uint16_t)sizeof(out));
}
static UDSErr_t read_status(hmi_uds_t *u, UDSServer_t *srv, UDSRDBIArgs_t *a)
{
    uint8_t out[HMI_UDS_STATUS_SIZE];
    hmi_uds_snapshot_t s;
    UDSErr_t e = snapshot(u, &s);
    if (e != UDS_PositiveResponse)
        return e;
    out[0] = s.state < sizeof(state_code) ? state_code[s.state] : 0xffu;
    out[1] = s.error < sizeof(error_code) ? error_code[s.error] : 0xffu;
    out[2] = (uint8_t)((s.backend_supported ? HMI_UDS_FLAG_BACKEND_SUPPORTED : 0u) |
                       (s.maintenance ? HMI_UDS_FLAG_MAINTENANCE : 0u) |
                       (s.exclusive ? HMI_UDS_FLAG_EXCLUSIVE : 0u) |
                       (s.admitted ? HMI_UDS_FLAG_ADMISSION : 0u) |
                       ((s.manifest_valid || u->staged_valid) ? HMI_UDS_FLAG_MANIFEST_PRESENT : 0u) |
                       (s.candidate_verified ? HMI_UDS_FLAG_CANDIDATE_VERIFIED : 0u));
    out[3] = 0u;
    put_be32(out + 4u, s.received);
    put_be32(out + 8u, s.total);
    return a->copy(srv, out, (uint16_t)sizeof(out));
}
static UDSErr_t read_endpoint(hmi_uds_t *u, UDSServer_t *srv, UDSRDBIArgs_t *a)
{
    uint8_t out[HMI_UDS_ENDPOINT_SIZE];
    hmi_uds_snapshot_t s;
    UDSErr_t e = snapshot(u, &s);
    if (e != UDS_PositiveResponse)
        return e;
    if (!put_text(out, 32u, s.product) || !put_text(out + 32u, 32u, s.hardware) ||
        !put_text(out + 64u, 48u, s.version) ||
        !put_text(out + 112u, HMI_UDS_ENDPOINT_OS_FILE, s.os_file) ||
        !put_text(out + 132u, HMI_UDS_ENDPOINT_REASON, s.reason))
        return UDS_NRC_ConditionsNotCorrect;
    put_be32(out + 128u, s.capacity);
    return a->copy(srv, out, (uint16_t)sizeof(out));
}
static UDSErr_t read_ident(hmi_uds_t *u, UDSServer_t *srv, UDSRDBIArgs_t *a, size_t capacity,
                           const char *src)
{
    uint8_t out[HMI_UDS_SOFTWARE_SIZE];
    (void)u;
    if (capacity > sizeof(out) || !put_text(out, capacity, src))
        return UDS_NRC_ConditionsNotCorrect;
    return a->copy(srv, out, (uint16_t)capacity);
}
static UDSErr_t dispatch(hmi_uds_t *u, const hmi_update_job_t *j)
{
    if (!u->pending)
    {
        if (!u->port.submit(u->port.context, j))
            return UDS_NRC_BusyRepeatRequest;
        u->pending = true;
        u->pending_kind = j->kind;
    }
    if (u->pending_kind != j->kind)
        return UDS_NRC_RequestSequenceError;
    hmi_update_error_t e;
    if (!u->port.result(u->port.context, &e, &u->generation, &u->offset))
        return UDS_NRC_RequestCorrectlyReceived_ResponsePending;
    u->pending = false;
    /* Begin 完成即转移权威：成功由 worker 持有候选，失败则该 Manifest 未成为候选。
     * 两种情况都必须停止回读 staged，否则 FAILED/ABORTED 会暴露上一任务的旧值。 */
    if (u->pending_kind == HMI_UPDATE_JOB_BEGIN)
        u->staged_valid = false;
    return map_error(e);
}
static UDSErr_t event(UDSServer_t *srv, UDSEvent_t ev, void *arg)
{
    hmi_uds_t *u = srv->fn_data;
    /* 活跃请求与 0x78 处理维持诊断会话，空闲时仍按 S3 超时。 */
    if (ev != UDS_EVT_SessionTimeout)
        srv->s3_session_timeout_timer = UDSMillis() + srv->s3_ms;
    hmi_update_job_t j = {0};
    j.generation = u->generation;
    j.offset = u->offset;
    switch (ev)
    {
    case UDS_EVT_DiagSessCtrl:
    {
        UDSDiagSessCtrlArgs_t *a = arg;
        if (a->type != 1u && a->type != 2u)
            return UDS_NRC_SubFunctionNotSupported;
        if (u->port.maintenance)
            u->port.maintenance(u->port.context, a->type == 2u);
        if (a->type == 1u)
        {
            u->port.cancel(u->port.context);
            u->staged_valid = false;
            u->pending = false;
            srv->xferIsActive = false;
        }
        return UDS_PositiveResponse;
    }
    case UDS_EVT_WriteDataByIdent:
    {
        UDSWDBIArgs_t *a = arg;
        /* 只写 FD10；其余 DID 只读或不存在，统一 0x31。 */
        if (a->dataId != HMI_UDS_DID_UPDATE_MANIFEST)
            return UDS_NRC_RequestOutOfRange;
        if (srv->sessionType != 2u)
            return UDS_NRC_ServiceNotSupportedInActiveSession;
        if (a->len != HMI_UDS_MANIFEST_SIZE)
            return UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
        hmi_uds_snapshot_t s;
        UDSErr_t e = snapshot(u, &s);
        if (e != UDS_PositiveResponse)
            return e;
        /* 下载中、已存在候选或请求未完成时不得覆盖候选身份。 */
        if (s.manifest_valid || srv->xferIsActive || u->pending)
            return UDS_NRC_ConditionsNotCorrect;
        hmi_update_manifest_t m;
        if (!parse_manifest(a->data, &m))
            return UDS_NRC_RequestOutOfRange;
        u->staged = m;
        u->staged_valid = true;
        return UDS_PositiveResponse;
    }
    case UDS_EVT_ReadDataByIdent:
    {
        UDSRDBIArgs_t *a = arg;
        hmi_uds_snapshot_t s;
        switch (a->dataId)
        {
        case HMI_UDS_DID_UPDATE_MANIFEST:
            return read_manifest(u, srv, a);
        case HMI_UDS_DID_UPDATE_STATUS:
            return read_status(u, srv, a);
        case HMI_UDS_DID_UPDATE_ENDPOINT:
            return read_endpoint(u, srv, a);
        case HMI_UDS_DID_ACTIVE_SESSION:
        {
            uint8_t session = (uint8_t)srv->sessionType;
            return a->copy(srv, &session, 1u);
        }
        case HMI_UDS_DID_HARDWARE_NUMBER:
            if (snapshot(u, &s) != UDS_PositiveResponse)
                return UDS_NRC_ConditionsNotCorrect;
            return read_ident(u, srv, a, HMI_UDS_HARDWARE_SIZE, s.hardware);
        case HMI_UDS_DID_SOFTWARE_VERSION:
            if (snapshot(u, &s) != UDS_PositiveResponse)
                return UDS_NRC_ConditionsNotCorrect;
            return read_ident(u, srv, a, HMI_UDS_SOFTWARE_SIZE, s.version);
        default:
            return UDS_NRC_RequestOutOfRange;
        }
    }
    case UDS_EVT_RequestDownload:
    {
        UDSRequestDownloadArgs_t *a = arg;
        if (srv->sessionType != 2u)
            return UDS_NRC_ServiceNotSupportedInActiveSession;
        if (!u->staged_valid)
            return UDS_NRC_ConditionsNotCorrect;
        if (a->addr || a->dataFormatIdentifier || a->size != u->staged.size)
            return UDS_NRC_RequestOutOfRange;
        a->maxNumberOfBlockLength = HMI_UPDATE_BLOCK_SIZE + 2u;
        j.kind = HMI_UPDATE_JOB_BEGIN;
        j.manifest = u->staged;
        return dispatch(u, &j);
    }
    case UDS_EVT_TransferData:
    {
        UDSTransferDataArgs_t *a = arg;
        if (!a->len || a->len > sizeof(j.data))
            return UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
        j.kind = HMI_UPDATE_JOB_WRITE;
        j.size = a->len;
        memcpy(j.data, a->data, a->len);
        return dispatch(u, &j);
    }
    case UDS_EVT_RequestTransferExit:
    {
        UDSRequestTransferExitArgs_t *a = arg;
        if (a->len)
            return UDS_NRC_IncorrectMessageLengthOrInvalidFormat;
        j.kind = HMI_UPDATE_JOB_VERIFY;
        return dispatch(u, &j);
    }
    case UDS_EVT_RoutineCtrl:
    {
        UDSRoutineCtrlArgs_t *a = arg;
        if (srv->sessionType != 2u)
            return UDS_NRC_ServiceNotSupportedInActiveSession;
        if (a->ctrlType != 1u || a->len)
            return UDS_NRC_RequestOutOfRange;
        if (a->id == 0xf001u)
            j.kind = HMI_UPDATE_JOB_ACTIVATE;
        else if (a->id == 0xf002u)
            j.kind = HMI_UPDATE_JOB_ABORT;
        else if (a->id == 0xf003u)
            j.kind = HMI_UPDATE_JOB_CONFIRM;
        else
            return UDS_NRC_RequestOutOfRange;
        UDSErr_t e = dispatch(u, &j);
        if (e == UDS_PositiveResponse && j.kind == HMI_UPDATE_JOB_ABORT)
        {
            srv->xferIsActive = false;
            u->staged_valid = false;
        }
        return e;
    }
    case UDS_EVT_EcuReset:
    {
        UDSECUResetArgs_t *a = arg;
        if (a->type != 1u || !u->port.can_reset(u->port.context))
            return UDS_NRC_ConditionsNotCorrect;
        a->powerDownTimeMillis = 300;
        return UDS_PositiveResponse;
    }
    case UDS_EVT_DoScheduledReset:
        u->port.reset(u->port.context);
        return UDS_PositiveResponse;
    case UDS_EVT_SessionTimeout:
    {
        /* 只丢弃诊断会话里的临时 Manifest；已验证候选属于 worker，必须继续可读。 */
        u->staged_valid = false;
        u->pending = false;
        if (u->port.maintenance)
            u->port.maintenance(u->port.context, false);
        u->port.cancel(u->port.context);
        /* 使用上游初始化器一并清除旧的响应挂起、请求和传输上下文。 */
        UDSTp_t *transport = srv->tp;
        (void)UDSServerInit(srv);
        srv->tp = transport;
        srv->fn = event;
        srv->fn_data = u;
        return UDS_PositiveResponse;
    }
    default:
        return UDS_NRC_ServiceNotSupported;
    }
}
bool hmi_uds_init(hmi_uds_t *u, UDSTp_t *tp, const hmi_uds_port_t *p)
{
    if (!u || !tp || !p || !p->submit || !p->result || !p->snapshot || !p->can_reset || !p->reset ||
        !p->cancel)
        return false;
    memset(u, 0, sizeof(*u));
    if (UDSServerInit(&u->server) != UDS_OK)
        return false;
    u->port = *p;
    u->server.fn = event;
    u->server.fn_data = u;
    u->server.tp = tp;
    /* 身份字符串超长必须拒绝启动，不能截断成错误的线协议身份。 */
    hmi_uds_snapshot_t s;
    if (!p->snapshot(p->context, &s))
        return false;
    if (strlen(s.product) >= sizeof(s.product) || strlen(s.hardware) >= sizeof(s.hardware) ||
        strlen(s.version) >= sizeof(s.version) || strlen(s.os_file) >= sizeof(s.os_file) ||
        strlen(s.reason) >= sizeof(s.reason))
        return false;
    return true;
}
