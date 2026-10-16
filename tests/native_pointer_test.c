#include "../apps/native/oxp.h"
#include "../apps/native/ui_pointer.h"
#include <assert.h>
#include <stdio.h>
#define EVT(kind,x,y) ((uint32_t)(kind)|(((uint32_t)(x)&1023u)<<10)|((uint32_t)(y)&1023u))
int main(void){
    oxp_pointer_model_t s;
    oxp_pointer_reset(&s);
    assert(s.hover==-1 && s.armed==-1 && s.focus==-1);
    assert(oxp_pointer_add(&s,(oxp_pointer_rect_t){10,12,70,20})==0);
    assert(oxp_pointer_add(&s,(oxp_pointer_rect_t){90,12,50,20})==1);
    assert(oxp_pointer_at(&s,26,55)==0); /* left/top window frame */
    assert(oxp_pointer_at(&s,106,55)==1);
    assert(oxp_pointer_at(&s,25,55)==-1);
    assert(oxp_pointer_at(&s,1023,1023)==-1);
    int changed=0;
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_MOVE,26,55),&changed)==-2);
    assert(changed && s.hover==0 && oxp_pointer_style(&s,0)==1);
    changed=0;
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_DOWN,30,57),&changed)==-2);
    assert(s.down && s.armed==0 && s.focus==0 && oxp_pointer_style(&s,0)==2);
    changed=0;
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_MOVE,106,55),&changed)==-2);
    assert(changed && s.hover==1 && oxp_pointer_style(&s,0)==1);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_UP,106,55),&changed)==-2);
    assert(!s.down && s.armed==-1); /* release over another control cancels */
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_UP,30,57),&changed)==-2);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_DOWN,30,57),&changed)==-2);
    assert(oxp_pointer_feed(&s,OXP_POINTER_LEAVE,&changed)==-2);
    assert(s.hover==-1);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_UP,1023,1023),&changed)==-2);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_DOWN,30,57),&changed)==-2);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_UP,31,58),&changed)==0);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_DOWN,250,140),&changed)==-2);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_UP,250,140),&changed)==-1);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_DOWN,250,140),&changed)==-2);
    assert(oxp_pointer_feed(&s,EVT(OXP_POINTER_UP,280,140),&changed)==-2);
    puts("PASS: native control hover, armed states, release, cancel, leave, generic content and coordinates");
    return 0;
}
