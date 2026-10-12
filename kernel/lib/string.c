/* Freestanding C runtime primitives. Volatile byte accesses prevent the
 * compiler from rewriting these implementations as calls to themselves. */
#include <stddef.h>
#include <stdint.h>
void *memset(void *dst, int value, size_t count) {
    volatile unsigned char *p = (volatile unsigned char *)dst;
    for (size_t i = 0; i < count; ++i) p[i] = (unsigned char)value;
    return dst;
}
void *memcpy(void *dst, const void *src, size_t count) {
    volatile unsigned char *d = (volatile unsigned char *)dst;
    const volatile unsigned char *s = (const volatile unsigned char *)src;
    for (size_t i = 0; i < count; ++i) d[i] = s[i];
    return dst;
}
void *memmove(void *dst, const void *src, size_t count) {
    volatile unsigned char *d = (volatile unsigned char *)dst;
    const volatile unsigned char *s = (const volatile unsigned char *)src;
    if (d == s) return dst;
    if ((uintptr_t)d < (uintptr_t)s || (uintptr_t)d - (uintptr_t)s >= count) {
        for (size_t i = 0; i < count; ++i) d[i] = s[i];
    } else {
        for (size_t i = count; i > 0; --i) d[i - 1] = s[i - 1];
    }
    return dst;
}
