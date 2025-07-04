#pragma once

#include <string.h>

#include "common.h"

class ByteStream {
protected:
    enum class FlushMode : uint8_t {
        FixedBuffer, // fixed capacity externally owned buffer
    };

    ubyte* begin = nullptr;
    ubyte* end = nullptr;
    ubyte* cap = nullptr;

    FlushMode mode;
    bool overflowed = false; // only used by FixedBuffer

    ByteStream(FlushMode mode) : mode(mode) { }

public:
    ByteStream(const ByteStream&) = delete;
    ByteStream& operator=(const ByteStream&) = delete;

    // May set error or overflow (FixedBuffer) flags.
    // _May change begin/end/cap_.
    bool Flush();

    void PutByteRepeated(ubyte c, size_t n)
    {
        while (n) {
            size_t room = cap - end;
            if (room == 0) {
                if (!Flush()) {
                    return;
                }
                room = cap - end;
            }
            ASSERT(room != 0);
            size_t const nclamp = Min(room, n);
            memset(end, c, nclamp);
            end += nclamp;
            n -= nclamp;
        }
    }

    void PutBytes(const void* src, size_t n)
    {
        while (n) {
            size_t room = cap - end;
            if (room == 0) {
                if (!Flush()) {
                    return;
                }
                room = cap - end;
            }
            ASSERT(room != 0);
            size_t const nclamp = Min(room, n);
            memcpy(end, src, nclamp);
            src = reinterpret_cast<const char*>(src) + nclamp;
            end += nclamp;
            n -= nclamp;
        }
    }

    void PutByte(ubyte c)
    {
        PutByteRepeated(c, 1);
    }

    void _printf_helper(_Printf_format_string_ char const* const fmt, ...);
};

inline void Print(ByteStream& bs, uint64_t ui)
{
    char tmp[24], *p = endof(tmp);
    do {
        *--p = (ui % 10u) + '0';
        ui /= 10u;
    } while (ui);
    bs.PutBytes(p, endof(tmp) - p);
}

inline void Print(ByteStream& bs, int64_t si)
{
    if (si < 0) {
        bs.PutByte('-');
        si = -si;
    }
    Print(bs, uint64_t(si));
}

inline void Print(ByteStream& bs, uint32_t ui) { Print(bs, uint64_t(ui)); }
inline void Print(ByteStream& bs, int32_t si) { Print(bs, int64_t(si)); }

inline void Print(ByteStream& bs, const char* str)
{
    bs.PutBytes(str, strlen(str));
}

// MSVC's _Printf_format_string_ only works with /analyze
#define ByteStream_printf(bs_ref, ...) ((void)sizeof(printf(__VA_ARGS__)), (bs_ref)._printf_helper(__VA_ARGS__))

template<typename T, typename... Args>
inline void Print(ByteStream& bs, T value, Args... args)
{
    Print(bs, value);
    Print(bs, args...);
}

class FixedBufferByteStream : public ByteStream {
public:
    FixedBufferByteStream(const FixedBufferByteStream&) = delete;
    FixedBufferByteStream& operator=(const FixedBufferByteStream&) = delete;

    FixedBufferByteStream(ubyte *mem, uint32_t capacity) : ByteStream(FlushMode::FixedBuffer)
    {
        ASSERT(capacity != 0);

        begin = mem;
        end = mem;
        cap = mem + capacity;
    }

    bool Flush()
    {
        ASSERT(cap >= end);
        if (end == cap) {
            end = begin;
            overflowed = true;
        }
        return true;
    }

    // Overflow causes wrap around:
    uint32_t WrappedSize() const { return uint32_t(end - begin); }
    bool Overflowed() const { return overflowed; }
    bool ClearOverflowed() { overflowed = false; }
};
