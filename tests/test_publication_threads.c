#include "runtime/meter_periodic.h"
#include <pthread.h>
#include <assert.h>
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static meter_tx_value_t storage[2];
static meter_tx_publication_t shared = {.values = storage};
static void *publisher(void *arg)
{
    (void)arg;
    for (uint32_t i = 1u; i <= 50000u; ++i)
    {
        meter_tx_value_t v[2] = {{(int32_t)i, i, true}, {-(int32_t)i, i, true}};
        assert(pthread_mutex_lock(&mutex) == 0);
        assert(meter_tx_publish(&shared, 2u, v, 2u, 1u, i));
        assert(pthread_mutex_unlock(&mutex) == 0);
    }
    return NULL;
}
static void *reader(void *arg)
{
    (void)arg;
    meter_tx_value_t values[2]; meter_tx_publication_t copy = {.values = values};
    for (unsigned i = 0u; i < 50000u; ++i)
    {
        assert(pthread_mutex_lock(&mutex) == 0);
        assert(meter_tx_copy(&copy, 2u, &shared));
        assert(pthread_mutex_unlock(&mutex) == 0);
        if (copy.count)
        {
            assert(copy.values[0].value == -copy.values[1].value);
            assert(copy.values[0].sample_ms == copy.values[1].sample_ms);
            assert(copy.revision == copy.published_ms && copy.published_ms == copy.values[0].sample_ms);
        }
    }
    return NULL;
}
int main(void)
{
    pthread_t writer, consumer;
    assert(pthread_create(&writer, NULL, publisher, NULL) == 0);
    assert(pthread_create(&consumer, NULL, reader, NULL) == 0);
    assert(pthread_join(writer, NULL) == 0 && pthread_join(consumer, NULL) == 0);
    return 0;
}
