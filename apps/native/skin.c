#include "skin.h"
/* Resource input is untrusted (even if the default comes from the initrd).
 * Every pixel address is range-checked at load time; drawing clips to rows. */
static uint16_t u16(const uint8_t *p){return (uint16_t)(p[0]|(uint16_t)p[1]<<8);}
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|
    (uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int oxp_skin_parse(oxp_skin_t *skin,const uint8_t *b,size_t n){
    if(!skin)return 0;
    skin->count=0;skin->bytes=0;skin->length=0;
    if(!b || n<16u || n>OXP_SKIN_MAX_BYTES ||
       b[0]!='O'||b[1]!='X'||b[2]!='S'||b[3]!='K'||u16(b+4)!=1u ||
       u32(b+8)!=n || u32(b+12)!=0u)return 0;
    unsigned count=u16(b+6);
    if(!count || count>OXP_SKIN_MAX_SPRITES ||
       16u+count*OXP_SKIN_RECORD_BYTES>n)return 0;
    for(unsigned i=0;i<count;++i){
        const uint8_t *r=b+16u+i*20u;
        unsigned id=u16(r),w=u16(r+2),h=u16(r+4),
                 l=u16(r+6),right=u16(r+8),top=u16(r+10),bottom=u16(r+12);
        uint32_t off=u32(r+16);
        if(id>=OXP_SKIN_MAX_SPRITES || !w || !h || w>256u || h>256u ||
           l+right>=w || top+bottom>=h || u16(r+14)!=0u ||
           off<16u+count*20u || off>n || (uint64_t)w*h*3u>n-off)return 0;
        for(unsigned j=0;j<i;++j)if(skin->sprites[j].id==id)return 0;
        skin->sprites[i]=(oxp_skin_sprite_t){(uint16_t)id,(uint16_t)w,(uint16_t)h,
            (uint16_t)l,(uint16_t)right,(uint16_t)top,(uint16_t)bottom,off};
    }
    skin->bytes=b;skin->length=(uint32_t)n;skin->count=count;
    return 1;
}
int oxp_skin_load(oxp_skin_t *skin,uint8_t blob[OXP_SKIN_MAX_BYTES],
                  oxp_skin_read_fn reader,void *context){
    if(!skin||!blob||!reader)return 0;
    if(!reader(context,0,blob,16u))return 0;
    uint32_t total=u32(blob+8);
    if(total<16u || total>OXP_SKIN_MAX_BYTES)return 0;
    for(uint32_t off=16;off<total;){
        uint32_t chunk=total-off;
        if(chunk>256u)chunk=256u;
        if(!reader(context,off,blob+off,chunk))return 0;
        off+=chunk;
    }
    return oxp_skin_parse(skin,blob,total);
}
static const oxp_skin_sprite_t *sprite(const oxp_skin_t *skin,unsigned id){
    if(!skin||!skin->bytes)return 0;
    for(unsigned i=0;i<skin->count;++i)
        if(skin->sprites[i].id==id)return &skin->sprites[i];
    return 0;
}
static uint16_t blend(uint16_t back,uint16_t fore,unsigned a){
    if(a==255u)return fore;
    unsigned inv=255u-a;
    unsigned r=(((back>>11)&31u)*inv+((fore>>11)&31u)*a+127u)/255u;
    unsigned g=(((back>>5)&63u)*inv+((fore>>5)&63u)*a+127u)/255u;
    unsigned b=((back&31u)*inv+(fore&31u)*a+127u)/255u;
    return (uint16_t)((r<<11)|(g<<5)|b);
}
static void put(const oxp_skin_t *skin,const oxp_skin_sprite_t *s,
                unsigned src_x,unsigned src_y,uint16_t *dst,unsigned x){
    uint32_t p=s->offset+3u*(src_y*(unsigned)s->w+src_x);
    unsigned a=skin->bytes[p+2u];
    if(!a)return;
    uint16_t color=u16(skin->bytes+p);
    /* Reserved 0x0001 is the native-layer transparency key. Keep opaque
     * black and very dark blue visible rather than accidentally punching
     * holes in a native window. */
    uint16_t mixed=blend(dst[x],color,a);
    if(mixed==0x0001u)mixed=0x0000u;
    dst[x]=mixed;
}
int oxp_skin_region_row(const oxp_skin_t *skin,unsigned id,
                        unsigned sx,unsigned sy,unsigned sw,unsigned sh,
                        unsigned x,unsigned y,unsigned w,unsigned h,
                        unsigned row,uint16_t *dst,unsigned dst_w){
    const oxp_skin_sprite_t *s=sprite(skin,id);
    if(!s||!dst||!sw||!sh||!w||!h||row<y||row-y>=h||x>=dst_w||
       sx>=s->w||sy>=s->h||sw>s->w-sx||sh>s->h-sy)return 0;
    unsigned src_y=sy+(unsigned)((uint64_t)(row-y)*sh/h);
    unsigned bound=w;
    if(bound>dst_w-x)bound=dst_w-x;
    for(unsigned dx=0;dx<bound;++dx){
        unsigned src_x=sx+(unsigned)((uint64_t)dx*sw/w);
        put(skin,s,src_x,src_y,dst,x+dx);
    }
    return 1;
}
static unsigned axis(unsigned at,unsigned target,unsigned source,
                     unsigned left,unsigned right){
    if(target<=left+right)return (unsigned)((uint64_t)at*source/target);
    if(at<left)return at;
    if(at>=target-right)return source-(target-at);
    return left+(unsigned)((uint64_t)(at-left)*(source-left-right)/(target-left-right));
}
int oxp_skin_nine_row(const oxp_skin_t *skin,unsigned id,
                      unsigned x,unsigned y,unsigned w,unsigned h,
                      unsigned row,uint16_t *dst,unsigned dst_w){
    const oxp_skin_sprite_t *s=sprite(skin,id);
    if(!s||!dst||!w||!h||row<y||row-y>=h||x>=dst_w)return 0;
    unsigned src_y=axis(row-y,h,s->h,s->top,s->bottom);
    unsigned bound=w;
    if(bound>dst_w-x)bound=dst_w-x;
    for(unsigned dx=0;dx<bound;++dx){
        unsigned src_x=axis(dx,w,s->w,s->left,s->right);
        put(skin,s,src_x,src_y,dst,x+dx);
    }
    return 1;
}
