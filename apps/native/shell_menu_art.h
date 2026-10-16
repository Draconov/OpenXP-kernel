#ifndef OPENXP_SHELL_MENU_ART_H
#define OPENXP_SHELL_MENU_ART_H
#include <stdint.h>
#include <stddef.h>
/* Compact editable OXUA sprite atlas for the Ring 3 Start menu. This format
 * needs no PNG decoder in the running OS, and is kept independent from the
 * application's atlas allocations. */
#define OXP_MENU_ART_W 192u
#define OXP_MENU_ART_H 128u
#define OXP_MENU_ART_BYTES (16u+OXP_MENU_ART_W*OXP_MENU_ART_H*3u)
#define OXP_MENU_ART_PANEL 0u
#define OXP_MENU_ART_HEADER 1u
#define OXP_MENU_ART_SELECTED 2u
#define OXP_MENU_ART_HOVER 3u
#define OXP_MENU_ART_FOOTER 4u
#define OXP_MENU_ART_DESKTOP_LABEL 5u
typedef struct { const uint8_t *pixels; } oxp_menu_art_t;
int oxp_menu_art_parse(oxp_menu_art_t *art,const uint8_t *data,size_t bytes);
/* Draw one scanline; absent resources and out-of-bounds requests are harmless.
 * Nine-slice coordinates preserve the source corners when scaling a window. */
int oxp_menu_art_row(const oxp_menu_art_t *art,unsigned region,
                     unsigned x,unsigned y,unsigned w,unsigned h,
                     unsigned row,uint16_t *dst,unsigned dest_width);
#endif
