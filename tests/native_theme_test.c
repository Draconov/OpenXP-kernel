#include "../apps/native/theme.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    assert(oxp_theme_valid(0) && oxp_theme_valid(2));
    assert(!oxp_theme_valid(3) && !oxp_theme_valid(0xffffffffu));
    for(unsigned i=0;i<3u;++i){
        assert(oxp_theme_wallpaper(i)<=0xffffff);
        assert(oxp_theme_taskbar(i)<=0xffffff);
        assert(oxp_theme_start(i)<=0xffffff);
        for(unsigned j=i+1;j<3u;++j)
            assert(oxp_theme_wallpaper(i)!=oxp_theme_wallpaper(j));
    }
    for(unsigned i=0;i<OXP_THEME_COUNT;++i){
        assert(oxp_theme_name(i)[0]);
        assert(oxp_theme_skin_path(i)[0]);
        assert(oxp_theme_widgets_path(i)[0]);
        assert(oxp_theme_icons_path(i)[0]);
        assert(oxp_theme_panel(i)!=0);
        assert(oxp_theme_accent(i)!=0);
        assert(oxp_theme_toolbar(i)!=0);
    }
    assert(oxp_theme_name(1)[0]=='O' && oxp_theme_name(2)[0]=='M');
    puts("PASS: bounded three-theme palette and resource routing");
}
