#include "ByteStream.h"
#include <stdarg.h>

// May set error or overflow (FixedBuffer) flags.
// _May change begin/end/cap_.
bool ByteStream::Flush()
{
    switch (mode) {
    case FlushMode::FixedBuffer: return static_cast<FixedBufferByteStream*>(this)->Flush();
    default: unreachable;
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
            this->PutByte(ch0);
        } break;
        }
    }
}
