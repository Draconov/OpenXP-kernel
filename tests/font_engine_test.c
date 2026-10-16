#include "../apps/native/font_engine.h"
#include "../apps/native/shell_raster.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t data[OXP_FONT_BYTES];
static uint16_t line[OXP_SHELL_RASTER_W+2];
int main(int argc,char **argv){
    assert(argc==2);
    FILE *fp=fopen(argv[1],"rb");assert(fp);
    assert(fread(data,1,sizeof data,fp)==sizeof data);
    assert(fgetc(fp)==EOF);fclose(fp);
    oxp_font_t f={0};
    assert(!oxp_font_parse(&f,data,0));
    assert(!oxp_font_parse(&f,data,sizeof data-1u));
    assert(oxp_font_parse(&f,data,sizeof data));
    assert(f.advance && f.alpha);
    assert(oxp_font_width(&f,"Hello 123!",0)>0);
    assert(oxp_font_width(&f,"iiii",0)<oxp_font_width(&f,"WWWW",0));
    assert(oxp_font_advance(&f,' ',0)==4u);
    assert(oxp_font_advance(&f,0xff,0)==oxp_font_advance(&f,'?',0));
    assert(oxp_font_alpha(&f,'A',0,0,OXP_FONT_CH)==0);
    unsigned nonzero=0;
    for(unsigned y=0;y<OXP_FONT_CH;++y)for(unsigned x=0;x<OXP_FONT_CW;++x)
        if(oxp_font_alpha(&f,'A',0,x,y))++nonzero;
    assert(nonzero>5u);
    /* Text compositor shares the exact font with native GUI bitmap labels. */
    oxp_raster_set_font(&f);
    assert(oxp_raster_text_width("iiii",1)<oxp_raster_text_width("WWWW",1));
    oxp_shell_draw_t cmd={.op=OXP_SHELL_TEXT,.x=12,.y=7,.w=1,.color=0xffffff,.text="Aa"};
    unsigned lit=0;
    for(unsigned y=7;y<7u+OXP_FONT_CH;++y){
        line[0]=0xaaaa;line[OXP_SHELL_RASTER_W+1]=0xbbbb;
        oxp_raster_row(&cmd,1,1,y,line+1);
        assert(line[0]==0xaaaa && line[OXP_SHELL_RASTER_W+1]==0xbbbb);
        for(unsigned x=0;x<OXP_SHELL_RASTER_W;++x)
            if(line[x+1]!=OXP_SHELL_TRANSPARENT)++lit;
    }
    assert(lit>20u);
    oxp_raster_set_font(0);
    assert(oxp_raster_text_width("AAA",1)==18u);
    /* Malformed control bytes and advances are rejected. */
    data[5]=0xff;
    assert(!oxp_font_parse(&f,data,sizeof data));
    data[5]=OXP_FONT_CW;
    data[16]=0;
    assert(!oxp_font_parse(&f,data,sizeof data));
    puts("PASS: OXFT validation, proportional text, alpha glyphs and shell fallback");
    return 0;
}
