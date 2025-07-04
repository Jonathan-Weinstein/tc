#include <stdio.h>

#include "utility/common.h"

void DoSomething();

int main()
{
#if BUILD_TESTS
    puts("BUILD_TESTS: done.");
#endif

    puts("Bye!");
    return 0;
}
