#include <stdio.h>

#include "utility/common.h"
#include "utility/ByteStream.h"

void DoSomething();

int main()
{
#if BUILD_TESTS
    puts("BUILD_TESTS: done.");
#endif

#if 0
    {
        // unsigned char buf[4]; // {xyz}
        unsigned char buf[2048]; // {a^^^~-56xyz}
        FixedBufferByteStream bs(buf, sizeof buf);

        bs.PutByte('a');
        bs.PutByteRepeated('!', 0);
        bs.PutByteRepeated('^', 3);
        bs.PutBytes("", 0);
        bs.PutBytes("~", 1);
        Print(bs, -5, 6u);
        Print(bs, "xyz");

        putchar('{');
        fwrite(buf, 1, bs.WrappedSize(), stdout);
        putchar('}');
        puts("");
    }

    {
        // unsigned char buf[4]; // {xyz}
        unsigned char buf[2048]; // {a^^^~-56xyz}
        FixedBufferByteStream bs(buf, sizeof buf);

        // {s = -1, u = 4000000123, s = hello, c = ^, percent = %100}
        ByteStream_printf(bs, "s = %d, u = %u, s = %s, c = %c, percent = %%100", -1, 4'000'000'123u, "hello", '^');

        putchar('{');
        fwrite(buf, 1, bs.WrappedSize(), stdout);
        putchar('}');
        puts("");
    }
#endif

    puts("Bye!");
    return 0;
}
