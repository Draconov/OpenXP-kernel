#include "video/fb.h"
#include <stddef.h>
#include <stdint.h>
/* 5x7 original glyphs, uppercase, numeric and a small punctuation set. */
static const uint8_t font[37][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14},{0,0,0,0,0,0,0}
};static const uint8_t symbols[][7] = {
    {0,4,4,4,4,0,4},  /* ! */
    {0,0,0,31,0,0,0}, /* - */
    {0,0,0,0,0,12,12},/* . */
    {0,0,0,0,0,12,8}, /* , */
    {0,4,0,0,4,0,0}, /* : */
    {0,0,1,2,4,8,16},/* / */
    {16,8,4,2,1,0,0},/* backslash */
    {14,17,1,2,4,0,4},/* ? */
    {31,16,16,16,16,16,31},/* [ */
    {31,1,1,1,1,1,31},/* ] */
    {0,0,0,0,0,0,31},/* _ */
    {0,0,31,0,31,0,0},/* = */
    {0,0,4,31,4,0,0},/* + */
    {0,0,0,0,0,0,0},/* space */
    {2,4,8,8,8,4,2},/* ( */
    {8,4,2,2,2,4,8} /* ) */
};
static const uint8_t *glyph(char c) {
    if (c >= 'a' && c <= 'z') c=(char)(c-'a'+'A');
    if (c >= 'A' && c <= 'Z') return font[c-'A'];
    if (c >= '0' && c <= '9') return font[c-'0'+26];
    switch(c) {
    case '!': return symbols[0]; case '-':return symbols[1];
    case '.':return symbols[2];case ',':return symbols[3];
    case ':':return symbols[4];case '/':return symbols[5];
    case '\\':return symbols[6];case '?':return symbols[7];
    case '[':return symbols[8];case ']':return symbols[9];
    case '_':return symbols[10];case '=':return symbols[11];
    case '+':return symbols[12];case '(':return symbols[14];
    case ')':return symbols[15]; default:return symbols[13];
    }
}
bool fb_init(fb_surface_t *s,const struct limine_framebuffer *f) {
    if(!s||!f||!f->address||f->memory_model!=1||f->bpp!=32||f->width<320||f->height<240||
       f->width>4096||f->height>4096||f->pitch<f->width*4||f->pitch>INT32_MAX||
       !f->red_mask_size||f->red_mask_size>8||!f->green_mask_size||f->green_mask_size>8||
       !f->blue_mask_size||f->blue_mask_size>8||f->red_mask_shift>24||
       f->green_mask_shift>24||f->blue_mask_shift>24) return false;
    *s=(fb_surface_t){(volatile uint8_t*)f->address,(int)f->width,(int)f->height,(int)f->pitch,
        f->red_mask_size,f->green_mask_size,f->blue_mask_size,
        f->red_mask_shift,f->green_mask_shift,f->blue_mask_shift};
    return true;
}
uint32_t fb_rgb(const fb_surface_t *s,uint8_t r,uint8_t g,uint8_t b) {
    if(!s)return 0;
    return (((uint32_t)r >> (8u-s->red_size)) << s->red_shift)|
           (((uint32_t)g >> (8u-s->green_size)) << s->green_shift)|
           (((uint32_t)b >> (8u-s->blue_size)) << s->blue_shift);
}
void fb_rect(fb_surface_t *s,int x,int y,int w,int h,uint32_t col) {
    if(!s||!s->pixels||w<=0||h<=0)return;
    /* 64-bit endpoints prevent x+w/y+h overflow from corrupting framebuffer. */
    int64_t x0=x,y0=y,x1=(int64_t)x+w,y1=(int64_t)y+h;
    if (x0<0) x0=0;
    if (y0<0) y0=0;
    if (x1>s->width) x1=s->width;
    if (y1>s->height) y1=s->height;
    if(x1<=x0||y1<=y0)return;
    for(int64_t row=y0;row<y1;++row) {
        volatile uint32_t *p=(volatile uint32_t *)(s->pixels+row*s->pitch);
        for(int64_t colidx=x0;colidx<x1;++colidx)p[colidx]=col;
    }
}
void fb_outline(fb_surface_t *s,int x,int y,int w,int h,uint32_t col) {
    if(w<=0||h<=0)return;
    fb_rect(s,x,y,w,1,col);fb_rect(s,x,y+h-1,w,1,col);
    fb_rect(s,x,y,1,h,col);fb_rect(s,x+w-1,y,1,h,col);
}
void fb_text_limit(fb_surface_t *s,int x,int y,const char *t,int limit,int scale,uint32_t col) {
    if(!t||!s||scale<1||scale>4||limit<=0)return;
    for(int i=0;t[i] && i<limit;++i) {
        if ((int64_t)x+5*scale <0) { x+=6*scale; continue; }
        if (x>=s->width)break;
        const uint8_t *bitmap=glyph(t[i]);
        for(int row=0;row<7;++row)
            for(int bit=0;bit<5;++bit)
                if(bitmap[row]&(1u<<(4-bit)))fb_rect(s,x+bit*scale,y+row*scale,scale,scale,col);
        x+=6*scale;
    }
}
void fb_text(fb_surface_t *s,int x,int y,const char *t,int scale,uint32_t col) {
    fb_text_limit(s,x,y,t,256,scale,col);
}
