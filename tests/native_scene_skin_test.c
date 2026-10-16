#include "../apps/native/native_scene.h"
#include "../apps/native/skin.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
static unsigned char blob[OXP_SKIN_MAX_BYTES];
static oxp_shell_window_t windows[OXP_NATIVE_SLOTS];
static oxp_native_info_t infos[OXP_NATIVE_SLOTS];
static uint16_t surfaces[OXP_NATIVE_SLOTS][OXP_NATIVE_W*OXP_NATIVE_H];
static uint16_t row_a[OXP_SHELL_RASTER_W],row_b[OXP_SHELL_RASTER_W],row_c[OXP_SHELL_RASTER_W];
int main(int argc,char **argv){
    assert(argc==2);
    FILE *f=fopen(argv[1],"rb");assert(f);
    size_t n=fread(blob,1,sizeof(blob),f);fclose(f);
    oxp_skin_t skin;assert(oxp_skin_parse(&skin,blob,n));
    windows[0]=(oxp_shell_window_t){.visible=1,.active=1,.x=30,.y=40,
        .width=470,.height=300};
    infos[0].active=1;
    memcpy(infos[0].title,"TEST WINDOW",12);
    /* The caption frame is the same code path consumed by windowd. */
    oxp_native_scene_row(windows,infos,surfaces,43,row_a);
    oxp_native_scene_row_skinned(windows,infos,surfaces,43,row_b,&skin,-1,-1,0);
    assert(memcmp(row_a,row_b,sizeof(row_a))!=0);
    assert(row_b[0]==OXP_SHELL_TRANSPARENT && row_b[900]==OXP_SHELL_TRANSPARENT);
    /* Inactive theme uses a distinct atlas, with identical layout. */
    windows[0].active=0;
    oxp_native_scene_row_skinned(windows,infos,surfaces,43,row_c,&skin,-1,-1,0);
    assert(memcmp(row_b,row_c,sizeof(row_b))!=0);
    windows[0].active=1;
    /* Hover/press paths must visibly change close button. */
    unsigned yy=55;
    oxp_native_scene_row_skinned(windows,infos,surfaces,yy,row_a,&skin,-1,-1,0);
    oxp_native_scene_row_skinned(windows,infos,surfaces,yy,row_b,&skin,480,56,0);
    oxp_native_scene_row_skinned(windows,infos,surfaces,yy,row_c,&skin,480,56,1);
    assert(memcmp(row_a,row_b,sizeof(row_a))!=0);
    assert(memcmp(row_b,row_c,sizeof(row_a))!=0);
    /* Same atlas renders on a maximized client without memory overrun. */
    windows[0].x=0;windows[0].y=0;windows[0].width=1024;
    windows[0].height=726;windows[0].maximized=1;
    oxp_native_scene_row_skinned(windows,infos,surfaces,720,row_a,&skin,-1,-1,0);
    assert(row_a[0]!=OXP_SHELL_TRANSPARENT);
    puts("PASS: native compositor uses skinned active/inactive chrome, button states and max window");
    return 0;
}
