#include "lex.h"
#include "tc_common.h"
#include "utility/common.h"
#include "utility/str.h"

struct Scanner {
    const char* pCurrent = nullptr;
    const char* pSentinel = nullptr;
    uint        line = 0;

    Scanner(view<const char> source)
        : pCurrent(source.begin())
        , pSentinel(source.end())
        , line(1)
    {
    }
};

static forceinline bool IsNameFirstChar(char c)
{
    return isalpha_simple(c) || (c == '_');
}

static forceinline bool IsNameTrailerChar(char c)
{
    return IsNameFirstChar(c) || ((c - '0') < 10u);
}

static forceinline unsigned DigitValue(char c)
{
    unsigned d = c - '0';
    if (d < 10u) return d;
    else         return ((c | 32) - 'a') + 0xa;
}

// A digit-separator must always follow a digit, so these are rejected:  0'  0x'AB  0xA''B  0xAB'
// There seems to be one other rule: literals like 0x'F and 0b'1111 are illegal, yet 0'17 is allowed.
// We just allow literals like 0x'F and 0b'1111.
static constexpr char DigitSep = '\'';

// There are no negative literals, -1 is a unary minus token followed by 1.
// shift is 0 for base 10, otherwise log2(base) for binary/octal/hex (1/3/4)
static const char* FinishIntegerLiteral(const char* p, Token* token, uint64_t zext, unsigned shift)
{
    token->kind = Token_NumberLiteral;
    token->data.number.nonFpZext64 = zext;

    bool isUnsigned = false;
    if ((*p | 32) == 'u') {
        ++p;
        isUnsigned = true;
    }
    // The "L" or "LL" suffix means the rank must be at least that,
    // but the type still needs to be able to represent the value.

    if (zext <= UINT32_MAX) {
        if (isUnsigned)              token->xdata.number.ctk = ctk_u32;
        else if (zext >= (1u << 31)) token->xdata.number.ctk = shift == 0 ? ctk_s64_alias : ctk_u32;
        else                         token->xdata.number.ctk = ctk_s32;
    }
    else {
        // TODO: @invalid_source message instead of just ctk_invalid.
        if (isUnsigned)                token->xdata.number.ctk = ctk_u64_alias;
        else if (zext >= (1uLL << 63)) token->xdata.number.ctk = shift == 0 ? ctk_invalid : ctk_u64_alias;
        else                           token->xdata.number.ctk = ctk_s64_alias;
    }

    // FP handled elsewhere
    ASSERT(*p != '.' && (*p | 32) != 'e' && (*p | 31) != 'p');

    if (IsNameTrailerChar(*p)) {
        NotImplemented; // other suffixes (could also be fp literal, like 1e6) or @invalid_source
    }

    return p;
}

