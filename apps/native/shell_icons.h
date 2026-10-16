#ifndef OPENXP_SHELL_ICONS_H
#define OPENXP_SHELL_ICONS_H
#include <stdint.h>
#include <stddef.h>
/* Aurora OXUA atlas: 12 original 32x32 icons, 6 columns x 2 rows.
 * The kernel never interprets these pixels; Ring 3 composites them into its
 * already-bounded shell overlay raster. No runtime PNG decoder is required. */
#define OXP_SHELL_ICON_COLUMNS 6u
#define OXP_SHELL_ICON_COUNT 12u
#define OXP_SHELL_ICON_DIM 32u
#define OXP_SHELL_ICON_W 192u
#define OXP_SHELL_ICON_H 64u
#define OXP_SHELL_ICON_BYTES (16u+OXP_SHELL_ICON_W*OXP_SHELL_ICON_H*3u)
typedef struct {const uint8_t *pixels;} oxp_shell_icons_t;
int oxp_shell_icons_parse(oxp_shell_icons_t *out,const uint8_t *data,size_t n);
/* Alpha-composite one row, using nearest-neighbor scaling and RGB565.
 * Transparent pixels leave the destination (including the transparency key)
 * untouched.  All source and destination clipping happens inside. */
void oxp_shell_icon_row(const oxp_shell_icons_t *icons,unsigned icon,
                        unsigned x,unsigned y,unsigned size,unsigned row,
                        uint16_t *dst,unsigned dst_w);
#endif
