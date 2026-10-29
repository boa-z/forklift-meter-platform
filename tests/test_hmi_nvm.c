#include "storage/hmi_nvm.h"
#include <stdio.h>
#include <string.h>
#include "check.h"
int main(void)
{
    uint8_t pending[32], inflight[32], a[] = {1, 2}, b[] = {3, 4};
    hmi_nvm_service_t s;
    hmi_nvm_job_t j;
    CHECK(hmi_nvm_init(&s, pending, inflight, sizeof(pending), 1, 2, 1, 100, 500));
    CHECK(!hmi_nvm_loaded(&s, 2, HMI_SLOTS_EMPTY, NULL));
    CHECK(hmi_nvm_loaded(&s, 1, HMI_SLOTS_EMPTY, NULL));
    CHECK(hmi_nvm_observe(&s, a, sizeof(a), 10));
    CHECK(hmi_nvm_observe(&s, a, sizeof(a), 20) && s.ram_revision == 1);
    CHECK(!hmi_nvm_take(&s, 109, &j));
    CHECK(hmi_nvm_take(&s, 110, &j) && j.revision == 1);
    CHECK(hmi_nvm_observe(&s, b, sizeof(b), 111) && s.ram_revision == 2);
    CHECK(!memcmp(j.record.payload, a, 2));
    CHECK(!hmi_nvm_complete(&s, 2, 1, HMI_SLOTS_OK));
    CHECK(!hmi_nvm_complete(&s, 1, 2, HMI_SLOTS_OK));
    CHECK(hmi_nvm_complete(&s, 1, 1, HMI_SLOTS_OK));
    CHECK(s.dirty && s.durable_revision == 1 && hmi_nvm_barrier(&s, 1));
    CHECK(!hmi_nvm_barrier(&s, 2));
    hmi_nvm_request_save(&s);
    CHECK(hmi_nvm_take(&s, 112, &j) && j.revision == 2);
    CHECK(!hmi_nvm_cancel(&s));
    CHECK(hmi_nvm_complete(&s, 1, 2, HMI_SLOTS_WRITE_INTERRUPTED));
    CHECK(s.dirty && s.state == HMI_NVM_UNCERTAIN && !hmi_nvm_barrier(&s, 1));
    CHECK(!hmi_nvm_take(&s, 900, &j));
    hmi_record_view_t r = {1, 2, 1, 0, 2, b, sizeof(b)};
    CHECK(hmi_nvm_reconcile(&s, HMI_SLOTS_OK, &r));
    CHECK(!s.dirty && hmi_nvm_barrier(&s, 2));
    CHECK(hmi_nvm_observe(&s, a, 2, 1000));
    CHECK(hmi_nvm_cancel(&s) && !hmi_nvm_take(&s, 2000, &j));
    hmi_nvm_request_save(&s);
    CHECK(hmi_nvm_take(&s, 2001, &j));
    CHECK(hmi_nvm_complete(&s, 1, j.revision, HMI_SLOTS_OK));
    /* 持续修改不能无限延长最大未保存时间，且毫秒回绕正确。 */
    CHECK(hmi_nvm_observe(&s, b, 2, UINT32_MAX - 10u));
    CHECK(hmi_nvm_observe(&s, a, 2, 80));
    CHECK(hmi_nvm_observe(&s, b, 2, 170));
    CHECK(hmi_nvm_observe(&s, a, 2, 260));
    CHECK(hmi_nvm_observe(&s, b, 2, 350));
    CHECK(hmi_nvm_observe(&s, a, 2, 440));
    CHECK(hmi_nvm_take(&s, 490, &j));
    CHECK(!hmi_nvm_loaded(&s, 1, HMI_SLOTS_EMPTY, NULL));
    CHECK(hmi_nvm_complete(&s, 1, j.revision, HMI_SLOTS_OK));
    s.ram_revision = UINT64_MAX;
    CHECK(!hmi_nvm_observe(&s, b, 2, 600));
    /* 初始化同样会写介质；中断必须保持不确定状态，不能伪装为只读扫描损坏。 */
    CHECK(hmi_nvm_init(&s, pending, inflight, sizeof(pending), 1, 2, 1, 100, 500));
    CHECK(hmi_nvm_loaded(&s, 1, HMI_SLOTS_NO_VALID_SLOT, NULL));
    CHECK(hmi_nvm_observe(&s, a, sizeof(a), 0));
    CHECK(!hmi_nvm_reconcile(&s, HMI_SLOTS_WRITE_INTERRUPTED, NULL));
    CHECK(s.state == HMI_NVM_UNCERTAIN && s.dirty && !s.writable);
    CHECK(!hmi_nvm_take(&s, 1000, &j) && !hmi_nvm_barrier(&s, 1));
    CHECK(hmi_nvm_reconcile(&s, HMI_SLOTS_EMPTY, NULL));
    CHECK(hmi_nvm_take(&s, 1001, &j));
    CHECK(hmi_nvm_complete(&s, 1, j.revision, HMI_SLOTS_OK));
    CHECK(s.state == HMI_NVM_DURABLE && hmi_nvm_barrier(&s, j.revision));
    /* 启动加载有效记录处于 READY，已持久化版本可通过门禁且相同设置不重写。 */
    CHECK(hmi_nvm_init(&s, pending, inflight, sizeof(pending), 1, 2, 1, 100, 500));
    r.sequence = 4;
    CHECK(hmi_nvm_loaded(&s, 1, HMI_SLOTS_OK, &r));
    CHECK(s.state == HMI_NVM_READY && !s.dirty && !s.busy);
    CHECK(s.ram_revision == 4 && s.durable_revision == 4);
    CHECK(hmi_nvm_barrier(&s, 4) && !hmi_nvm_barrier(&s, 5));
    CHECK(s.pending_size == sizeof(b) && !memcmp(s.pending, b, sizeof(b)));
    CHECK(hmi_nvm_observe(&s, b, sizeof(b), 10));
    hmi_nvm_request_save(&s);
    CHECK(!hmi_nvm_take(&s, 1000, &j));
    CHECK(s.state == HMI_NVM_READY && !s.dirty && s.ram_revision == 4);
    CHECK(s.durable_revision == 4);
    /* 在途写入期间多次修改只保留最新副本；完成旧版本不能误报最新版本 durable。 */
    CHECK(hmi_nvm_init(&s, pending, inflight, sizeof(pending), 1, 2, 1, 100, 500));
    CHECK(hmi_nvm_loaded(&s, 1, HMI_SLOTS_EMPTY, NULL));
    CHECK(hmi_nvm_observe(&s, a, sizeof(a), 0));
    CHECK(hmi_nvm_take(&s, 100, &j) && j.revision == 1);
    for (uint32_t i = 1u; i <= 3u; ++i)
    {
        const uint8_t latest[] = {(uint8_t)(i * 16u + 15u), 0u};
        CHECK(hmi_nvm_observe(&s, latest, sizeof(latest), 100u + i * 40u));
        CHECK(!hmi_nvm_take(&s, 1000u, &j));
        CHECK(!memcmp(j.record.payload, a, sizeof(a)));
    }
    CHECK(s.ram_revision == 4u);
    CHECK(hmi_nvm_complete(&s, 1u, 1u, HMI_SLOTS_OK));
    CHECK(!hmi_nvm_barrier(&s, 4u));
    CHECK(hmi_nvm_take(&s, 1100u, &j) && j.revision == 4u);
    CHECK(j.record.payload[0] == 63u);
    CHECK(!hmi_nvm_complete(&s, 1u, 1u, HMI_SLOTS_OK));
    CHECK(hmi_nvm_complete(&s, 1u, 4u, HMI_SLOTS_OK));
    CHECK(!s.dirty && hmi_nvm_barrier(&s, 4u));
    puts("nvm: PASS");
    return 0;
}
