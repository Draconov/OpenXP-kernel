/* Test OpenXP's freestanding memory primitives against expected byte arrays. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    unsigned char a[64];
    memset(a, 0x5a, sizeof(a));
    for (unsigned i = 0; i < sizeof(a); ++i) assert(a[i] == 0x5a);
    unsigned char b[64];
    memcpy(b, a, sizeof(b));
    for (unsigned i = 0; i < sizeof(b); ++i) assert(b[i] == 0x5a);
    for (unsigned i = 0; i < sizeof(a); ++i) a[i] = (unsigned char)i;
    memmove(a + 4, a, 40);
    for (unsigned i = 0; i < 40; ++i) assert(a[4 + i] == i);
    memmove(a, a + 4, 40);
    for (unsigned i = 0; i < 40; ++i) assert(a[i] == i);
    assert(memmove(a, a, sizeof(a)) == a);
    puts("PASS: freestanding memset, memcpy, memmove, overlapping copies");
    return 0;
}
