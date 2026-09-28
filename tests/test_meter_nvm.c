#include "storage/meter_nvm.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(x))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "nvm:%d: %s\n", __LINE__, #x);                                                   \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
int main(void)
{
    uint8_t pending[32], inflight[32], a[] = {1, 2}, b[] = {3, 4};
    meter_nvm_service_t s;
    meter_nvm_job_t j;
    CHECK(meter_nvm_init(&s, pending, inflight, sizeof(pending), 1, 2, 1, 100, 500));
    CHECK(!meter_nvm_loaded(&s, 2, METER_SLOTS_EMPTY, NULL));
    CHECK(meter_nvm_loaded(&s, 1, METER_SLOTS_EMPTY, NULL));
    CHECK(meter_nvm_observe(&s, a, sizeof(a), 10));
    CHECK(meter_nvm_observe(&s, a, sizeof(a), 20) && s.ram_revision == 1);
    CHECK(!meter_nvm_take(&s, 109, &j));
    CHECK(meter_nvm_take(&s, 110, &j) && j.revision == 1);
    CHECK(meter_nvm_observe(&s, b, sizeof(b), 111) && s.ram_revision == 2);
    CHECK(!memcmp(j.record.payload, a, 2));
    CHECK(!meter_nvm_complete(&s, 2, 1, METER_SLOTS_OK));
    CHECK(!meter_nvm_complete(&s, 1, 2, METER_SLOTS_OK));
    CHECK(meter_nvm_complete(&s, 1, 1, METER_SLOTS_OK));
    CHECK(s.dirty && s.durable_revision == 1 && meter_nvm_barrier(&s, 1));
    CHECK(!meter_nvm_barrier(&s, 2));
    meter_nvm_request_save(&s);
    CHECK(meter_nvm_take(&s, 112, &j) && j.revision == 2);
    CHECK(!meter_nvm_cancel(&s));
    CHECK(meter_nvm_complete(&s, 1, 2, METER_SLOTS_WRITE_INTERRUPTED));
    CHECK(s.dirty && s.state == METER_NVM_UNCERTAIN && !meter_nvm_barrier(&s, 1));
    CHECK(!meter_nvm_take(&s, 900, &j));
    meter_record_view_t r = {1, 2, 1, 0, 2, b, sizeof(b)};
    CHECK(meter_nvm_reconcile(&s, METER_SLOTS_OK, &r));
    CHECK(!s.dirty && meter_nvm_barrier(&s, 2));
    CHECK(meter_nvm_observe(&s, a, 2, 1000));
    CHECK(meter_nvm_cancel(&s) && !meter_nvm_take(&s, 2000, &j));
    meter_nvm_request_save(&s);
    CHECK(meter_nvm_take(&s, 2001, &j));
    CHECK(meter_nvm_complete(&s, 1, j.revision, METER_SLOTS_OK));
    /* 持续修改不能无限延长最大未保存时间，且毫秒回绕正确。 */
    CHECK(meter_nvm_observe(&s, b, 2, UINT32_MAX - 10u));
    CHECK(meter_nvm_observe(&s, a, 2, 80));
    CHECK(meter_nvm_observe(&s, b, 2, 170));
    CHECK(meter_nvm_observe(&s, a, 2, 260));
    CHECK(meter_nvm_observe(&s, b, 2, 350));
    CHECK(meter_nvm_observe(&s, a, 2, 440));
    CHECK(meter_nvm_take(&s, 490, &j));
    CHECK(!meter_nvm_loaded(&s, 1, METER_SLOTS_EMPTY, NULL));
    CHECK(meter_nvm_complete(&s, 1, j.revision, METER_SLOTS_OK));
    s.ram_revision = UINT64_MAX;
    CHECK(!meter_nvm_observe(&s, b, 2, 600));
    /* 初始化同样会写介质；中断必须保持不确定状态，不能伪装为只读扫描损坏。 */
    CHECK(meter_nvm_init(&s, pending, inflight, sizeof(pending), 1, 2, 1, 100, 500));
    CHECK(meter_nvm_loaded(&s, 1, METER_SLOTS_NO_VALID_SLOT, NULL));
    CHECK(meter_nvm_observe(&s, a, sizeof(a), 0));
    CHECK(!meter_nvm_reconcile(&s, METER_SLOTS_WRITE_INTERRUPTED, NULL));
    CHECK(s.state == METER_NVM_UNCERTAIN && s.dirty && !s.writable);
    CHECK(!meter_nvm_take(&s, 1000, &j) && !meter_nvm_barrier(&s, 1));
    CHECK(meter_nvm_reconcile(&s, METER_SLOTS_EMPTY, NULL));
    CHECK(meter_nvm_take(&s, 1001, &j));
    CHECK(meter_nvm_complete(&s, 1, j.revision, METER_SLOTS_OK));
    CHECK(s.state == METER_NVM_DURABLE && meter_nvm_barrier(&s, j.revision));
    /* 启动加载有效记录处于 READY，已持久化版本可通过门禁且相同设置不重写。 */
    CHECK(meter_nvm_init(&s, pending, inflight, sizeof(pending), 1, 2, 1, 100, 500));
    r.sequence = 4;
    CHECK(meter_nvm_loaded(&s, 1, METER_SLOTS_OK, &r));
    CHECK(s.state == METER_NVM_READY && !s.dirty && !s.busy);
    CHECK(s.ram_revision == 4 && s.durable_revision == 4);
    CHECK(meter_nvm_barrier(&s, 4) && !meter_nvm_barrier(&s, 5));
    CHECK(s.pending_size == sizeof(b) && !memcmp(s.pending, b, sizeof(b)));
    CHECK(meter_nvm_observe(&s, b, sizeof(b), 10));
    meter_nvm_request_save(&s);
    CHECK(!meter_nvm_take(&s, 1000, &j));
    CHECK(s.state == METER_NVM_READY && !s.dirty && s.ram_revision == 4);
    CHECK(s.durable_revision == 4);
    puts("nvm: PASS");
    return 0;
}
