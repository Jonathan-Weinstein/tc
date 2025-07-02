#include <stdio.h>

int main()
{
#if BUILD_TESTS
	puts("BUILD_TESTS: done.");
#endif

	puts("Hello World!");
	return 0;
}
