#pragma once
#include "common.h"

#define isalpha_simple(c) ( ((c) | 32u) - 'a' < 26u )

// The "simple" means
inline int tolower_simple(int c)
{
    return c - 'A' < 26u ? c + (int('a') - 'A') : c;
}

// The "_lower" is only relevant when the first mismatch byte pair is not a pair of ascii letters.
// The returned value will use the lowercase value of the letter for the lexicographical ordering.
// For example, '_' (95) will be considered less than 'A' (65), since that becomes 'a' (97).
inline int stricmp_ascii_lower(const char* s0, const char* s1)
{
    for (size_t i = 0;; ++i) {
        int c0 = tolower_simple(ubyte(s0[i]));
        int c1 = tolower_simple(ubyte(s1[i]));
        int diff = c0 - c1;
        if (diff)
            return diff;
        else if (c1 == 0)
            return 0;
    }
}

inline int memicmp_ascii_lower(const char* s0, const char* s1, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        int c0 = tolower_simple(ubyte(s0[i]));
        int c1 = tolower_simple(ubyte(s1[i]));
        int diff = c0 - c1;
        if (diff)
            return diff;
    }
    return 0;
}

#define max_strlen(buf) (countof(buf) - 1)
#define max_strlen_ptr(buf) (endof(buf) - 1)

/**
 * Unlike strncpy, this function _always_ writes exactly one terminator;
 * [dst:max_dst_sentinel_pos] inclusive _must_ be writable.
 *
 * This function's size-limiting argument is a different convention from strncpy;
 * this function's argument does _not_ include space for a terminator (which is consistent with strlen),
 * but strncpy does count it. This function also uses a pointer instead of a size to make appending
 * multiple calls easier.
 *
 * Other differences from strncpy/strncat:
 *  - More efficient (strncpy fills the whole rest of the dst's capacity with zeros and throws away the length),
 *    but note there is no unrolled/vectorized version of this currently for large strings.
 *  - Return value is different.
 *
 * In most cases, it may be preferred to use ByteStream/String instead of this.
**/
struct strcpy_result {
    char* dst_sentinel; // Where sentinel was written; exactly a single sentinel is written.
    char truncated;     // == 0: the entire source plus a sentinel was fit in dst.
                        // != 0: the first dropped byte's value from src.
};

inline strcpy_result
strcpy_max_strlen(char* dst, char* dst_plus_max_strlen, char const* src)
{
    ASSERT(dst_plus_max_strlen >= dst);
    char c;
    while ((c = *src++) != 0 && dst < dst_plus_max_strlen)
        *dst++ = c;
    *dst = 0;
    return { dst, c };
}
