#ifndef OPENXP_SHELL_RASTER_H
#define OPENXP_SHELL_RASTER_H
#include <stddef.h>
#include <stdint.h>
#include "font_engine.h"
void oxp_raster_set_font(const oxp_font_t *font);
unsigned oxp_raster_text_width(const char *text,unsigned scale);
#include "../../kernel/user/shell_abi.h"
/* Software-compose one RGB565 scene row from Ring 3 layout commands. */
void oxp_raster_row(const oxp_shell_draw_t *commands,size_t count,
                    unsigned layer,unsigned y,uint16_t *pixels);
/* Overlay icon rectangles and text on an already decoded wallpaper row,
 * skipping the full-width gradient bands. */
void oxp_raster_overlay_background(const oxp_shell_draw_t *commands,size_t n,
                                   unsigned y,uint16_t *pixels);
/* Preserve an already-painted row and overlay shell UI commands. */
/* Redraw labels after compositing an image-based desktop hover plate. */
void oxp_raster_background_text_row(const oxp_shell_draw_t *commands,size_t n,
                                    unsigned y,uint16_t *pixels);
void oxp_raster_over_row(const oxp_shell_draw_t *commands,size_t n,
                         unsigned layer,unsigned y,uint16_t *pixels);
#endif
