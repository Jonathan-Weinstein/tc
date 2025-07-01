#include "lex.h"
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

    if (zext <= UINT32_MAX) {
        if (isUnsigned)              token->xdata.number.htk = htk_uint;
        else if (zext >= (1u << 31)) token->xdata.number.htk = shift == 0 ? htk_longlong : htk_uint;
        else                         token->xdata.number.htk = htk_int;
    }
    else {
        NotImplemented; // 64-bit integers
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

    TokenKind kind = Token_EOF;
    switch (c) {
    case '-': kind = Token_Minus;           break;
    case '{': kind = Token_CurlyBraceOpen;  break;
    case '}': kind = Token_CurlyBraceClose; break;
    case ',': kind = Token_Comma;           break;
    case '=': kind = Token_Assign;          break;
    case '*':
        if (*p == '/') {
            p++;
            Verify(0); // @invalid_source: `*/` without matching prior `/*`
        }
        else {
            NotImplemented;
        }
        break;
    case '\0': {
        if (p >= pSentinel) {
            ASSERT(p == pSentinel + 1);
            p = pSentinel;
            kind = Token_EOF;
        }
        else {
            NotImplemented;
        }
    } break;
    case '0': {
        uint const x = *p;
        uint const maybeLower = x | 32u;
        uint shift;
        if (x == '.' || maybeLower == 'e') {
            NotImplemented; // floating point, but its 0
            break;
        }
        else if (maybeLower == 'x') {
            shift = 4; // hex (base 16)
        }
        else if (maybeLower == 'b') {
            shift = 1; // binary (base 2)
        }
        else {
            shift = 3; // octal (base 8)
            --p; // will probably just be 0 and this will be quite roundabout...
        }
        // scan power of 2 base U64 integer:
        char msdChar;
        while ((msdChar = *p) == '0')
            p++;

        unsigned const base = 1 << shift;
        uint64_t       accum = 0;
        unsigned       count = 0;
        for (;; ++p) {
            if (*p != DigitSep) {
                uint const d = DigitValue(*p);
                if (d >= base)
                    break;
                count++;
                accum = accum << shift | d;
            }
            else if (p[1] == DigitSep) {
                // This isn't allowed, but does it really matter?
                Verify(0); // @invalid_source, consecutive digit sep
            }
        }
        if (*p == DigitSep) {
            // This isn't allowed, but does it really matter?
            Verify(0); // @invalid_source, trailing digit sep
        }

        if (count > 16) { // unlikely
            if (shift == 3) {
                // octal, 64 == 21*3 + 1
                Verify(count < 22 || (count == 22 && msdChar == '1')); // @invalid_source, overflow
            }
            else if (shift == 1) {
                // binary
                Verify(count <= 64); // @invalid_source, overflow
            }
            else {
                // hex
                Verify(0); // @invalid_source, overflow
            }
        }

        kind = Token_NumberLiteral;
        p = FinishIntegerLiteral(p, &token, accum, shift);
    } break;
    case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
        // Scan nonzero base 10 number.
        char const msdChar = c;
        unsigned constexpr base  = 10;
        uint64_t           accum = 0;
        unsigned           count = 0;
        for (;; ++p) {
            if (*p != DigitSep) {
                uint const d = *p - '0';
                if (d >= base)
                    break;
                count++;
                accum = accum*base + d;
            }
            else if (p[1] == DigitSep) {
                // This isn't allowed, but does it really matter?
                Verify(0); // @invalid_source, consecutive digit sep
            }
        }
        if (*p == DigitSep) {
            // This isn't allowed, but does it really matter?
            Verify(0); // @invalid_source, trailing digit sep
        }

        if (*p == '.' || (*p | 32) == 'e') {
            NotImplemented; // floating point
        }
        else {
            // Check for uint64_t overflow in a maybe bad way but doesn't add code to the loop.
            // MSVC's std::from_chars handles all bases much nicer looking, but the way here
            // and for non-base 10 is maybe faster or more unique.
            if (count >= 20) {
                // UINT64_MAX has 20 digits with a most significant digit of 1 (at digit[19]).
                // { 2 * 10^19 / 2^64 } has a quotient of 1 and a remainder that is < 10*19.
                // Since that quotient is 1, if a 20 digit number has a MSD of 1 and it mod 2^64 is < 10*19,
                // it must be > UINT64_MAX.
                if (count != 20 || msdChar != '1' || accum < 10'000'000'000'000'000'000u) {
                    Verify(0); // @invalid_source: integer literal too big
                }
            }
            kind = Token_NumberLiteral;
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
        kind = Token_Name;
    } break;
    default: {
        Verify(0); // @invalid_source: bad byte value (as the start of token)
    } break;
    } // end switch
    pScanner->pCurrent = p;
    size_t const length = p - pFirstByte;
    ImplementedIf(length < 1023u);
    token.length = uint16_t(length);
    token.kind = kind;
    return token;
}

#if BUILD_TESTS && 0
#include <stdio.h>
static void ScannerTest()
{
    puts(__FUNCTION__);
    Token t = { };
    FinishIntegerLiteral("",  &t, 1,                 10); Verify(t.xdata.number.htk == htk_int);
    FinishIntegerLiteral("u", &t, 1,                 10); Verify(t.xdata.number.htk == htk_uint);
    FinishIntegerLiteral("",  &t, 1u << 31,          10); Verify(t.xdata.number.htk == htk_longlong);
    FinishIntegerLiteral("u", &t, 1u << 31,          16); Verify(t.xdata.number.htk == htk_uint);
    //FinishIntegerLiteral("",  &t, (uint64_t)1 << 32, 10); Verify(t.xdata.number.htk == htk_ulonglong);
    //FinishIntegerLiteral("",  &t, (uint64_t)1 << 32, 16); Verify(t.xdata.number.htk == htk_ulonglong);
}
static const int s_test = []() { ScannerTest(); return 0; }();
#endif
