#include "ByteStream.h"
#include <stdarg.h>

// May set error or overflow (FixedBuffer) flags.
// May change begin/end/cap.
bool ByteStream::Flush()
{
    switch (mode) {
    case FlushMode::FixedBuffer: return static_cast<FixedBufferByteStream*>(this)->Flush();
    default: unreachable;
    }
}

void ByteStream::PutByteRepeated(ubyte c, size_t n)
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

void ByteStream::PutBytes(const void* src, size_t n)
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

void ByteStream::_printf_helper(_Printf_format_string_ char const* const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    for (const char* p = fmt;;) {
        char const ch0 = *p++;
        switch (ch0) {
        case '%': {
            char const ch1 = *p++;
            switch (ch1) {
            case '%': {
                this->PutByte('%');
            } break;
            case 'c': {
                this->PutByte(char(va_arg(args, int)));
            } break;
            case 's': {
                Print(*this, va_arg(args, const char*));
            } break;
            case 'u': {
                Print(*this, va_arg(args, unsigned));
            } break;
            case 'd': {
                Print(*this, va_arg(args, int));
            } break;
            default: {
                unreachable;
            } break;
            } // end-switch
        } break;
        case '\0':
            va_end(args);
            return;
        default: {
            this->PutByteFast(ch0);
        } break;
        }
    }
}
