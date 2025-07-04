#pragma once
#include "utility/common.h"

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

// The kind of type for a high-level C-like language.
// Sorta ordered by "usual arithmetic conversions" rank.
enum Typekind : uint8_t {
    Typekind_Invalid,         // Internal.
    _Typekind_Placeholder,    // For MakeIntegerUnsigned, delete if no longer needed.
    Typekind_void,
    Typekind_bool,
    Typekind_s32,             // "int"
    Typekind_u32,             // "unsigned int" (or other spellings)
    Typekind_slong,
    Typekind_ulong,
    Typekind_slonglong,
    Typekind_ulonglong,

    _Typekind_EnumEnd,

    // These are the lowest rank type.
    // TODO: make a runtime value, this assumes Windows
    Typekind_s64_alias = Typekind_slonglong,
    Typekind_u64_alias = Typekind_ulonglong,
};

forceinline bool IsInteger(Typekind typekind)
{
    return typekind >= Typekind_s32 &&
           typekind <= Typekind_ulonglong;
}

forceinline bool IsIntegerOrBool(Typekind typekind)
{
    return typekind >= Typekind_bool &&
           typekind <= Typekind_ulonglong;
}

forceinline Typekind MakeIntegerUnsigned(Typekind typekind)
{
    static_assert(Typekind(Typekind_s32       | 1) == Typekind_u32   &&
                  Typekind(Typekind_slong     | 1) == Typekind_ulong &&
                  Typekind(Typekind_slonglong | 1) == Typekind_ulonglong, "delete _Typekind_placeholder and adjust LUT(s)");
    ASSERT(IsInteger(typekind));
    return Typekind(typekind | 1);
}

struct Token {
    TokenKind kind;
    union // Valid field determined by TokenKind.
    {
        struct {
            Typekind typekind;
        } number;
    } xdata;
    uint16_t length;

    uint32_t line; // TODO: remove
    const char* source; // 24-32 bit offset would be smaller, but ptr nicer for debugging

    union // Valid field determined by TokenKind.
    {
        union {
            // XXX: some code (constexpr eval) might have to load less than the entire 64-bit value,
            // in which case that and the lex code assume little-endian.
            uint64_t nonFpZext64;
        } number;
    } data;
};
