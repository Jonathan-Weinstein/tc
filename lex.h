#pragma once
#include <stdint.h>

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

// High as in the "high level" source language.
enum HighTypeKind : uint8_t {
    // unsigned:
    htk_uint,
    htk_ulonglong,
    // not-unsigned, doesn't really matter for 8-bit bool as value is either 0/1:
    htk_bool,
    htk_int,
    htk_longlong,
};

struct Token {
    TokenKind kind;
    union // Valid field determined by TokenKind.
    {
        struct {
            HighTypeKind htk;
        } number;
    } xdata;
    uint16_t length;

    uint32_t line; // TODO: remove
    const char* source; // 24-32 bit offset would be smaller, but ptr easier for debug

    union // Valid field determined by TokenKind.
    {
        union {
            uint64_t nonFpZext64;
        } number;
    } data;
};
