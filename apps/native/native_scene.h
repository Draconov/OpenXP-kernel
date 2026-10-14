#ifndef OPENXP_NATIVE_SCENE_H
#define OPENXP_NATIVE_SCENE_H
#include <stdint.h>
#include "../../kernel/user/shell_abi.h"
/* Input data are sanitized kernel snapshots copied into the Ring 3 service.
 * The caller owns the backing application surfaces; no hardware mapping is
 * used for composition. Each output row is an RGB565 transparent overlay. */
void oxp_native_scene_row(const oxp_shell_window_t windows[OXP_NATIVE_SLOTS],
                          const oxp_native_info_t infos[OXP_NATIVE_SLOTS],
                          const uint16_t surfaces[OXP_NATIVE_SLOTS][OXP_NATIVE_W*OXP_NATIVE_H],
                          unsigned y, uint16_t dst[OXP_SHELL_RASTER_W]);
#endif
