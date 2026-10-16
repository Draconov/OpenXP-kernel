#include "../apps/native/scroll_layout.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    assert(oxp_scroll_limit(10,15)==0);
    assert(oxp_scroll_limit(30,7)==23);
    assert(oxp_scroll_clamp(99,30,7)==23);
    assert(oxp_scroll_step(0,-1,30,7)==0);
    assert(oxp_scroll_step(0,1,30,7)==1);
    assert(oxp_scroll_step(99,1,30,7)==23);
    assert(oxp_scroll_follow(0,16,8)==9);
    assert(oxp_scroll_follow(10,2,8)==2);
    assert(oxp_scroll_follow(10,12,8)==10);
    assert(oxp_scroll_thumb(0,10,15,100).h==100);
    assert(oxp_scroll_thumb(0,100,10,80).y==0);
    oxp_scroll_thumb_t end=oxp_scroll_thumb(90,100,10,80);
    assert(end.h>=10 && end.y+end.h==80);
    for(unsigned total=0;total<500;++total)
        for(unsigned visible=1;visible<80;++visible){
            oxp_scroll_thumb_t t=oxp_scroll_thumb(total,total,visible,37);
            assert(t.y+t.h<=37);
        }
    puts("PASS: bounded scroll math, caret following, thumb position and clamping");
    return 0;
}