Token ScanToken(Scanner* pScanner)
{
    const char* p = pScanner->pCurrent;
    const char* const pSentinel = pScanner->pSentinel;
    ubyte c;

    ASSERT(pSentinel >= p);
    ASSERT(*pSentinel == '\0');

    // Skip whitespace and comments:
    for (;;) {
        c = *p++;
        switch (c) {
        case '\n':
            pScanner->line++;
            continue;
        case ' ':
        case '\r': // assume \r is always followed by \n
        case '\t':
            continue;
        case '/':
            if (*p == '*') {
                // It should usually be faster to find / before * since there may be many of the latter for a style.
                // Skip a char since /*/ does not count as both /* and */,
                // but don't unconditionally skip since that could skip over a \n
                // and fail to increment pScanner->line.
                p += (*++p == '/');
                for (;; ++p) {
                    if (p >= pSentinel) {
                        ASSERT(p == pSentinel);
                        Verify(0); // @invalid_source: unterminated block comment
                    }
                    if ((*p == '/') && (p[-1] == '*')) {
                        ++p;
                        break;
                    }
                    // non-ASCII or non-print characters in comments are ignored
                    // assume \r is always followed by \n
                    pScanner->line += (*p == '\n'); // compiler may think aliasing
                }
                continue;
            }
            else if (*p != '/') {
                break;
            }
            p++; // C++ style line comment
            continue; // if we got \n, we'll go to that case next in the outer loop
        default:
            break; // for valid source, sentinel/EOF goes down this path
        } // switch
        break;
    } // loop
    const char* const pFirstByte = p - 1;
    ASSERT(*pFirstByte == c);
    Token token;
    token.kind   = Token_EOF;
    token.length = 0;
    token.line   = pScanner->line;
    token.source = pFirstByte;

    switch (c) {
    case '-': token.kind = Token_Minus;           break;
    case '{': token.kind = Token_CurlyBraceOpen;  break;
    case '}': token.kind = Token_CurlyBraceClose; break;
    case ',': token.kind = Token_Comma;           break;
    case '=': token.kind = Token_Assign;          break;
    case '*':
        if (*p == '/') {
            p++;
            Verify(0); // @invalid_source: `*/` without matching prior `/*`
        }
        else {
            NotImplemented;
        }
        break;
    case '0': {
        uint const x = *p;
        uint const xMaybeLower = x | 32u;
        uint shift;
        if (x == '.' || xMaybeLower == 'e') {
            NotImplemented; // floating point, though its 0
            break;
        }
        else if (xMaybeLower == 'x') {
            shift = 4; // hex (base 16)
            p++;
        }
        else if (xMaybeLower == 'b') {
            shift = 1; // binary (base 2)
            p++;
        }
        else {
            shift = 3; // octal (base 8), zero is handled here
        }
        unsigned const base = 1 << shift;
        uint64_t accum = 0;
        for (;; ++p) {
            // See comments near declaration of DigitSep
            if (*p == DigitSep)
                ++p;
            uint const d = DigitValue(*p);
            if (d >= base)
                break;
            const uint64_t accumShifted = accum << shift;
            if (accumShifted < accum)
                Verify(0); // @invalid_source, overflow
            accum = accumShifted | d;
        }
        if (p[-1] == DigitSep)
            Verify(0); // @invalid_source, digit sep not followed by digit
        p = FinishIntegerLiteral(p, &token, accum, shift);
    } break;
    case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
        // Scan nonzero and no-leading-zeros base 10 number.
        char const msdChar = c;
        unsigned constexpr base  = 10;
        uint64_t           accum = msdChar - '0';
        unsigned           count = 0; // after leading zeros
        for (;; ++p) {
            if (*p == DigitSep)
                ++p;
            uint const d = *p - '0';
            if (d >= base)
                break;
            count++;
            accum = accum*base + d;
        }
        if (p[-1] == DigitSep)
            Verify(0); // @invalid_source, digit sep not followed by digit

        if (*p == '.' || (*p | 32) == 'e') {
            NotImplemented; // floating point
        }
        else {
            // Check for uint64_t overflow in a maybe bad way but doesn't add code to the loop.
            // MSVC's std::from_chars handles all bases much nicer looking, but the way here
            // and for non-base 10 is maybe faster or more unique.
            if (count >= 20) {
                // UINT64_MAX has 20 digits with a most significant digit of 1 at digit[19].
                // { 2 * 10^19 / 2^64 } has a quotient of 1 and a remainder that is < 10*19
                // (that remainder is 2 * 10^19 - 1 * 2^64).
                // Since that quotient is 1, if a 20 digit number (should be >= 10^19) has a
                // MSD of 1 and it mod 2^64 is < 10^19, it must be > UINT64_MAX.
                if (count != 20 || msdChar != '1' || accum < 10'000'000'000'000'000'000u) {
                    Verify(0); // @invalid_source: integer literal too big
                }
            }
            p = FinishIntegerLiteral(p, &token, accum, 0);
        }
    } break;
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
    case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
    case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
    case '_': {
        while (IsNameTrailerChar(*p))
            p++;
        token.kind = Token_Name;
    } break;
    case '\0': {
        if (p >= pSentinel) {
            ASSERT(p == pSentinel + 1);
            p = pSentinel;
            token.kind = Token_EOF;
            break;
        }
    } // fallthrough
    default: {
        Verify(0); // @invalid_source: bad byte value (as the start of token)
    } break;
    } // end switch
    pScanner->pCurrent = p;
    size_t const length = p - pFirstByte;
    ImplementedIf(length < 1023u);
    token.length = uint16_t(length);
    return token;
}

#if BUILD_TESTS
static void ScannerTest()
{
    {
        Scanner s(R"(
0 00 0x0 0b0
1 1u
4'000'000'000 4'000'000'000u 0xFFFF'FFFF 0x7FFF'FFFF
0b101 077 0x7aFAf
)"_view);
        static const struct { CxxTypeKind ctk; uint64_t zext; } expected[] = {
            { ctk_s32, 0 },
            { ctk_s32, 0 },
            { ctk_s32, 0 },
            { ctk_s32, 0 },

            { ctk_s32, 1 },
            { ctk_u32, 1 },

            { ctk_s64_alias, 4'000'000'000 },
            { ctk_u32,       4'000'000'000 },
            { ctk_u32,       0xFFFF'FFFF },
            { ctk_s32,       0x7FFF'FFFF },

            { ctk_s32, 5 },
            { ctk_s32, 63 },
            { ctk_s32, 0x7aFAf },
        };
        Token t;
        uint i = 0;
        for (; (t = ScanToken(&s)).kind != Token_EOF; i++) {
            Verify(t.kind = Token_NumberLiteral);
            Verify(t.xdata.number.ctk == expected[i].ctk);
            Verify(t.data.number.nonFpZext64 == expected[i].zext);
        }
        Verify(i == countof(expected));
    }
}
INVOKE_TEST(ScannerTest);
#endif
