#ifndef OPENXP_SYSTEM_CLIPBOARD_H
#define OPENXP_SYSTEM_CLIPBOARD_H
#include <stddef.h>
#include <stdint.h>
#define OPENXP_CLIPBOARD_CAP 960u
/* Clipboard is intentionally volatile; resets at boot and shared by apps. */
typedef struct { uint8_t bytes[OPENXP_CLIPBOARD_CAP]; size_t length; } openxp_clipboard_t;
static inline int openxp_clipboard_put(openxp_clipboard_t *c,const uint8_t *src,size_t n){
    if(!c||n>OPENXP_CLIPBOARD_CAP||(!src&&n))return 0;
    for(size_t i=0;i<c->length;++i)c->bytes[i]=0;
    for(size_t i=0;i<n;++i)c->bytes[i]=src[i];
    c->length=n;
    return 1;
}
static inline size_t openxp_clipboard_take(const openxp_clipboard_t *c,uint8_t *dst,size_t cap){
    if(!c||c->length>cap||(!dst&&c->length))return (size_t)-1;
    for(size_t i=0;i<c->length;++i)dst[i]=c->bytes[i];
    return c->length;
}
#endif
