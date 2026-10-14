#include "../kernel/gui/native_viewport.h"
#include "../apps/native/ui_layout.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

static void verify(unsigned window_w,unsigned window_h){
    oxp_native_canvas_t c=oxp_native_canvas(window_w,window_h);
    oxp_native_viewport_t viewport=oxp_native_viewport(0,0,window_w,window_h);
    assert(c.w>0 && c.w<=OXP_NATIVE_W && c.h>0 && c.h<=OXP_NATIVE_H);
    assert(viewport.w==window_w-22u && viewport.h==window_h-49u);
    uint64_t lhs=(uint64_t)c.w*viewport.h,rhs=(uint64_t)c.h*viewport.w;
    uint64_t rounding=viewport.w>viewport.h?viewport.w:viewport.h;
    assert(lhs<=rhs+rounding && rhs<=lhs+rounding);
    oxp_paint_layout_t paint=oxp_paint_layout(c.w,c.h);
    assert(paint.pixel>=1 && paint.pixel<=3);
    if(c.w>=110u)assert(paint.board.x+paint.board.w<paint.palette_x);
    for(unsigned i=0;i<8u;++i){
        oxp_ui_rect_t swatch=oxp_paint_palette_box(paint,i);
        assert(swatch.x+swatch.w<=c.w);
        if(c.h>=145u)assert(swatch.y+swatch.h<=c.h);
    }
    if(c.w>=76u)assert(paint.board.x+paint.board.w<=c.w);
    if(c.h>=95u)assert(paint.board.y+paint.board.h<=c.h);
    oxp_grid_layout_t mines=oxp_mines_layout(c.w,c.h);
    oxp_grid_layout_t snake=oxp_snake_layout(c.w,c.h);
    assert(mines.x+9u*mines.tile<=c.w);
    assert(mines.y+9u*mines.tile<=c.h);
    assert(snake.x+20u*snake.tile<=c.w);
    assert(snake.y+10u*snake.tile<=c.h);
    uint32_t cx=0,cy=0;
    oxp_native_pointer_to_canvas(viewport.x,viewport.y,window_w,window_h,&cx,&cy);
    assert(cx==16u && cy==43u);
    oxp_native_pointer_to_canvas(viewport.x+viewport.w-1u,viewport.y+viewport.h-1u,
                                 window_w,window_h,&cx,&cy);
    assert(cx>=16u && cx<16u+c.w && cy>=43u && cy<43u+c.h);
    oxp_native_pointer_to_canvas(viewport.x-1u,viewport.y,window_w,window_h,&cx,&cy);
    assert(cx==1023u && cy==1023u);
}
int main(void){
    verify(240,190); /* native minimum */
    verify(280,200); /* narrow restored */
    verify(470,300); /* default */
    verify(640,480); /* large restored */
    verify(1024,726); /* maximized */
    verify(640,190); /* wide and short */
    verify(1024,190); /* extreme layout: important controls remain reachable */
    for(unsigned w=240;w<=1024;w+=37)
        for(unsigned h=190;h<=726;h+=31)verify(w,h);
    puts("PASS: adaptive native canvas/layout, palette/game fit and pointer coordinates");
}
