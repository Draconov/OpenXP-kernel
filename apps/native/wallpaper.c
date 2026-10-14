#include "wallpaper.h"
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static unsigned le16(const uint8_t *p){return (unsigned)p[0]|((unsigned)p[1]<<8);}
uint16_t oxp_wall_rgb565(uint8_t r,uint8_t g,uint8_t b){
    return (uint16_t)(((uint16_t)(r>>3)<<11)|((uint16_t)(g>>2)<<5)|(b>>3));
}
/* Reject malformed headers and integer overflow before allocating/reading.
 * One bounded row buffer works for any 24-bit BMP up to 1024x768. */
int oxp_wall_decode(oxp_wall_read_fn read,void *ctx,uint16_t *dst){
    if(!read||!dst)return 0;
    uint8_t header[54];if(!read(ctx,0,header,54))return 0;
    if(header[0]!='B'||header[1]!='M'||le32(header+14)!=40 ||
       le16(header+26)!=1||le16(header+28)!=24||le32(header+30)!=0)return 0;
    uint32_t w=le32(header+18),h=le32(header+22),offset=le32(header+10),size=le32(header+2);
    if(w<1||w>1024||h<1||h>768||offset<54)return 0;
    uint32_t stride=(w*3u+3u)&~3u;
    if((uint64_t)offset+(uint64_t)stride*h!=size)return 0;
    uint8_t row[3072];
    for(uint32_t y=0;y<OXP_WALL_H;++y){
        uint32_t sy=(y*h)/OXP_WALL_H;
        uint32_t pos=offset+(h-1u-sy)*stride;
        if(!read(ctx,pos,row,w*3u))return 0;
        for(uint32_t x=0;x<OXP_WALL_W;++x){
            uint32_t sx=(x*w)/OXP_WALL_W, i=sx*3u;
            dst[y*OXP_WALL_W+x]=oxp_wall_rgb565(row[i+2],row[i+1],row[i]);
        }
    }
    return 1;
}
