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
        const char a[] = "qwertyuiopasdfghjklzxcvbnm.0123456789_qwertyuiopasdfghjklzxcvbnm.0123456789_abcde";
        static_assert(countof(a) - 1 == 81, "");
        for (int len = 0; len <= 81; ++len) {
            const uint64_t ref = mx3::hash(reinterpret_cast<const uint8_t*>(a), len, 0);
            const uint64_t got = HashBytes64(a, len);
            ASSERT(ref == got);
        }
    }
#endif

    puts("Bye!");
    return 0;
}
