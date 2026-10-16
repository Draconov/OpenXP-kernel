#include "oxp.h"
#include "paint_codec.h"
#include "paint_history.h"
#include "ui_layout.h"
#include "ui_widgets.h"
#include "advanced_input.h"
#include "file_picker.h"
/* Expanded to eight colors; files are RAM-backed and the format remains small. */
static uint8_t pixels[OXP_PAINT_SIZE];
static oxp_paint_history_t paint_history;
static int stroke_changed;
static const uint32_t palette[]={0xffffff,0x204b8b,0xe85252,0x38a86e,
    0xf2ce5e,0x7348a8,0x222a3b,0x25aec5};
static unsigned ink=1;
static char filename[96]="home/painting.oxp";
static oxp_picker_t file_picker;
static int pick_open_drawing(void){
    /* Per-process document path wins. Legacy mailbox remains a fallback for
     * older launchers; it is never consumed after receiving a launch arg. */
    if(oxp_launch_argument(filename,sizeof filename)>0)return 1;
    app_file_request_t r={0};
    oxp_copy(r.path,"home/open-paint.cfg",sizeof r.path);
    if(oxp_file(APP_FS_READ,&r)==0 && r.size>0 && r.size<sizeof filename){
        unsigned i=0;for(;i<r.size && r.data[i] && i+1<sizeof filename;++i)
            filename[i]=(char)r.data[i];
        filename[i]=0;
        (void)oxp_file(APP_FS_DELETE,&r);
        return 1;
    }
    return 0;
}

