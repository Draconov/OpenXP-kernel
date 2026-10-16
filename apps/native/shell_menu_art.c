#include "shell_menu_art.h"
static unsigned little16(const uint8_t *p){return (unsigned)p[0]|((unsigned)p[1]<<8);}
static unsigned little32(const uint8_t *p){return little16(p)|(little16(p+2)<<16);}
int oxp_menu_art_parse(oxp_menu_art_t *art,const uint8_t *data,size_t n){
    if(!art)return 0;
    art->pixels=0;
    if(!data || n!=OXP_MENU_ART_BYTES || data[0]!='O' || data[1]!='X' ||
       data[2]!='U'||data[3]!='A' || little16(data+4)!=1u ||
       little16(data+6)!=OXP_MENU_ART_W || little16(data+8)!=OXP_MENU_ART_H ||
       little16(data+10)!=0u || little32(data+12)!=OXP_MENU_ART_W*OXP_MENU_ART_H*3u)return 0;
    art->pixels=data+16u;
    return 1;
}
typedef struct {unsigned x,y,w,h,left,right,top,bottom;} region_t;
static int region_geometry(unsigned id,region_t *r){
    switch(id){
    case OXP_MENU_ART_PANEL:*r=(region_t){0u,0u,96u,96u,12u,12u,12u,12u};break;
    case OXP_MENU_ART_HEADER:*r=(region_t){96u,0u,96u,28u,9u,9u,8u,6u};break;
    case OXP_MENU_ART_SELECTED:*r=(region_t){96u,28u,96u,24u,8u,8u,7u,7u};break;
    case OXP_MENU_ART_HOVER:*r=(region_t){96u,52u,96u,24u,8u,8u,7u,7u};break;
    case OXP_MENU_ART_FOOTER:*r=(region_t){96u,76u,96u,26u,9u,9u,7u,7u};break;
    case OXP_MENU_ART_DESKTOP_LABEL:*r=(region_t){96u,104u,96u,24u,9u,9u,7u,7u};break;
    default:return 0;
    }
    return 1;
}
static unsigned axis(unsigned at,unsigned target,unsigned source,unsigned start,unsigned end){
    if(target<=start+end)return (unsigned)((uint64_t)at*source/target);
    if(at<start)return at;
    if(at>=target-end)return source-(target-at);
    return start+(unsigned)((uint64_t)(at-start)*(source-start-end)/(target-start-end));
}
static uint16_t blend565(uint16_t back,uint16_t fore,unsigned a){
    if(a==255u)return fore;
    unsigned inv=255u-a;
    unsigned red=(((back>>11u)&31u)*inv+((fore>>11u)&31u)*a+127u)/255u;
    unsigned green=(((back>>5u)&63u)*inv+((fore>>5u)&63u)*a+127u)/255u;
    unsigned blue=((back&31u)*inv+(fore&31u)*a+127u)/255u;
    uint16_t out=(uint16_t)((red<<11u)|(green<<5u)|blue);
    /* Reserve 0x0001 as the compositor's transparency key. */
    return out==0x0001u?0u:out;
}
int oxp_menu_art_row(const oxp_menu_art_t *art,unsigned region,
                     unsigned x,unsigned y,unsigned w,unsigned h,
                     unsigned row,uint16_t *dst,unsigned dw){
    region_t src;
    if(!art||!art->pixels||!dst||!region_geometry(region,&src)||!w||!h||
       row<y||row-y>=h||x>=dw)return 0;
    unsigned iy=src.y+axis(row-y,h,src.h,src.top,src.bottom);
    unsigned limit=w<dw-x?w:dw-x;
    for(unsigned dx=0;dx<limit;++dx){
        unsigned ix=src.x+axis(dx,w,src.w,src.left,src.right);
        unsigned offset=3u*(iy*OXP_MENU_ART_W+ix);
        unsigned alpha=art->pixels[offset+2u];
        if(!alpha)continue;
        uint16_t foreground=(uint16_t)((unsigned)art->pixels[offset]|
                                        ((unsigned)art->pixels[offset+1u]<<8));
        dst[x+dx]=blend565(dst[x+dx],foreground,alpha);
    }
    return 1;
}
