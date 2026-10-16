#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../apps/native/shell_icons.h"
#include "../apps/native/shell_layout.h"
#include "../kernel/user/shell_abi.h"
int main(void){
    static uint8_t atlas[OXP_SHELL_ICON_BYTES];
    oxp_shell_icons_t icons={0};
    assert(!oxp_shell_icons_parse(&icons,atlas,sizeof atlas));
    atlas[0]='O';atlas[1]='X';atlas[2]='U';atlas[3]='A';
    atlas[4]=1;atlas[6]=192;atlas[8]=64;
    unsigned count=OXP_SHELL_ICON_W*OXP_SHELL_ICON_H*3u;
    atlas[12]=(uint8_t)count;atlas[13]=(uint8_t)(count>>8u);
    atlas[14]=(uint8_t)(count>>16u);
    assert(!oxp_shell_icons_parse(&icons,atlas,sizeof atlas-1u));
    assert(oxp_shell_icons_parse(&icons,atlas,sizeof atlas));
    assert(icons.pixels==atlas+16);
    uint16_t pixels[128];
    for(unsigned i=0;i<128;++i)pixels[i]=0x07e0u;
    /* First pixel of atlas icon 0, fully opaque magenta. */
    atlas[16]=0x1f;atlas[17]=0xf8;atlas[18]=255;
    oxp_shell_icon_row(&icons,0,10,3,32,3,pixels,128);
    assert(pixels[10]==0xf81fu);
    assert(pixels[11]==0x07e0u);
    /* Alpha zero leaves the transparent sentinel as-is. */
    pixels[10]=OXP_SHELL_TRANSPARENT;atlas[18]=0;
    oxp_shell_icon_row(&icons,0,10,3,32,3,pixels,128);
    assert(pixels[10]==OXP_SHELL_TRANSPARENT);
    /* Blend half against black must neither equal black nor magenta. */
    pixels[10]=0;atlas[18]=128;
    oxp_shell_icon_row(&icons,0,10,3,32,3,pixels,128);
    assert(pixels[10]!=0u && pixels[10]!=0xf81fu);
    /* Clip drawing past the right border. */
    pixels[127]=0x1234;
    oxp_shell_icon_row(&icons,0,127,3,32,3,pixels,128);
    assert(pixels[126]==0x07e0u);
    oxp_shell_icon_row(&icons,12,0,0,32,0,pixels,128);
    assert(oxp_desktop_hit(OXP_DESKTOP_X+10,OXP_DESKTOP_TOP+1)==0);
    assert(oxp_desktop_hit(OXP_DESKTOP_X,OXP_DESKTOP_TOP+OXP_DESKTOP_SLOT_H)==-1);
    assert(oxp_desktop_hit(OXP_DESKTOP_X,OXP_DESKTOP_TOP+4u*OXP_DESKTOP_STEP)==4);
    assert(oxp_desktop_hit(OXP_DESKTOP_X,725)==-1);
    assert(oxp_menu_hit(OXP_MENU_ROW_X+4,OXP_MENU_ROW_Y+3)==0);
    assert(oxp_menu_hit(OXP_MENU_ROW_X+OXP_MENU_COLUMN_W+4,OXP_MENU_ROW_Y+6u*OXP_MENU_ROW_STEP)==13);
    assert(oxp_menu_hit(0,OXP_MENU_ROW_Y)==-1);
    assert(oxp_menu_hit(OXP_MENU_ROW_X+234u,OXP_MENU_ROW_Y)==-1);
    assert(oxp_desktop_app(0)==1u && oxp_desktop_app(4)==12u);
    assert(oxp_menu_icon(9)==8u);
    puts("PASS: Aurora 4 shell icons, alpha, clipping, hit targets");
    return 0;
}