static const char *status="1-8 COLORS  S SAVE O OPEN";
static void save(void){
    app_file_request_t req={0};oxp_copy(req.path,filename,sizeof req.path);
    if(!(filename[0]=='h'&&filename[1]=='o'&&filename[2]=='m'&&
         filename[3]=='e'&&filename[4]=='/')){
        const char *leaf=filename;for(const char *scan=filename;*scan;++scan)if(*scan=='/')leaf=scan+1;
        char target[96]="home/";unsigned n=5;
        while(*leaf&&n+1<sizeof target)target[n++]=*leaf++;
        target[n]=0;oxp_copy(filename,target,sizeof filename);
        oxp_copy(req.path,filename,sizeof req.path);
    }
    req.size=OXP_PAINT_SIZE+4u;
    req.data[0]='O';req.data[1]='X';req.data[2]='P';req.data[3]='2';
    for(unsigned i=0;i<OXP_PAINT_SIZE;++i)req.data[i+4]=pixels[i];
    status=oxp_file(APP_FS_WRITE,&req)==0?"SAVED TO RAM":"SAVE FAILED";
}
static void load(void){
    app_file_request_t req={0};oxp_copy(req.path,filename,sizeof req.path);
    if(oxp_file(APP_FS_READ,&req)!=0 || req.size<4u ||
       req.data[0]!='O'||req.data[1]!='X'||req.data[2]!='P'){
        status="NO SAVED DRAWING";return;
    }
    if(req.data[3]=='2' && req.size==OXP_PAINT_SIZE+4u){
        for(unsigned i=0;i<OXP_PAINT_SIZE;++i)pixels[i]=req.data[i+4];
        status="DRAWING LOADED";
        oxp_paint_hist_init(&paint_history,pixels);
    }else if(req.data[3]=='1' && req.size==OXP_PAINT_W*OXP_PAINT_H/4u+4u){
        for(unsigned i=0;i<OXP_PAINT_SIZE;++i)pixels[i]=0;
        for(unsigned y=0;y<OXP_PAINT_H;++y)for(unsigned x=0;x<OXP_PAINT_W;++x){
            unsigned i=y*OXP_PAINT_W+x;
            oxp_paint_put(pixels,x,y,(req.data[4+i/4]>>((i%4u)*2u))&3u);
        }
        status="OLDER DRAWING IMPORTED";
        oxp_paint_hist_init(&paint_history,pixels);
    }else status="UNSUPPORTED DRAWING";
}
static void draw_palette(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    oxp_paint_layout_t l=oxp_paint_layout(w,h);
    unsigned panel_w=w-l.palette_x;
    oxp_fill(l.palette_x,29,panel_w,h>43u?h-40u:1u,oxp_theme_sidebar(oxp_ui_theme));
    for(unsigned i=0;i<8u;++i){
        oxp_ui_rect_t b=oxp_paint_palette_box(l,i);
        if(b.y+b.h>=h)continue;
        oxp_fill(b.x,b.y,b.w,b.h,palette[i]);
        if(i==ink && b.x+b.w+6u<w)oxp_label(b.x+b.w,b.y+4u,"<",1,oxp_ui_ink());
    }
}
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    oxp_paint_layout_t l=oxp_paint_layout(w,h);
    oxp_draw_cmd_t batch[32];unsigned n=0;
    batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_CLEAR,0,0,0,0,oxp_theme_panel(oxp_ui_theme)};
    batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,l.board.x-2u,l.board.y-2u,
        l.board.w+4u,l.board.h+4u,0x7b98ba};
    for(unsigned y=0;y<OXP_PAINT_H;++y)for(unsigned x=0;x<OXP_PAINT_W;++x){
        batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,l.board.x+x*l.pixel,
            l.board.y+y*l.pixel,l.pixel,l.pixel,palette[oxp_paint_get(pixels,x,y)]};
        if(n==32u){(void)oxp_draw(batch,n);n=0;}
    }
    if(n)(void)oxp_draw(batch,n);
    oxp_ui_appbar(w,"PAINT");
    if(w>224u){
        oxp_ui_toolbutton((oxp_ui_rect){w-113u,2u,52u,21u},"SAVE",0);
        oxp_ui_toolbutton((oxp_ui_rect){w-58u,2u,52u,21u},"OPEN",0);
    }
    draw_palette();
    if(h>=110u)oxp_ui_statusbar(w,h,status);
    if(w>330u)oxp_ui_toolbutton((oxp_ui_rect){w-190u,2u,72u,21u},"SAVE AS",0);
    oxp_picker_render(&file_picker);
}
/* Map local pointer coordinates to one logical 64x40 paint pixel. */
static int board_point(oxp_gesture_point_t p,oxp_paint_layout_t l,
                       unsigned *x,unsigned *y){
    if(!oxp_gesture_inside(p,l.board.x,l.board.y,l.board.w,l.board.h))return 0;
    *x=(p.x-l.board.x)/l.pixel;*y=(p.y-l.board.y)/l.pixel;
    return *x<OXP_PAINT_W&&*y<OXP_PAINT_H;
}
static void stroke_plot(unsigned x,unsigned y,void *unused){
    (void)unused;
    if(oxp_paint_get(pixels,x,y)==ink)return;
    oxp_paint_put(pixels,x,y,ink);stroke_changed=1;
    oxp_paint_layout_t l=oxp_paint_layout(oxp_canvas_width(),oxp_canvas_height());
    (void)oxp_fill(l.board.x+x*l.pixel,l.board.y+y*l.pixel,
                   l.pixel,l.pixel,palette[ink]);
}
static void toolbar_or_palette(oxp_gesture_point_t p){
    if(!p.valid)return;
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    if(w>224u && oxp_gesture_inside(p,w-113u,2u,52u,21u)){save();render();return;}
    if(w>224u && oxp_gesture_inside(p,w-58u,2u,52u,21u)){
        oxp_picker_begin(&file_picker,0,filename);render();return;
    }
    if(w>330u && oxp_gesture_inside(p,w-190u,2u,72u,21u)){
        oxp_picker_begin(&file_picker,1,filename);render();return;
    }
    oxp_paint_layout_t l=oxp_paint_layout(w,h);
    for(unsigned i=0;i<8u;++i){
        oxp_ui_rect_t b=oxp_paint_palette_box(l,i);
        if(b.y+b.h<h && oxp_gesture_inside(p,b.x,b.y,b.w,b.h)){
            ink=i;draw_palette();return;
        }
    }
}
void oxp_main(void){
    if(oxp_open("Paint - native ELF64"))oxp_exit(1);
    oxp_ui_atlas_init_widgets();
    if(pick_open_drawing())load();
    if(!paint_history.count)oxp_paint_hist_init(&paint_history,pixels);
    oxp_gesture_t pointer={0};
    unsigned last_x=0,last_y=0;
    int brushing=0;
    render();
    for(;;){uint32_t e=oxp_ui_wait_gesture();
        if(file_picker.active){
            int done=oxp_picker_event(&file_picker,e,OXP_FT_DRAWING);
            if(done==1){oxp_copy(filename,file_picker.picked,sizeof filename);
                if(file_picker.save)save();else load();}
            render();continue;
        }
        if(e==OXP_CLOSE||e==27)break;
        if(e==OXP_KEY_UNDO||e==OXP_KEY_REDO){
            if(oxp_paint_hist_step(&paint_history,e==OXP_KEY_UNDO?-1:1,pixels))
                status=e==OXP_KEY_UNDO?"STROKE UNDONE":"STROKE REDONE";
            render();continue;
        }
        if(e==OXP_SAVE||e=='s'||e=='S'){save();render();continue;}
        if(e==OXP_OPEN||e=='o'||e=='O'){oxp_picker_begin(&file_picker,0,filename);render();continue;}
        if(e>='1'&&e<='8'){ink=e-'1';draw_palette();continue;}
        if(e==OXP_RESIZE){
            if(stroke_changed){oxp_paint_hist_record(&paint_history,pixels);stroke_changed=0;}
            brushing=0;pointer.active=0;render();continue;
        }
        if(e&OXP_CLICK){toolbar_or_palette(oxp_gesture_point(e));continue;}
        unsigned kind=oxp_gesture_feed(&pointer,e);
        oxp_gesture_point_t p=oxp_gesture_point(e);
        oxp_paint_layout_t l=oxp_paint_layout(oxp_canvas_width(),oxp_canvas_height());
        unsigned x=0,y=0;
        int in_board=board_point(p,l,&x,&y);
        if(kind==OXP_GESTURE_DOWN){
            stroke_changed=0;
            brushing=in_board;
            if(brushing){last_x=x;last_y=y;stroke_plot(x,y,0);}
            render(); /* rebuild widget hit regions for release + focus styles */
        }else if(kind==OXP_GESTURE_MOVE){
            if(brushing && in_board){
                oxp_paint_stroke(last_x,last_y,x,y,stroke_plot,0);
                last_x=x;last_y=y;
            }else if(!in_board)brushing=0;
            /* A captured gesture re-entering the board resumes drawing. */
            else if(!brushing && board_point(pointer.start,l,&last_x,&last_y)){
                brushing=1;stroke_plot(x,y,0);last_x=x;last_y=y;
            }
        }else if(kind==OXP_GESTURE_UP){
            brushing=0;
            if(stroke_changed){oxp_paint_hist_record(&paint_history,pixels);stroke_changed=0;}
            if(oxp_gesture_click(&pointer,p,0u,0u,oxp_canvas_width(),
                                 oxp_canvas_height())){
                /* The target must match the original control, not just
                 * any point inside the client window. */
                unsigned w=oxp_canvas_width();
                if((w>224u&&oxp_gesture_click(&pointer,p,w-113u,2u,52u,21u)) ||
                   (w>224u&&oxp_gesture_click(&pointer,p,w-58u,2u,52u,21u)))
                    toolbar_or_palette(p);
                else for(unsigned i=0;i<8u;++i){
                    oxp_ui_rect_t b=oxp_paint_palette_box(l,i);
                    if(oxp_gesture_click(&pointer,p,b.x,b.y,b.w,b.h)){
                        toolbar_or_palette(p);break;
                    }
                }
            }
            render(); /* finish the stroke with a consistent final frame */
        }
    }
    oxp_exit(0);
}
