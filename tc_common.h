#pragma once

#if BUILD_TESTS
#include <stdio.h>
// Could use operator overloading or a constructor so INVOKE_TEST isn't needed.
// Could push to head of linked list with string name, then begin of "main" could sort/filter.
#define INVOKE_TEST(F) static const char s_##F = []() { puts("Running test: " #F "."); F(); return '\0'; }()
#endif
