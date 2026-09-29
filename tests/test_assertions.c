#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef NDEBUG
#error C test targets must evaluate assertions in every build configuration
#endif

/* Exit from inside assert evaluation: no platform abort dialog, no reliance on signals. */
static int deliberate_failure(void)
{
    puts("assert-expression-evaluated");
    exit(23);
}

int main(void)
{
    assert(deliberate_failure());
    return 0;
}
