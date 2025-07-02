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

// Sorta ordered by "usual arithmetic conversions" rank.
// "ctk" might read like "C-type kind".
// Lowercase since most C type keywords are lowercase.
enum CxxTypeKind : uint8_t {
    ctk_invalid,         // Internal.
    _ctk_placeholder,    // For MakeIntegerUnsigned, delete if no longer needed.
    ctk_void,
    ctk_bool,
    ctk_s32,             // "int"
    ctk_u32,             // "unsigned int" (or other spellings)
    ctk_slong,
    ctk_ulong,
    ctk_slonglong,
    ctk_ulonglong,
    _ctk_enum_end,

    // These are the lowest rank type.
    // TODO: make a runtime value, this assumes Windows
    ctk_s64_alias = ctk_slonglong,
    ctk_u64_alias = ctk_ulonglong,
};

forceinline bool IsInteger(CxxTypeKind ctk)
{
    return ctk >= ctk_s32 &&
           ctk <= ctk_ulonglong;
}

forceinline bool IsIntegerOrBool(CxxTypeKind ctk)
{
    return ctk >= ctk_bool &&
           ctk <= ctk_ulonglong;
}

forceinline CxxTypeKind MakeIntegerUnsigned(CxxTypeKind ctk)
{
    static_assert(CxxTypeKind(ctk_s32       | 1) == ctk_u32   &&
                  CxxTypeKind(ctk_slong     | 1) == ctk_ulong &&
                  CxxTypeKind(ctk_slonglong | 1) == ctk_ulonglong, "delete _ctk_placeholder and adjust LUT(s)");
    ASSERT(IsInteger(ctk));
    return CxxTypeKind(ctk | 1);
}

struct Token {
    TokenKind kind;
    union // Valid field determined by TokenKind.
    {
        struct {
            CxxTypeKind ctk;
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
