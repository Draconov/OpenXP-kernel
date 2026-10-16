#ifndef OPENXP_SKIN_H
#define OPENXP_SKIN_H
#include <stdint.h>
#include <stddef.h>
/* OXSK 1: immutable build-time PNG conversion to RGB565 + 8-bit alpha.
 * The resource is loaded by windowd using its existing read-only FS API.
 * No PNG codec, dynamic allocation or direct framebuffer access in Ring 3. */
#define OXP_SKIN_MAX_BYTES 65536u
#define OXP_SKIN_MAX_SPRITES 8u
#define OXP_SKIN_FRAME_ACTIVE 0u
#define OXP_SKIN_FRAME_INACTIVE 1u
#define OXP_SKIN_CAPTION_ATLAS 2u
#define OXP_SKIN_TASKBAR 3u
#define OXP_SKIN_START 4u
#define OXP_SKIN_HEADER_BYTES 16u
#define OXP_SKIN_RECORD_BYTES 20u
typedef int (*oxp_skin_read_fn)(void *,uint32_t,void *,uint32_t);
typedef struct {
    uint16_t id,w,h,left,right,top,bottom;
    uint32_t offset;
} oxp_skin_sprite_t;
typedef struct {
    const uint8_t *bytes;
    uint32_t length;
    unsigned count;
    oxp_skin_sprite_t sprites[OXP_SKIN_MAX_SPRITES];
} oxp_skin_t;
/* dst buffer is already composed; 0 alpha leaves it unchanged. */
int oxp_skin_parse(oxp_skin_t *skin,const uint8_t *blob,size_t n);
int oxp_skin_load(oxp_skin_t *skin,uint8_t blob[OXP_SKIN_MAX_BYTES],
                  oxp_skin_read_fn reader,void *context);
int oxp_skin_nine_row(const oxp_skin_t *skin,unsigned id,
                      unsigned x,unsigned y,unsigned w,unsigned h,
                      unsigned row,uint16_t *dst,unsigned dst_w);
int oxp_skin_region_row(const oxp_skin_t *skin,unsigned id,
                        unsigned sx,unsigned sy,unsigned sw,unsigned sh,
                        unsigned x,unsigned y,unsigned w,unsigned h,
                        unsigned row,uint16_t *dst,unsigned dst_w);
#endif
