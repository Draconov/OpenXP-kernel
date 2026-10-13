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
    puts("PASS: bounded three-theme native desktop palette");
}
