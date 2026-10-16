#include "../apps/native/ui_widgets.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    oxp_ui_rect rect={10u,20u,80u,25u};
    assert(oxp_ui_contains(rect,10u,20u));
    assert(oxp_ui_contains(rect,89u,44u));
    assert(!oxp_ui_contains(rect,90u,44u));
    assert(!oxp_ui_contains(rect,89u,45u));
    assert(!oxp_ui_contains((oxp_ui_rect){10u,20u,0u,1u},10u,20u));
    assert(oxp_ui_clamp(0u,10u,20u)==10u);
    assert(oxp_ui_clamp(15u,10u,20u)==15u);
    assert(oxp_ui_clamp(30u,10u,20u)==20u);
    assert(oxp_ui_hit(OXP_CLICK | ((rect.x+16u)<<10) | (rect.y+43u),rect));
    assert(!oxp_ui_hit(0u,rect));
    uint8_t hdr[16]={'O','X','U','A',1,0,192,0,128,0,0,0,0,32,1,0};
    /* 192*128*3 = 73728 = 0x12000. */
    assert(oxp_ui_header_ok(hdr,192u,128u));
    hdr[10]=1;assert(!oxp_ui_header_ok(hdr,192u,128u));hdr[10]=0;
    hdr[0]='B';assert(!oxp_ui_header_ok(hdr,192u,128u));
    assert(oxp_ui_axis(0,100,63,6,6)==0);
    assert(oxp_ui_axis(99,100,63,6,6)==62);
    assert(oxp_ui_axis(1,10,63,6,6)<63);
    puts("PASS: Aurora widget rectangle geometry, clamp, input hit-testing");
    return 0;
}
