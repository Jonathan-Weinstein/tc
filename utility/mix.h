#pragma once

#include <stdint.h>

/*
 * These "Avalanche" functions are invertible (and thus bijective).
 * Being invertible means 2+ inputs cannot map to the same output, which can help
 * reduce collisions in a hash table, though the full N bit output would still have
 * to be mapped to a table array index.
 *
 * These functions should ideally also have the "avalanche effect". One property
 * of this is flipping any input bit should flip each output bit with about 50% probabilty.
 * This makes it so extracting N bits from Avalanche(key) is much less likely
 * to cause collisions (but may not be faster overall due to extra instructions) in a hash
 * table with a power of 2 array size than taking N bits from (key) directly for
 * "real world data".
 */

/*
 * xorshift-multiply constructions are somewhat common for this kind of thing.
 * The Murmur3 fmix32 finalizer uses the same construction, but with different
 * constants [16 0x85ebca6b 13 0xc2b2ae35 16].
 *
 * From Pelle Evensen's blog https://mostlymangling.blogspot.com/2018/07/on-mixing-functions-in-fast-splittable.html
 * "multiplication is good for mangling bits upwards and xorshift right works alright for mangling downwards"
 *
 * The constants here from "lowbias32" by Chris Wellons (skeeto)
 * https://github.com/skeeto/hash-prospector,
 * though that page mentions that constants [16 0x21f0aaad 15 0xd35a2d97 15] should be better.
 */
inline uint32_t Avalanche(uint32_t x)
{
    // "lowbias32" constants
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

/*
 * mx3::mix version 3 by John Maiga 
 * https://github.com/jonmaiga/mx3/releases/tag/v3.0.0
 *
 * This has good PractRand score when used as a counter-based PRNG,
 * but for just hash table purposes it may be a bit too expensive.
 */
enum : uint64_t { MX3_C = 0xbea225f9eb34556d };

inline uint64_t Avalanche(uint64_t x)
{
    x ^= x >> 32;
    x *= MX3_C;
    x ^= x >> 29;
    x *= MX3_C;
    x ^= x >> 32;
    x *= MX3_C;
    x ^= x >> 29;
    return x;
}

/*
 * mx3::mix_stream
 *
 * No idea, but _maybe_ this could be decent on its own as a "hash_combine".
 */
inline uint64_t MixCombine(uint64_t h, uint64_t x)
{
    // This structure looks pretty similar to MurmurHash2 MurmurHash64A variant.
    x *= MX3_C;
    x ^= x >> 39;
    x *= MX3_C;

    h += x;
    h *= MX3_C;
    return h;
}

/*
 * Smaller implementation of mx3::hash with a fixed seed of 0.
 */
inline uint64_t HashBytes64(const void* data, size_t len)
{
    // const size_t originalTotalLength = len;
    uint64_t h = MixCombine(0, len + 1); // seed = 0

    // UB: unaligned loads and strict aliasing
    // XXX: __unaligned is MSVC only, though not sure it actually helps
    // Computes different hash on big-endian systems.
    const uint64_t __unaligned* p64 = reinterpret_cast<const uint64_t *>(data);

    while (len >= 8) {
        h = MixCombine(h, *p64++);
        len -= 8;
    }

    int j = int(len) - 1; // subtract sets flags on x86 and could want -1 value later
    if (j >= 0) {
        const uint8_t* const p8 = reinterpret_cast<const uint8_t *>(p64);
        uint64_t x = 0;

#if 0 // this might be interesting
        if (originalTotalLength >= 8) {
            unsigned nMissingBytes = (j ^ 7); // (8 - len)
            const uint64_t __unaligned* _p64 = reinterpret_cast<const uint64_t *>(p8 - nMissingBytes);
            x = *_p64 >> (nMissingBytes * 8);
        }
        else
#endif
        // This _seems_ like it should be a bit better than
        //      `unsigned i = 0; do x |= uint64_t(p8[i]) << (i * 8); while (++i < 8);`
        // since there isn't a (i * 8) and the shift doesn't have to wait for the load.
        do {
            uint8_t const b = p8[unsigned(j)];
            x <<= 8; // nop first iteration
            x |= b;
        } while (--j >= 0);

        h = MixCombine(h, x);
    }

    // TODO(?): have a "cheap" version for tables that can tolerate collisions well
    // that maybe does nothing here or just a xorshift.
    //
    // Also, if a hash table is not even involved, like for file checksums where the hash's only use
    // is comparing all of it (not a suset of bits) for equality, doing something invertible here is
    // not beneficial, though for such cases it may hardly matter.
    return Avalanche(h);
}


// Other potentially interesing things related to "bit mangling":
/*


32-bit ouput PRNG: PCG
64-bit ouput PRNG: xoshiro256**, see update on https://nullprogram.com/blog/2017/09/21/


Daniel Lemire "A fast alternative to the modulo reduction"
https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
https://github.com/lemire/fastrange
Sequence of x values _must_ be distributed well over U32 range (see Avalanche).
Uses:
- Random number generation when speed is preferred over potential bias
(see Division with Rejection (Unbiased) https://www.pcg-random.org/posts/bounded-rands.html).
- More varied hash table sizes.
```
    uint32_t lemire_reduce_fast(uint32_t x, uint32_t N)
    {
        return uint32_t( ( uint64_t(x) * N ) >> 32 );
    }
```


Daniel Lemire "Faster remainders when the divisor is a constant: beating compilers and libdivide"
https://lemire.me/blog/2019/02/08/faster-remainders-when-the-divisor-is-a-constant-beating-compilers-and-libdivide/
Paper: "Faster Remainder by Direct Computation: Applications to Compilers and Software Libraries",
Daniel Lemire, Owen Kaser, Nathan Kurz https://arxiv.org/pdf/1902.01961.pdf
```
    uint32_t d = ...; // your divisor > 0

    uint64_t c = UINT64_C(0xFFFFFFFFFFFFFFFF) / d + 1;

    // fastmod computes (n mod d) given precomputed c
    uint32_t fastmod(uint32_t n )
    {
        uint64_t lowbits = c * n;
        return ((__uint128_t)lowbits * d) >> 64;
    }
```


Shuffle (my misc stuff)


Counter-based/stateless/random-access PRNG (like mx3::random).
Called a Noise-Based RNG in GDC 2017 talk by Squirrel Eiserloh:
https://www.youtube.com/watch?v=LWFzPP8ZbdU


Non-repeating random sequence of length N:
Have sets of constants for N-bit reversible function, use rejection, at worst 50% are rejected.
Maybe some seed thing in the middle.


"Swiss tables":
https://abseil.io/about/design/swisstables
Full    = 0b0'xxxxxxx // 7 bit "H2" hash
Empty   = 0b1'0000000
Deleted = 0b1'1111111
^ My thinking: there's 127 encodings for deleted a entry when only 1 is needed.
What if instead of the "H2" hash being some 7 bits extracted from the primary hash,
H2 is the primary hash mod 251 (constant, but could also be Lemire fast range
for avalanche-quality hashes). Then encodings could be something like:
Full    = [0:250]
Empty   = 254
Deleted = 255


Folly F14: even fancier swiss table?
*/
