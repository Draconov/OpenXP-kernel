#include "shell_icons.h"
#include "../../kernel/user/shell_abi.h"
static unsigned rd16(const uint8_t *p){return (unsigned)p[0] | (unsigned)p[1]<<8;}
static unsigned rd32(const uint8_t *p){return rd16(p) | rd16(p+2)<<16;}
int oxp_shell_icons_parse(oxp_shell_icons_t *out,const uint8_t *data,size_t n){
    if(!out)return 0;
    out->pixels=0;
    if(!data || n!=OXP_SHELL_ICON_BYTES || data[0]!='O'||data[1]!='X'||
       data[2]!='U'||data[3]!='A' || rd16(data+4)!=1u ||
       rd16(data+6)!=OXP_SHELL_ICON_W || rd16(data+8)!=OXP_SHELL_ICON_H ||
       rd16(data+10)!=0u || rd32(data+12)!=OXP_SHELL_ICON_W*OXP_SHELL_ICON_H*3u)
        return 0;
    out->pixels=data+16;
    return 1;
}
static uint16_t over565(uint16_t dst,uint16_t fg,unsigned a){
    if(a==255u)return fg;
    if(a==0u)return dst;
    if(dst==OXP_SHELL_TRANSPARENT)dst=0u;
    unsigned rb=((fg>>11u)&31u)*a + ((dst>>11u)&31u)*(255u-a);
    unsigned gb=((fg>>5u)&63u)*a + ((dst>>5u)&63u)*(255u-a);
    unsigned bb=(fg&31u)*a+(dst&31u)*(255u-a);
    return (uint16_t)(((rb/255u)<<11u)|((gb/255u)<<5u)|(bb/255u));
}
void oxp_shell_icon_row(const oxp_shell_icons_t *icons,unsigned icon,
                        unsigned x,unsigned y,unsigned size,unsigned row,
                        uint16_t *dst,unsigned dst_w){
    if(!icons || !icons->pixels || !dst || icon>=OXP_SHELL_ICON_COUNT ||
       !size || row<y || row-y>=size || x>=dst_w)return;
    unsigned sy=(icon/OXP_SHELL_ICON_COLUMNS)*OXP_SHELL_ICON_DIM +
                (unsigned)(((uint64_t)(row-y)*OXP_SHELL_ICON_DIM)/size);
    unsigned vis=size<dst_w-x?size:dst_w-x;
    for(unsigned dx=0;dx<vis;++dx){
        unsigned sx=(icon%OXP_SHELL_ICON_COLUMNS)*OXP_SHELL_ICON_DIM +
                    (unsigned)(((uint64_t)dx*OXP_SHELL_ICON_DIM)/size);
        unsigned off=3u*(sy*OXP_SHELL_ICON_W+sx);
        unsigned alpha=icons->pixels[off+2];
        if(!alpha)continue;
        uint16_t fg=(uint16_t)((unsigned)icons->pixels[off]|
                               (unsigned)icons->pixels[off+1]<<8);
        dst[x+dx]=over565(dst[x+dx],fg,alpha);
    }
}
