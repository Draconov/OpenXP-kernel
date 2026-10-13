#ifndef OPENXP_TEXT_EDIT_H
#define OPENXP_TEXT_EDIT_H
#include <stddef.h>
/* Small bounded insert/delete primitives for standalone native editors. */
static inline int oxp_insert(char *text,size_t *length,size_t *cursor,size_t cap,char ch){
    if(!text||!length||!cursor||*cursor>*length||*length+1>=cap)return 0;
    for(size_t i=*length+1;i>*cursor;--i)text[i]=text[i-1];
    text[*cursor]=ch;++*length;++*cursor;return 1;
}
static inline int oxp_backspace(char *text,size_t *length,size_t *cursor){
    if(!text||!length||!cursor||!*cursor||*cursor>*length)return 0;
    for(size_t i=*cursor-1;i<*length;++i)text[i]=text[i+1];
    --*cursor;--*length;return 1;
}
static inline int oxp_delete(char *text,size_t *length,size_t *cursor){
    if(!text||!length||!cursor||*cursor>=*length)return 0;
    for(size_t i=*cursor;i<*length;++i)text[i]=text[i+1];
    --*length;return 1;
}
#endif
