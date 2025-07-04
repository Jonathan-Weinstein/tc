#include <stdio.h>

#include "utility/common.h"
#include "utility/ByteStream.h"

void DoSomething();

int main()
{
#if BUILD_TESTS
    puts("BUILD_TESTS: done.");
#endif

    DoSomething();

    puts("Bye!");
    return 0;
}
