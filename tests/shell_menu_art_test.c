#include "../apps/native/shell_menu_art.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char data[OXP_MENU_ART_BYTES];
int main(int argc,char **argv){
    assert(argc==2);
    FILE *fp=fopen(argv[1],"rb");assert(fp);
    size_t n=fread(data,1,sizeof data,fp);assert(!ferror(fp));
    assert(fgetc(fp)==EOF);fclose(fp);
    oxp_menu_art_t atlas={0};
    assert(n==sizeof data);
    assert(oxp_menu_art_parse(&atlas,data,n));
    uint16_t row[1024];for(unsigned i=0;i<1024;++i)row[i]=0x1234u;
    assert(oxp_menu_art_row(&atlas,OXP_MENU_ART_PANEL,4,356,514,370,356,row,1024));
    assert(row[3]==0x1234u && row[32]!=0x1234u);
    assert(oxp_menu_art_row(&atlas,OXP_MENU_ART_SELECTED,13,410,234,37,435,row,1024));
    assert(oxp_menu_art_row(&atlas,OXP_MENU_ART_DESKTOP_LABEL,9,60,90,22,67,row,1024));
    assert(!oxp_menu_art_row(&atlas,6u,0,0,100,30,15,row,1024));
    assert(!oxp_menu_art_row(&atlas,0,0,0,100,30,30,row,1024));
    assert(oxp_menu_art_row(&atlas,0,1020,0,100,30,5,row,1024));
    assert(oxp_menu_art_row(&atlas,0,0,0,3,4,0,row,1024));
    unsigned char old=data[0];data[0]='X';
    assert(!oxp_menu_art_parse(&atlas,data,n));assert(!atlas.pixels);
    data[0]=old;
    assert(!oxp_menu_art_parse(&atlas,data,n-1));
    data[6]=0xff;assert(!oxp_menu_art_parse(&atlas,data,n));
    puts("PASS: Aurora 7 OXUA menu artwork parsing, clipping, 9-slice and fallback");
    return 0;
}
