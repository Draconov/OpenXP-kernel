#include "../apps/native/wallpaper.h"
#include "../apps/native/filetypes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* 2x2 padded 24-bit BMP: bottom row red/green; top row blue/white. */
static unsigned char bmp[70];
static int read_chunk(void *ctx,uint32_t offset,void *out,uint32_t n){
    const unsigned char *p=ctx;
    if(offset>sizeof bmp||n>sizeof bmp-offset)return 0;
    memcpy(out,p+offset,n);return 1;
}
static void set32(unsigned pos,unsigned n){for(unsigned i=0;i<4;++i)bmp[pos+i]=(unsigned char)(n>>(8*i));}
static void set16(unsigned pos,unsigned n){bmp[pos]=(unsigned char)n;bmp[pos+1]=(unsigned char)(n>>8);}
int main(void){
    assert(oxp_filetype("windows/system32/notepad.exe",0)==OXP_FT_EXE);
    assert(oxp_filetype("windows/web/wallpaper/azure.BMP",0)==OXP_FT_BITMAP);
    assert(oxp_filetype("home/note.txt",0)==OXP_FT_TEXT);
    assert(oxp_filetype("home/picture.oxp",0)==OXP_FT_DRAWING);
    assert(oxp_filetype("docs/config.ini",0)==OXP_FT_CONFIG);
    assert(oxp_filetype("program files",1)==OXP_FT_DIR);
    assert(oxp_filetype_app(OXP_FT_TEXT) && oxp_filetype_app(OXP_FT_DRAWING));
    assert(!oxp_filetype_app(OXP_FT_EXE));
    bmp[0]='B';bmp[1]='M';set32(2,sizeof bmp);set32(10,54);
    set32(14,40);set32(18,2);set32(22,2);set16(26,1);set16(28,24);
    /* bottom row */
    bmp[54+2]=255;bmp[57+1]=255;
    /* top row */
    bmp[62]=255;bmp[65]=255;bmp[66]=255;bmp[67]=255;
    static uint16_t pixels[OXP_WALL_W*OXP_WALL_H];
    assert(oxp_wall_decode(read_chunk,bmp,pixels));
    assert(pixels[0]==oxp_wall_rgb565(0,0,255));
    assert(pixels[OXP_WALL_W-1]==oxp_wall_rgb565(255,255,255));
    assert(pixels[(OXP_WALL_H-1)*OXP_WALL_W]==oxp_wall_rgb565(255,0,0));
    assert(pixels[OXP_WALL_W*OXP_WALL_H-1]==oxp_wall_rgb565(0,255,0));
    bmp[28]=32;assert(!oxp_wall_decode(read_chunk,bmp,pixels));
    bmp[28]=24;set32(18,2048);assert(!oxp_wall_decode(read_chunk,bmp,pixels));
    set32(18,2);set32(2,69);assert(!oxp_wall_decode(read_chunk,bmp,pixels));
    puts("PASS: file associations and strict BMP decoding / scaling");
}
