#include <stddef.h>
/* Deliberately tiny native userland runtime. No libc or kernel imports. */
void *memset(void *dst,int c,size_t n){
    volatile unsigned char *p=(volatile unsigned char *)dst;
    for(size_t i=0;i<n;++i)p[i]=(unsigned char)c;
    return dst;
}
void *memcpy(void *dst,const void *src,size_t n){
    volatile unsigned char *p=(volatile unsigned char *)dst;
    const volatile unsigned char *s=(const volatile unsigned char *)src;
    for(size_t i=0;i<n;++i)p[i]=s[i];
    return dst;
}
