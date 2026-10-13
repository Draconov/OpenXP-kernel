#include "../apps/native/wm_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static wm_packet_t request(uint32_t op,uint32_t id){
    return (wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,.operation=op,.window_id=id};
}
static wm_packet_t send(wm_state_t *s,uint64_t owner,wm_packet_t r){
    wm_packet_t answer;wm_state_process(s,owner,&r,&answer);
    assert(answer.magic==WM_MAGIC&&answer.version==WM_VERSION);
    assert(answer.operation==(r.operation|WM_REPLY_FLAG));
    return answer;
}
int main(void){
    wm_state_t s;wm_state_init(&s);
    wm_packet_t a=request(WM_CREATE,0);
    a.x=16;a.y=32;a.width=240;a.height=140;
    memcpy(a.title,"Client A",9);
    wm_packet_t out=send(&s,11,a);
    assert(out.status==WM_OK&&out.window_id&&out.width==240);
    assert(!strcmp(out.title,"Client A"));
    uint32_t first=out.window_id;
    assert(wm_state_active_window(&s)==first);
    assert(send(&s,11,a).status==WM_NO_SLOTS);
    a=request(WM_QUERY,first);
    assert(send(&s,12,a).status==WM_NOT_FOUND);
    assert(send(&s,11,a).status==WM_OK);
    a=request(WM_MOVE,first);a.x=60;a.y=90;
    out=send(&s,11,a);assert(out.status==WM_OK&&out.x==60&&out.y==90);
    a.x=UINT32_MAX;assert(send(&s,11,a).status==WM_BAD_GEOMETRY);
    assert(send(&s,11,request(WM_QUERY,first)).x==60);
    a=request(WM_CREATE,0);a.x=30;a.y=32;a.width=300;a.height=190;
    out=send(&s,12,a);uint32_t second=out.window_id;
    assert(out.status==WM_OK && second!=first);
    assert(wm_state_active_window(&s)==second);
    assert(send(&s,11,request(WM_FOCUS,first)).status==WM_OK);
    assert(wm_state_active_window(&s)==first);
    assert(send(&s,12,request(WM_CLOSE,first)).status==WM_NOT_FOUND);
    assert(send(&s,11,request(WM_CLOSE,first)).status==WM_OK);
    assert(send(&s,11,request(WM_QUERY,first)).status==WM_NOT_FOUND);
    assert(wm_state_active_window(&s)==second);
    /* Reuse increments generation; old IDs cannot regain authority. */
    out=send(&s,11,a);
    assert(out.status==WM_OK&&out.window_id!=first);
    wm_state_disconnect(&s,11);
    assert(send(&s,11,request(WM_QUERY,out.window_id)).status==WM_NOT_FOUND);
    assert(wm_state_active_window(&s)==second);
    wm_state_disconnect(&s,12);assert(wm_state_active_window(&s)==0);
    a=request(WM_CREATE,0);a.width=1;a.height=100;
    assert(send(&s,11,a).status==WM_BAD_GEOMETRY);
    a.width=250;a.x=UINT32_MAX;
    assert(send(&s,11,a).status==WM_BAD_GEOMETRY);
    a.x=0;
    for(unsigned i=0;i<WM_MAX_WINDOWS;++i)assert(send(&s,11u+i,a).status==WM_OK);
    assert(send(&s,11+WM_MAX_WINDOWS,a).status==WM_NO_SLOTS);
    a.version=2;assert(send(&s,11,a).status==WM_BAD_REQUEST);
    a.version=WM_VERSION;a.magic=0;assert(send(&s,11,a).status==WM_BAD_REQUEST);
    assert(send(&s,11,request(WM_PING,0)).status==WM_OK);
    puts("wm_protocol_test: passed");
    return 0;
}
