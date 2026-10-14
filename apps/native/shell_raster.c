#include "shell_raster.h"
#include <stdint.h>
#include <stddef.h>
/* Original 5x7 OpenXP glyphs, rendered in Ring 3. This library does not
 * touch hardware or the kernel framebuffer. */
static const uint8_t font[37][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14},{0,0,0,0,0,0,0}
};static const uint8_t symbols[][7] = {
    {0,4,4,4,4,0,4},  /* ! */
    {0,0,0,31,0,0,0}, /* - */
    {0,0,0,0,0,12,12},/* . */
    {0,0,0,0,0,12,8}, /* , */
    {0,4,0,0,4,0,0}, /* : */
    {0,0,1,2,4,8,16},/* / */
    {16,8,4,2,1,0,0},/* backslash */
    {14,17,1,2,4,0,4},/* ? */
    {31,16,16,16,16,16,31},/* [ */
    {31,1,1,1,1,1,31},/* ] */
    {0,0,0,0,0,0,31},/* _ */
    {0,0,31,0,31,0,0},/* = */
    {0,0,4,31,4,0,0},/* + */
    {0,0,0,0,0,0,0},/* space */
    {2,4,8,8,8,4,2},/* ( */
    {8,4,2,2,2,4,8}, /* ) */
    {0,21,14,31,14,21,0}, /* * */
    {4,4,8,0,0,0,0}, /* apostrophe */
    {10,10,0,0,0,0,0}, /* double quote */
    {10,31,10,31,10,0,0}, /* # */
    {4,15,20,14,5,30,4}, /* $ */
    {17,2,4,8,17,0,0}, /* % */
    {12,18,20,8,21,18,13}, /* & */
    {14,17,23,21,23,16,14}, /* @ */
    {0,0,4,0,4,4,8}, /* ; */
    {2,4,8,16,8,4,2}, /* < */
    {8,4,2,1,2,4,8}, /* > */
    {3,4,4,8,4,4,3}, /* { */
    {24,4,4,2,4,4,24}, /* } */
    {4,10,17,0,0,0,0}, /* ^ */
    {0,0,9,22,0,0,0}, /* ~ */
    {4,4,4,4,4,4,4}, /* | */
    {8,4,2,0,0,0,0} /* backtick */
};
static const uint8_t *glyph(char c) {
    if (c >= 'a' && c <= 'z') c=(char)(c-'a'+'A');
    if (c >= 'A' && c <= 'Z') return font[c-'A'];
    if (c >= '0' && c <= '9') return font[c-'0'+26];
    switch(c) {
    case '!': return symbols[0]; case '-':return symbols[1];
    case '.':return symbols[2];case ',':return symbols[3];
    case ':':return symbols[4];case '/':return symbols[5];
    case '\\':return symbols[6];case '?':return symbols[7];
    case '[':return symbols[8];case ']':return symbols[9];
    case '_':return symbols[10];case '=':return symbols[11];
    case '+':return symbols[12];case '(':return symbols[14];
    case ')':return symbols[15];case '*':return symbols[16];
    case '\'':return symbols[17];case '"':return symbols[18];
    case '#':return symbols[19];case '$':return symbols[20];
    case '%':return symbols[21];case '&':return symbols[22];
    case '@':return symbols[23];case ';':return symbols[24];
    case '<':return symbols[25];case '>':return symbols[26];
    case '{':return symbols[27];case '}':return symbols[28];
    case '^':return symbols[29];case '~':return symbols[30];
    case '|':return symbols[31];case '`':return symbols[32];
    default:return symbols[13];
    }
}

static uint16_t pack565(uint32_t c){
    return (uint16_t)((((c>>19)&31u)<<11)|(((c>>10)&63u)<<5)|((c>>3)&31u));
}
static void draw_span(uint16_t *dst,unsigned a,unsigned b,uint16_t color){
    for(unsigned x=a;x<b;++x)dst[x]=color;
}
static void oxp_raster_core(const oxp_shell_draw_t *commands,size_t n,unsigned layer,
                    unsigned y,uint16_t *pixels,int overlay){
    if(!pixels || layer>1u || y>=OXP_SHELL_RASTER_H)return;
    uint16_t empty=layer?OXP_SHELL_TRANSPARENT:0;
    if(!overlay)draw_span(pixels,0,OXP_SHELL_RASTER_W,empty);
    if(!commands)return;
    for(size_t i=0;i<n;++i){
        const oxp_shell_draw_t *c=&commands[i];
        int bg=(c->op==OXP_SHELL_BG_RECT || c->op==OXP_SHELL_BG_TEXT);
        if((unsigned)!bg!=layer)continue;
        if(overlay && c->op==OXP_SHELL_BG_RECT && c->x==0 &&
           c->w>=OXP_SHELL_RASTER_W)continue;
        uint16_t col=pack565(c->color);
        if(c->op==OXP_SHELL_RECT || c->op==OXP_SHELL_BG_RECT){
            if(y<c->y || y-c->y>=c->h || c->x>=OXP_SHELL_RASTER_W)continue;
            unsigned end=c->x+c->w;
            if(end>OXP_SHELL_RASTER_W)end=OXP_SHELL_RASTER_W;
            draw_span(pixels,c->x,end,col);
        }else if(c->op==OXP_SHELL_TEXT || c->op==OXP_SHELL_BG_TEXT){
            unsigned scale=c->w;
            if(!scale || scale>2 || y<c->y)continue;
            unsigned row=(y-c->y)/scale;
            if(row>=7u)continue;
            for(unsigned letter=0;letter<OXP_SHELL_TEXT_MAX && c->text[letter];++letter){
                uint32_t pos=c->x+letter*6u*scale;
                if(pos>=OXP_SHELL_RASTER_W)break;
                uint8_t bits=glyph(c->text[letter])[row];
                for(unsigned bit=0;bit<5;++bit){
                    if(!(bits&(1u<<(4u-bit))))continue;
                    for(unsigned dx=0;dx<scale;++dx){
                        unsigned x=pos+bit*scale+dx;
                        if(x<OXP_SHELL_RASTER_W)pixels[x]=col;
                    }
                }
            }
        }
    }
}

void oxp_raster_row(const oxp_shell_draw_t *commands,size_t n,unsigned layer,
                    unsigned y,uint16_t *pixels){
    oxp_raster_core(commands,n,layer,y,pixels,0);
}
void oxp_raster_overlay_background(const oxp_shell_draw_t *commands,size_t n,
                                   unsigned y,uint16_t *pixels){
    oxp_raster_core(commands,n,0,y,pixels,1);
}
