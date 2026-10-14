#ifndef OPENXP_WALLPAPER_H
#define OPENXP_WALLPAPER_H
#include <stdint.h>
#include <stddef.h>
#define OXP_WALL_W 160u
#define OXP_WALL_H 120u
/* Return exactly n bytes of immutable file at offset, otherwise false. */
typedef int (*oxp_wall_read_fn)(void *context,uint32_t offset,void *out,uint32_t n);
int oxp_wall_decode(oxp_wall_read_fn read,void *context,uint16_t *pixels);
uint16_t oxp_wall_rgb565(uint8_t r,uint8_t g,uint8_t b);
#endif
