#ifndef OPENXP_SHELL_RASTER_H
#define OPENXP_SHELL_RASTER_H
#include <stddef.h>
#include <stdint.h>
#include "../../kernel/user/shell_abi.h"
/* Software-compose one RGB565 scene row from Ring 3 layout commands. */
void oxp_raster_row(const oxp_shell_draw_t *commands,size_t count,
                    unsigned layer,unsigned y,uint16_t *pixels);
/* Overlay icon rectangles and text on an already decoded wallpaper row,
 * skipping the full-width gradient bands. */
void oxp_raster_overlay_background(const oxp_shell_draw_t *commands,size_t n,
                                   unsigned y,uint16_t *pixels);
#endif
