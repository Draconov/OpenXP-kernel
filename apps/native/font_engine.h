#ifndef OPENXP_FONT_ENGINE_H
#define OPENXP_FONT_ENGINE_H
#include <stdint.h>
#include <stddef.h>
/* OXFT v1: version, cell geometry, style count, first codepoint,
 * glyph count, alpha-array size; then per-style byte advances and
 * one 16-column x 6-row glyph atlas per style (alpha8).  ASCII 32..126.
 * Everything is untrusted initrd data: no bare pointer arithmetic before
 * a validated fixed-size parse.  Shared by Ring 3 shell and native apps. */
#define OXP_FONT_CW 11u
#define OXP_FONT_CH 13u
#define OXP_FONT_FIRST 32u
#define OXP_FONT_COUNT 95u
#define OXP_FONT_STYLES 2u
#define OXP_FONT_COLS 16u
#define OXP_FONT_ROWS 6u
#define OXP_FONT_W (OXP_FONT_CW*OXP_FONT_COLS)
#define OXP_FONT_H (OXP_FONT_CH*OXP_FONT_ROWS*OXP_FONT_STYLES)
#define OXP_FONT_ALPHAS (OXP_FONT_W*OXP_FONT_H)
#define OXP_FONT_BYTES (16u + OXP_FONT_COUNT*OXP_FONT_STYLES + OXP_FONT_ALPHAS)
typedef struct {const uint8_t *advance,*alpha;} oxp_font_t;
static inline unsigned oxp_font_le16(const uint8_t *p){return (unsigned)p[0] | (unsigned)p[1]<<8;}
static inline unsigned oxp_font_le32(const uint8_t *p){return oxp_font_le16(p) | oxp_font_le16(p+2)<<16;}
static inline int oxp_font_parse(oxp_font_t *out,const uint8_t *data,size_t n){
    if(!out)return 0;
    out->advance=0;out->alpha=0;
    if(!data||n!=OXP_FONT_BYTES||data[0]!='O'||data[1]!='X'||data[2]!='F'||data[3]!='T'||
       data[4]!=1u ||data[5]!=OXP_FONT_CW ||data[6]!=OXP_FONT_CH ||
       data[7]!=OXP_FONT_STYLES ||oxp_font_le16(data+8)!=OXP_FONT_FIRST ||
       oxp_font_le16(data+10)!=OXP_FONT_COUNT ||oxp_font_le32(data+12)!=OXP_FONT_ALPHAS)return 0;
    for(unsigned i=0;i<OXP_FONT_COUNT*OXP_FONT_STYLES;++i)
        if(!data[16u+i]||data[16u+i]>OXP_FONT_CW)return 0;
    out->advance=data+16u;out->alpha=data+16u+OXP_FONT_COUNT*OXP_FONT_STYLES;
    return 1;
}
static inline unsigned oxp_font_index(unsigned char c){
    return c>=OXP_FONT_FIRST && c<OXP_FONT_FIRST+OXP_FONT_COUNT?
        (unsigned)c-OXP_FONT_FIRST:(unsigned)'?'-OXP_FONT_FIRST;
}
static inline unsigned oxp_font_advance(const oxp_font_t *f,unsigned char c,unsigned bold){
    if(!f||!f->advance)return 6u;
    return f->advance[(bold?OXP_FONT_COUNT:0u)+oxp_font_index(c)];
}
static inline unsigned oxp_font_width(const oxp_font_t *f,const char *s,unsigned bold){
    if(!s)return 0;
    unsigned total=0;
    for(unsigned n=0;n<256u && s[n];++n){
        unsigned w=oxp_font_advance(f,(unsigned char)s[n],bold);
        if(total>4096u-w)return 4096u;
        total+=w;
    }
    return total;
}
/* Pixel-accurate, allocation-free ellipsis planning. Never reads beyond the
 * caller-supplied byte cap (92 for native labels); both text and dots fit. */
typedef struct {unsigned chars,width,dots;} oxp_font_fit_t;
static inline oxp_font_fit_t oxp_font_fit(const oxp_font_t *f,const char *s,
                                         unsigned bold,unsigned pixels,unsigned cap){
    oxp_font_fit_t result={0,0,0};
    if(!s||!pixels||!cap)return result;
    if(cap>256u)cap=256u;
    unsigned i=0,width=0;
    while(i<cap && s[i]){
        unsigned step=oxp_font_advance(f,(unsigned char)s[i],bold);
        if(step>pixels-width)break;
        width+=step;++i;
    }
    if(i<cap && !s[i]){
        result.chars=i;result.width=width;return result;
    }
    unsigned dots_width=3u*oxp_font_advance(f,'.',bold);
    if(dots_width<=pixels){
        while(i && width>pixels-dots_width){
            --i;width-=oxp_font_advance(f,(unsigned char)s[i],bold);
        }
        result.dots=3u;
        width+=dots_width;
    }
    result.chars=i;result.width=width;
    return result;
}
static inline uint8_t oxp_font_alpha(const oxp_font_t *f,unsigned char c,
                                    unsigned bold,unsigned x,unsigned y){
    if(!f||!f->alpha||x>=OXP_FONT_CW||y>=OXP_FONT_CH)return 0u;
    unsigned index=oxp_font_index(c);
    unsigned gx=(index%OXP_FONT_COLS)*OXP_FONT_CW+x;
    unsigned gy=((index/OXP_FONT_COLS)+(bold?OXP_FONT_ROWS:0u))*OXP_FONT_CH+y;
    return f->alpha[gy*OXP_FONT_W+gx];
}
#endif
