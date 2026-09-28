#include "storage/meter_record.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                                                     \
    do                                                                                                       \
    {                                                                                                        \
        if (!(condition))                                                                                    \
        {                                                                                                    \
            fprintf(stderr, "meter-record:%d: %s\n", __LINE__, #condition);                                  \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)

int main(void)
{
    const uint8_t payload[] = {0x00u, 0x11u, 0xFEu, 0xFFu};
    const meter_record_view_t input = {1u, 0x424Du, 3u, 0u, 9u, payload, sizeof(payload)};
    uint8_t encoded[64u];
    size_t written = 0u;
    CHECK(meter_record_encode(&input, encoded, sizeof(encoded), &written) == METER_RECORD_RESULT_OK);
    CHECK(written == METER_RECORD_HEADER_SIZE + sizeof(payload) + METER_RECORD_TRAILER_SIZE);
    const uint8_t golden[] = {0x46u, 0x4Du, 0x50u, 0x32u, 0x02u, 0x00u, 0x01u, 0x00u, 0x4Du, 0x42u, 0x03u,
                              0x00u, 0x00u, 0x00u, 0x09u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                              0x04u, 0x00u, 0x00u, 0x00u, 0x3Eu, 0x7Au, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                              0x11u, 0xFEu, 0xFFu, 0x09u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                              0x04u, 0x00u, 0x00u, 0x00u, 0x3Eu, 0x7Au, 0xC1u, 0x85u};
    CHECK(written == sizeof(golden) && !memcmp(encoded, golden, sizeof(golden)));
    meter_record_view_t decoded;
    CHECK(meter_record_decode(encoded, written, 0x424Du, 3u, &decoded) == METER_RECORD_RESULT_OK);
    CHECK(decoded.type == 1u && decoded.sequence == 9u && decoded.payload_size == sizeof(payload));
    CHECK(memcmp(decoded.payload, payload, sizeof(payload)) == 0);
    encoded[written - 1u] ^= 1u;
    CHECK(meter_record_decode(encoded, written, 0x424Du, 3u, &decoded) == METER_RECORD_RESULT_CORRUPT);
    encoded[written - 1u] ^= 1u;
    encoded[8u] ^= 1u;
    CHECK(meter_record_decode(encoded, written, 0x424Du, 3u, &decoded) == METER_RECORD_RESULT_CORRUPT);
    CHECK(meter_record_encode(&input, encoded, 4u, &written) == METER_RECORD_RESULT_CAPACITY);
    CHECK(meter_record_encode(&input, encoded, sizeof(encoded), &written) == METER_RECORD_RESULT_OK);
    CHECK(meter_record_decode(encoded, written, 0x424Du, 99u, &decoded) == METER_RECORD_RESULT_INCOMPATIBLE);
    for (size_t i = 0u; i < written; ++i)
    {
        encoded[i] ^= 1u;
        CHECK(meter_record_decode(encoded, written, 0x424Du, 3u, &decoded) != METER_RECORD_RESULT_OK);
        encoded[i] ^= 1u;
    }
    puts("meter-record: PASS");
    return 0;
}
