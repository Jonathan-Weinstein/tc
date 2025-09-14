#include <stdio.h>

#include "utility/common.h"
#include "utility/ByteStream.h"
#include "utility/mix.h"

// #define TEST_MX3 1
#if TEST_MX3
#include "../../untracked/mx3.h"
#endif

void DoSomething();

int main()
{
#if BUILD_TESTS
    puts("BUILD_TESTS: done.");
#endif

    // DoSomething();

#if TEST_MX3
    {
        puts("mx3 version 3");
        uint8_t a[81] = {};
        for (unsigned len = 0; len < countof(a); ++len) {
            const uint64_t ref = mx3::hash(a, len, 0);
            const uint64_t got = HashBytes64(a, len);
            ASSERT(ref == got);
            a[len] = len + 17;
        }
    }
#endif

    puts("Bye!");
    return 0;
}
