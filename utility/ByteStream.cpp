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

/*
Ideas:

For convenience/safety, for non-FileByteStream, have a mode to always terminate (except for PutByteFast)?

Flush(): return view or scalar room?
*/


#if BUILD_TESTS
#include "str.h"
MSVC_PRAGMA(warning(push))
MSVC_PRAGMA(warning(disable : 4464)) // C4464: relative include path contains '..'
#include "../tc_common.h" // TODO: move INVOKE_TEST and friends to ./utility
MSVC_PRAGMA(warning(pop))

static void
UtilStringTest()
{
    // strcpy_max_strlen
    {
        char b1[1] = { 'x' }, *const maxlenp = max_strlen_ptr(b1);
        strcpy_result r = strcpy_max_strlen(b1, maxlenp, "");
        Verify(b1[0] == 0 && r.dst_sentinel == b1 && r.truncated == 0);
    }
    {
        char b1[1] = { 'x' };
        strcpy_result r = strcpy_max_strlen(b1, endof(b1) - 1, "a");
        Verify(b1[0] == 0 && r.dst_sentinel == b1 && r.truncated == 'a');
    }
    {
        char buf[8], * const maxlenp = max_strlen_ptr(buf);
        strcpy_result dst = { buf };
        dst = strcpy_max_strlen(dst.dst_sentinel, maxlenp, "abc");
        Verify(strcmp(buf, "abc") == 0 && dst.dst_sentinel == buf + strlen(buf) && dst.truncated == 0);
        dst = strcpy_max_strlen(dst.dst_sentinel, maxlenp, ":");
        Verify(strcmp(buf, "abc:") == 0 && dst.dst_sentinel == buf + strlen(buf) && dst.truncated == 0);
        dst = strcpy_max_strlen(dst.dst_sentinel, maxlenp, "0123");
        Verify(strcmp(buf, "abc:012") == 0 && dst.dst_sentinel == buf + strlen(buf) && dst.truncated == '3');
    }

    // stricmp_ascii_lower, memicmp_ascii_lower
    {
        struct Case {
            const char* a; const char* b; uint memlen; int memres; int strres;
        };
        static const Case cases[] = {
            { "",               "", 0,  0,  0 },
            { "_",             "_", 1,  0,  0 },
            { "a",              "", 0,  0, 'a' - 0 },
            { "",              "B", 0,  0,  0 - 'b' },
            { "e",             "G", 1, -2, -2 },
            { "abcxyz",   "ABCXYZ", 6,  0,  0 },
            { "ABCXYZ*", "abcxyzT", 7, (int)'*' - 't', (int)'*' - 't' },
        };
        for (const auto& tc : cases) {
            Verify(tc.memlen == Min(strlen(tc.a), strlen(tc.b)));
            Verify(stricmp_ascii_lower(tc.a, tc.b)            == tc.strres);
            Verify(memicmp_ascii_lower(tc.a, tc.b, tc.memlen) == tc.memres);
        }
    }

    // ByteStream
    {
        ubyte buf[128];
        memset(buf, ';', sizeof buf);
        FixedBufferByteStream bs(buf, sizeof buf);

        bs.PutByte('a');
        bs.PutByteRepeated('!', 0);
        bs.PutByteRepeated('^', 3);
        bs.PutBytes("", 0);
        bs.PutBytes("~", 1);
        Print(bs, -5, 6u);
        Print(bs, "xyz"_view);

        view<const char> v = "a^^^~-56xyz"_view;
        Verify(v.length < sizeof(buf) - 1 && buf[bs.WrappedSize()] == ';' && bs.WrappedSize() == v.length);
        Verify(memcmp(buf, v.ptr, v.length) == 0);
    }
    {
        ubyte buf[4];
        memset(buf, ';', sizeof buf);
        FixedBufferByteStream bs(buf, sizeof buf);

        bs.PutByte('a');
        bs.PutByteRepeated('!', 0);
        bs.PutByteRepeated('^', 3);
        bs.PutBytes("", 0);
        bs.PutBytes("~", 1);
        Print(bs, -5, 6u);
        Print(bs, "xyz"_view);

        view<const char> v = "xyz6"_view;
        Verify(memcmp(buf, v.ptr, v.length) == 0);
    }
    {
        ubyte buf[128];
        memset(buf, ';', sizeof buf);
        FixedBufferByteStream bs(buf, sizeof buf);

        ByteStream_printf(bs, "s=%d, u=%u, s=%s, c=%c, percent=%%100", -1, 4'000'000'123u, "hello", '^');

        view<const char> v = "s=-1, u=4000000123, s=hello, c=^, percent=%100"_view;
        Verify(v.length < sizeof(buf) - 1 && buf[bs.WrappedSize()] == ';' && bs.WrappedSize() == v.length);
        Verify(memcmp(buf, v.ptr, v.length) == 0);
    }
}
INVOKE_TEST(UtilStringTest);
#endif
