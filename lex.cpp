#include "utility/common.h"
#include "utility/str.h"

enum TokenKind : uint8_t {
    Token_EOF,              // end of input

    Token_Name,             // AKA identifier
    Token_NumberLiteral,    // 0, 0xCDBA

    Token_Minus,            // -

    Token_CurlyBraceOpen,   // {
    Token_CurlyBraceClose,  // }
    Token_Comma,            // ,
    Token_Assign,           // =
};

union NumberUnion {
    uint32_t ui32;
};

struct Token {
    TokenKind kind;
    union // Valid field determined by TokenKind.
    {
        struct {
            uint8_t flags;
        } number;
    } xdata;
    uint16_t length; // TODO: check max length not exceeded

    uint line;
    const char* source;

    union // Valid field determined by TokenKind.
    {
        NumberUnion number;
    } data;
};

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

static bool IsIdentifierTrailingChar(char ch)
{
    return (ch == '_') || isalpha_simple(ch) || ((ch - '0') < 10u);
}

static unsigned DigitValue(char c)
{
    unsigned d = c - '0';
    if (d < 10u)
        return d;
    else
        return ((c | 32) - 'a') + 0xa;
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
    Token token = { };
    token.source = pFirstByte;
    token.line = pScanner->line;

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
    case '0':
    case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
        NotImplemented;
    } break;
    default: {
        if (IsIdentifierTrailingChar(c)) {
            kind = Token_Name;
            c = *p;
            while (IsIdentifierTrailingChar(c)) {
                c = *++p;
            }
        }
        else {
            Verify(0); // @invalid_source: bad byte value
        }
    } break;
    } // end switch
    pScanner->pCurrent = p;
    size_t const length = p - pFirstByte;
    token.length = uint16_t(length);
    token.kind = kind;
    return token;
}
