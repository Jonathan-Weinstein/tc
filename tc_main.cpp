#include <stdio.h>

#include "utility/common.h"
#include "utility/ByteStream.h"

void DoSomething();

int main()
{
#if BUILD_TESTS
    puts("BUILD_TESTS: done.");
#endif

    unsigned char buf[2048];
    FixedBufferByteStream bs(buf, sizeof buf);

    bs.PutByte('a');
    bs.PutByteRepeated('!', 0);
    bs.PutByteRepeated('*', 3);
    bs.PutBytes("", 0);
    bs.PutBytes("~", 1);
    Print(bs, -5, 6u);
    Print(bs, "xyz");

    putchar('{');
    fwrite(buf, 1, bs.FilledSize(), stdout);
    putchar('}');

    puts("Bye!");
    return 0;
}
