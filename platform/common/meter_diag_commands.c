#include "platform/common/meter_diag_commands.h"
#include "storage/meter_nvm.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define U(x) ((unsigned long)(x))
static void line(meter_diag_line_fn emit, void *ctx, const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    emit(ctx, buffer);
}
bool meter_diag_query(int argc, const char *const *argv, meter_diag_query_t *q)
{
    if (!q || !argv || argc < 2 || argc > 3 || !argv[0] || strcmp(argv[0], "meter") || !argv[1])
        return false;
    static const char *const names[] = {"info",   "diag",   "runtime", "can",   "pdo",    "sdo",
                                        "domain", "signal", "touch",   "trace", "storage"};
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        if (!strcmp(argv[1], names[i]))
        {
            *q = (meter_diag_query_t){.kind = (meter_diag_query_kind_t)i, .bus = -1};
            if (argc == 2)
                return q->kind != METER_QUERY_SIGNAL;
            if (!argv[2])
                return false;
            if (q->kind == METER_QUERY_SIGNAL)
            {
                q->key = argv[2];
                return *q->key != 0;
            }
            if (q->kind == METER_QUERY_CAN && (!strcmp(argv[2], "0") || !strcmp(argv[2], "1")))
            {
                q->bus = argv[2][0] - '0';
                return true;
            }
            if (q->kind == METER_QUERY_TRACE)
            {
                if (!strcmp(argv[2], "dump"))
                {
                    q->kind = METER_QUERY_TRACE_DUMP;
                    return true;
                }
                if (!strcmp(argv[2], "clear"))
                {
                    q->kind = METER_QUERY_TRACE_CLEAR;
                    return true;
                }
            }
            return false;
        }
    return false;
}
const char *meter_trace_module_name(uint16_t module)
{
    static const char *const names[] = {"UNKNOWN", "RUNTIME", "CAN", "PDO", "SDO", "DOMAIN", "PRODUCT"};
    return module < sizeof(names) / sizeof(names[0]) ? names[module] : "UNKNOWN";
}
const char *meter_trace_event_name(uint16_t m, uint16_t e)
{
    static const char *const runtime[] = {"CONNECT", "DISCONNECT", "QUEUE_OVERFLOW", "RESET",
                                          "INVALID_FRAME"};
    static const char *const can[] = {"RX_DROP", "TX_BUSY", "ERROR"};
    static const char *const pdo[] = {"RX", "STALE", "RECOVER", "DECODE_ERROR"};
    static const char *const sdo[] = {"QUEUE",   "START", "COMPLETE", "ABORT",
                                      "TIMEOUT", "RETRY", "RESET",    "FAILED"};
    static const char *const domain[] = {"STALE", "RECOVER", "SOURCE_SWITCH", "ERROR"};
    static const char *const product[] = {"READY", "FAILED"};
    const char *const *names = NULL;
    size_t n = 0;
#define NAMES(a)                                                                                             \
    names = a;                                                                                               \
    n = sizeof(a) / sizeof(a[0]);                                                                            \
    break
    switch (m)
    {
    case METER_TRACE_RUNTIME:
        NAMES(runtime);
    case METER_TRACE_CAN:
        NAMES(can);
    case METER_TRACE_PDO:
        NAMES(pdo);
    case METER_TRACE_SDO:
        NAMES(sdo);
    case METER_TRACE_DOMAIN:
        NAMES(domain);
    case METER_TRACE_PRODUCT:
        NAMES(product);
    default:
        break;
    }
#undef NAMES
    return e && e <= n ? names[e - 1] : "UNKNOWN";
}
static void runtime(const meter_diag_snapshot_t *s, meter_diag_line_fn emit, void *ctx)
{
    const meter_diag_runtime_t *d = &s->runtime;
    if (!d->available)
    {
        emit(ctx, "runtime unavailable");
        return;
    }
    line(emit, ctx, "runtime connected=%u generation=%lu depth=%lu capacity=%lu adapters=%lu", d->connected,
         U(d->generation), U(d->depth), U(d->capacity), U(d->adapters));
    line(emit, ctx,
         "runtime accepted=%lu dispatched=%lu malformed=%lu unrouted=%lu decode_failed=%lu overflow=%lu "
         "resets=%lu",
         U(d->counters.accepted), U(d->counters.dispatched), U(d->counters.malformed),
         U(d->counters.unrouted), U(d->counters.decode_failed), U(d->counters.overflow),
         U(d->counters.resets));
}
static void can(const meter_diag_snapshot_t *s, unsigned bus, meter_diag_line_fn emit, void *ctx)
{
    const meter_diag_can_t *d = &s->can[bus];
    if (!d->available)
    {
        line(emit, ctx, "can%u unavailable", bus);
        return;
    }
    if (d->board_available)
        line(emit, ctx, "can%u open=%u bitrate=%lu", bus, d->open, U(d->bitrate));
    else
        line(emit, ctx, "can%u open=unavailable bitrate=unavailable", bus);
    line(emit, ctx, "can%u rx=%lu tx=%lu drop=%lu tx_busy=%lu rx_error=%lu tx_error=%lu", bus, U(d->rx),
         U(d->tx), U(d->rx_drop), U(d->tx_busy), U(d->rx_error), U(d->tx_error));
    if (d->rx_seen)
        line(emit, ctx, "can%u last_rx_age_ms=%lu", bus, U(s->uptime_ms - d->last_rx_ms));
    else
        line(emit, ctx, "can%u last_rx_age_ms=unavailable", bus);
    if (d->tx_seen)
        line(emit, ctx, "can%u last_tx_age_ms=%lu", bus, U(s->uptime_ms - d->last_tx_ms));
    else
        line(emit, ctx, "can%u last_tx_age_ms=unavailable", bus);
}
static void pdo(const meter_diag_snapshot_t *s, meter_diag_line_fn emit, void *ctx)
{
    const meter_diag_pdo_t *d = &s->pdo;
    if (!d->available)
    {
        emit(ctx, "pdo unavailable");
        return;
    }
    line(emit, ctx, "pdo bindings=%lu rx=%lu tx=%lu decode_error=%lu", U(d->bindings), U(d->rx), U(d->tx),
         U(d->decode_error));
    line(emit, ctx, "pdo state=%s timeout_ms=%lu stale=%lu recover=%lu",
         !d->seen         ? "UNKNOWN"
         : d->stale_state ? "STALE"
                          : "FRESH",
         U(d->timeout_ms), U(d->stale), U(d->recover));
    if (d->seen)
        line(emit, ctx, "pdo last_rx_age_ms=%lu", U(s->uptime_ms - d->last_rx_ms));
    else
        emit(ctx, "pdo last_rx_age_ms=unavailable");
}
static void sdo(const meter_diag_snapshot_t *s, meter_diag_line_fn emit, void *ctx)
{
    const meter_diag_sdo_t *d = &s->sdo;
    static const char *const states[] = {"IDLE", "PENDING", "SUCCESS", "ABORTED", "TIMEOUT"};
    if (!d->available)
    {
        emit(ctx, "sdo unavailable");
        return;
    }
    line(emit, ctx, "sdo bus=%u node=%u depth=%lu active_request=%lu operation=%s", d->bus, d->node,
         U(d->depth), U(d->active_request), d->operation == 0 ? "READ" : "WRITE");
    line(emit, ctx, "sdo index=0x%04x subindex=%u state=%s attempt=%u", d->index, d->subindex,
         d->state < 5 ? states[d->state] : "UNKNOWN", d->attempt);
    line(emit, ctx, "sdo queued=%lu started=%lu completed=%lu aborted=%lu timeout=%lu retry=%lu",
         U(d->queued), U(d->started), U(d->completed), U(d->aborted), U(d->timeout), U(d->retry));
    line(emit, ctx, "sdo tx_busy=%lu queue_full=%lu reset=%lu last_abort=0x%08lx", U(d->tx_busy),
         U(d->queue_full), U(d->reset), U(d->last_abort));
}
static void domain(const meter_diag_snapshot_t *s, meter_diag_line_fn emit, void *ctx)
{
    const meter_diag_domain_t *d = &s->domain;
    if (!d->available)
    {
        emit(ctx, "domain unavailable");
        return;
    }
    line(emit, ctx, "domain signals=%lu parameters=%lu faults=%lu generation=%lu revision=%lu connected=%u",
         U(d->signals), U(d->parameters), U(d->faults), U(d->generation), U(d->revision), d->connected);
    line(emit, ctx, "domain updates=%lu stale=%lu valid=%lu source_switch=%lu error=%lu", U(d->updates),
         U(d->stale_transition), U(d->valid_transition), U(d->source_switch), U(d->error_updates));
}
static void touch(const meter_diag_snapshot_t *s, meter_diag_line_fn emit, void *ctx)
{
    const meter_diag_touch_t *d = &s->touch;
    if (!d->available)
    {
        emit(ctx, "touch unavailable");
        return;
    }
    line(emit, ctx, "touch range=%ldx%ld xy=%ld,%ld state=%ld", (long)d->range_x, (long)d->range_y,
         (long)d->x, (long)d->y, (long)d->state);
    line(emit, ctx, "touch irq=%lu reads=%lu events=%lu delivered=%lu recovered=%lu empty=%lu invalid=%lu",
         U(d->irq), U(d->reads), U(d->events), U(d->delivered), U(d->recovered), U(d->empty_reads),
         U(d->invalid_reads));
}
static void storage(const meter_diag_snapshot_t *s, meter_diag_line_fn emit, void *ctx)
{
    if (s->storage.available)
    {
        line(emit, ctx, "storage reads=%lu writes=%lu errors=%lu", U(s->storage.reads), U(s->storage.writes),
             U(s->storage.errors));
        line(emit, ctx,
             "storage backend=%s state=%s dirty=%u degraded=%u depth=%lu last_error=%lu result=%lu",
             s->storage.backend ? s->storage.backend : "unknown",
             meter_nvm_state_name((meter_nvm_state_t)s->storage.state), (unsigned)s->storage.dirty,
             (unsigned)s->storage.degraded, U(s->storage.depth), U(s->storage.last_error),
             U(s->storage.last_result));
        line(emit, ctx, "storage ram_revision=%llu inflight_revision=%llu durable_revision=%llu",
             (unsigned long long)s->storage.ram_revision, (unsigned long long)s->storage.inflight_revision,
             (unsigned long long)s->storage.durable_revision);
        line(emit, ctx, "storage language=%lu brightness=%lu imperial=%u", U(s->storage.language),
             U(s->storage.brightness), (unsigned)s->storage.imperial);
        line(emit, ctx, "storage configured_can_rate=%lu apply=next-boot", U(s->storage.configured_can_rate));
    }
    else
        emit(ctx, "storage unavailable (no NVM backend)");
}
void meter_diag_render(const meter_diag_query_t *q, const meter_diag_view_t *v, meter_diag_line_fn emit,
                       void *ctx)
{
    if (!q || !v || !v->snapshot || !emit)
        return;
    const meter_diag_snapshot_t *s = v->snapshot;
    switch (q->kind)
    {
    case METER_QUERY_INFO:
    {
        const meter_build_info_t *b = v->build;
        if (!b)
        {
            emit(ctx, "build identity unavailable");
            break;
        }
#define FIELD(k, f) line(emit, ctx, k " : %s", b->f ? b->f : "unavailable")
        FIELD("product", product);
        FIELD("platform", platform_revision);
        FIELD("sdk", sdk_revision);
        FIELD("lvgl", lvgl_version);
        FIELD("lvgl-aic", lvgl_aic_revision);
        FIELD("board", board);
        FIELD("build_date", build_date);
        FIELD("build_time", build_time);
        FIELD("firmware_version", firmware_version);
#undef FIELD
        line(emit, ctx, "uptime_ms : %lu", U(s->uptime_ms));
        break;
    }
    case METER_QUERY_STORAGE:
        storage(s, emit, ctx);
        break;
    case METER_QUERY_DIAG:
        runtime(s, emit, ctx);
        for (unsigned i = 0; i < METER_BUS_COUNT; ++i)
            can(s, i, emit, ctx);
        pdo(s, emit, ctx);
        sdo(s, emit, ctx);
        domain(s, emit, ctx);
        touch(s, emit, ctx);
        if (s->ui.available)
        {
            line(emit, ctx, "ui present=%lu", U(s->ui.present_count));
            if (s->ui.flush_available)
                line(emit, ctx, "ui flush=%lu", U(s->ui.flush_count));
            else
                emit(ctx, "ui flush=unavailable");
        }
        else
            emit(ctx, "ui unavailable");
        storage(s, emit, ctx);
        break;
    case METER_QUERY_RUNTIME:
        runtime(s, emit, ctx);
        break;
    case METER_QUERY_CAN:
        for (unsigned i = 0; i < METER_BUS_COUNT; ++i)
            if (q->bus < 0 || (unsigned)q->bus == i)
                can(s, i, emit, ctx);
        break;
    case METER_QUERY_PDO:
        pdo(s, emit, ctx);
        break;
    case METER_QUERY_SDO:
        sdo(s, emit, ctx);
        break;
    case METER_QUERY_DOMAIN:
        domain(s, emit, ctx);
        break;
    case METER_QUERY_TOUCH:
        touch(s, emit, ctx);
        break;
    case METER_QUERY_SIGNAL:
    {
        const meter_diag_signal_t *d = v->signal;
        if (!d)
        {
            line(emit, ctx, "signal not found: %s", q->key);
            break;
        }
        static const char *const states[] = {"UNKNOWN", "VALID", "STALE", "ERROR"};
        line(emit, ctx, "signal id=%u key=%s value=%.9g unit=%s", d->id, d->key, (double)d->value.value,
             d->unit ? d->unit : "");
        line(emit, ctx, "state=%s source=%u timestamp_ms=%lu age_ms=%lu stale_ms=%lu",
             d->value.state < 4 ? states[d->value.state] : "UNKNOWN", d->value.source,
             U(d->value.timestamp_ms), U(d->age_ms), U(d->stale_ms));
        break;
    }
    case METER_QUERY_TRACE:
        line(emit, ctx, "trace capacity=%lu count=%lu write_index=%lu overwritten=%lu", U(s->trace_capacity),
             U(s->trace_count), U(s->trace_write_index), U(s->trace_overwritten));
        break;
    case METER_QUERY_TRACE_CLEAR:
        line(emit, ctx, "trace clear target=history cleared=%lu result=OK", U(v->cleared));
        break;
    case METER_QUERY_TRACE_DUMP:
        line(emit, ctx, "trace entries=%lu overwritten=%lu", U(v->entry_count), U(s->trace_overwritten));
        for (size_t i = 0; v->entries && i < v->entry_count; ++i)
        {
            const meter_trace_entry_t *e = &v->entries[i];
            line(emit, ctx, "%lu %s %s module=%u event=%u arg0=%lu arg1=0x%08lx", U(e->timestamp_ms),
                 meter_trace_module_name(e->module), meter_trace_event_name(e->module, e->event), e->module,
                 e->event, U(e->arg0), U(e->arg1));
        }
        break;
    }
}
