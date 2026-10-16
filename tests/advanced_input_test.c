#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../apps/native/advanced_input.h"
#include "../apps/native/paint_codec.h"
#include "../apps/native/text_edit.h"
#include "../apps/native/text_layout.h"
#include "../apps/native/scroll_layout.h"
static uint32_t event(unsigned type,unsigned x,unsigned y){
    return type|(((x+16u)&1023u)<<10)|((y+43u)&1023u);
}
static uint8_t canvas[OXP_PAINT_SIZE];
static unsigned calls;
static void plot(unsigned x,unsigned y,void *unused){
    (void)unused;assert(x<OXP_PAINT_W&&y<OXP_PAINT_H);
    oxp_paint_put(canvas,x,y,5u);++calls;
}
int main(void){
    oxp_gesture_t g={0};
    uint32_t e=event(OXP_GESTURE_DOWN,30u,40u);
    assert(oxp_gesture_feed(&g,e)==OXP_GESTURE_DOWN);
    assert(g.active&&g.start.valid&&g.start.x==30u&&g.start.y==40u);
    assert(oxp_gesture_feed(&g,event(OXP_GESTURE_MOVE,80u,80u))==OXP_GESTURE_MOVE);
    assert(oxp_gesture_feed(&g,OXP_GESTURE_LEAVE)==0 && g.active);
    assert(oxp_gesture_feed(&g,OXP_GESTURE_UP|1023u|(1023u<<10))==OXP_GESTURE_UP);
    assert(!g.active && !oxp_gesture_point(OXP_GESTURE_UP|1023u|(1023u<<10)).valid);
    oxp_gesture_point_t a={30,40,1},b={50,40,1};
    g.start=a;assert(!oxp_gesture_click(&g,b,20u,30u,20u,20u));
    assert(oxp_gesture_click(&g,(oxp_gesture_point_t){35,45,1},20u,30u,20u,20u));
    assert(!oxp_gesture_click(&g,(oxp_gesture_point_t){35,45,0},20u,30u,20u,20u));
    /* Large pointer jumps leave a gap with discrete clicks; the new stroke
     * must connect the whole segment, including reversed and diagonal ones. */
    oxp_paint_stroke(0,0,63,39,plot,0);
    assert(oxp_paint_get(canvas,0,0)==5u&&oxp_paint_get(canvas,63,39)==5u);
    assert(calls>=64u);
    calls=0;oxp_paint_stroke(63,39,0,39,plot,0);
    assert(calls==64u);
    calls=0;oxp_paint_stroke(0,0,64,0,plot,0);assert(!calls);
    /* A selection replaces exactly [min(anchor,cursor),max(...)); caret and
     * NUL terminator must stay coherent after deletion/insertion. */
    char doc[32]="abcdef";size_t len=6u,cur=5u;
    assert(oxp_erase_span(doc,&len,&cur,2u));
    assert(len==3u&&cur==2u&&!strcmp(doc,"abf"));
    assert(oxp_insert(doc,&len,&cur,sizeof doc,'X'));
    assert(len==4u&&cur==3u&&!strcmp(doc,"abXf"));
    assert(oxp_text_offset_at("abc\ndef",7u,3u,2u,2u)==6u);
    assert(!oxp_erase_span(doc,&len,&cur,cur));
    assert(!oxp_erase_span(doc,&len,&cur,20u));
    assert(oxp_gesture_scroll_at(100u,50u,100u,20u,10u,30u)==15u);
    assert(oxp_gesture_scroll_at(0u,50u,100u,20u,10u,30u)==0u);
    assert(oxp_gesture_scroll_at(10000u,50u,100u,20u,10u,30u)==30u);
    assert(oxp_gesture_scroll_at(10000u,50u,10u,10u,0u,30u)==0u);
    puts("PASS: Aurora 12 captured pointer, brush strokes, selection, and scroll math");
    return 0;
}
