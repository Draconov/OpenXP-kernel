#ifndef OPENXP_VIDEO_FB_H
#define OPENXP_VIDEO_FB_H
#include <stdbool.h>
#include <stdint.h>
#include "limine_min.h"
typedef struct { volatile uint8_t *pixels; int width,height,pitch; uint8_t red_size,green_size,blue_size,red_shift,green_shift,blue_shift; } fb_surface_t;
bool fb_init(fb_surface_t *s, const struct limine_framebuffer *limine);
uint32_t fb_rgb(const fb_surface_t *s,uint8_t r,uint8_t g,uint8_t b);
void fb_rect(fb_surface_t *s,int x,int y,int w,int h,uint32_t col);
void fb_outline(fb_surface_t *s,int x,int y,int w,int h,uint32_t col);
void fb_text(fb_surface_t *s,int x,int y,const char *t,int scale,uint32_t col);
void fb_text_limit(fb_surface_t *s,int x,int y,const char *t,int max_chars,int scale,uint32_t col);
#endif
