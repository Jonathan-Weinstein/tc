#pragma once
#include "common.h"

#define isalpha_simple(c) ( ((c) | 32u) - 'a' < 26u )

inline int tolower_simple(int c)
{
    return c - 'A' < 26u ? c + (int('a') - 'A') : c;
}

inline int stricmp_ascii(const char* s0, const char* s1)
{
    for (size_t i = 0;; ++i) {
        int c0 = tolower_simple(s0[i]);
        int c1 = tolower_simple(s1[i]);
        int diff = c0 - c1;
        if (diff)
            return diff;
        else if (c1 == 0)
            return 0;
    }
}

inline int memicmp_ascii(const char* s0, const char* s1, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        int c0 = tolower_simple(s0[i]);
        int c1 = tolower_simple(s1[i]);
        int diff = c0 - c1;
        if (diff)
            return diff;
    }
    return 0;
}

/**
 * "strcpy2"
 *
 * Compared to strncpy/strncat:
 *  - More efficient (strncpy fills the whole rest of the dst's capacity with zeros and throws away the length),
 *    but note there's no SIMD-implementation of this currently for large strings.
 *  - _Always_ NUL-terminates dst; [dst:max_dst_sentinel_pos] inclusive _must_ be writable.
 *    _NOTE_: the copy-limiting argument uses the same convention as strlen; it does _NOT_ count the terminator.
 *  - Return value is different.
 *
 * This is mainly meant to be used internally by {String/StringBuilder/Stream}-like things/classes.
**/
struct strcpy2_t {
    char* dst_sentinel; // Where sentinel was written; exactly a single sentinel is written.
    char truncated;     // == 0: the entire source plus a sentinel was fit in dst.
                        // != 0: the first dropped byte's value from src.
};

inline strcpy2_t
strcpy2(char* dst, char* max_dst_sentinel_pos, char const* src)
{
    ASSERT(max_dst_sentinel_pos >= dst);
    char c;
    while ((c = *src++) != 0 && dst < max_dst_sentinel_pos)
        *dst++ = c;
    *dst = 0;
    return { dst, c };
}
