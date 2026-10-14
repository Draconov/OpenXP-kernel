#ifndef OPENXP_NATIVE_UI_LAYOUT_H
#define OPENXP_NATIVE_UI_LAYOUT_H
/* No runtime dependency: apps render and hit-test from identical geometry.
 * All rectangles use native logical canvas coordinates, not screen pixels. */
typedef struct { unsigned x,y,w,h; } oxp_ui_rect_t;
typedef struct {
    oxp_ui_rect_t board;
    unsigned pixel, palette_x, palette_step, palette_columns;
} oxp_paint_layout_t;
static inline unsigned oxp_ui_min(unsigned a,unsigned b){return a<b?a:b;}
static inline oxp_paint_layout_t oxp_paint_layout(unsigned width,unsigned height){
    oxp_paint_layout_t l={0};
    unsigned usable_w=width>113u?width-113u:64u;
    unsigned usable_h=height>54u?height-54u:40u;
    l.pixel=oxp_ui_min(3u,oxp_ui_min(usable_w/64u,usable_h/40u));
    if(l.pixel<1u)l.pixel=1u;
    l.board=(oxp_ui_rect_t){12u,32u,64u*l.pixel,40u*l.pixel};
    l.palette_x=width<160u?width-28u:l.board.x+l.board.w+8u;
    l.palette_columns=width<160u?1u:2u;
    l.palette_step=width-l.palette_x>=100u?48u:38u;
    return l;
}
typedef struct {unsigned x,y,tile;} oxp_grid_layout_t;
static inline oxp_grid_layout_t oxp_mines_layout(unsigned w,unsigned h){
    unsigned tile=oxp_ui_min(17u,oxp_ui_min(w>20u?(w-20u)/9u:1u,
                                           h>46u?(h-46u)/9u:1u));
    if(tile<1u)tile=1u;
    return (oxp_grid_layout_t){(w-9u*tile)/2u,34u,tile};
}
static inline oxp_grid_layout_t oxp_snake_layout(unsigned w,unsigned h){
    unsigned tile=oxp_ui_min(15u,oxp_ui_min(w>18u?(w-18u)/20u:1u,
                                           h>50u?(h-50u)/10u:1u));
    if(tile<1u)tile=1u;
    return (oxp_grid_layout_t){(w-20u*tile)/2u,30u,tile};
}
static inline oxp_ui_rect_t oxp_paint_palette_box(oxp_paint_layout_t l,unsigned i){
    if(l.palette_columns==1u)
        return (oxp_ui_rect_t){l.palette_x+1u,26u+i*17u,24u,14u};
    return (oxp_ui_rect_t){l.palette_x+3u+(i/4u)*l.palette_step,
                           35u+(i%4u)*24u,26u,19u};
}
#endif

