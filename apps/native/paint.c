#include "oxp.h"
#include "paint_codec.h"
#include "ui_layout.h"
/* Expanded to eight colors; files are RAM-backed and the format remains small. */
static uint8_t pixels[OXP_PAINT_SIZE];
static const uint32_t palette[]={0xffffff,0x204b8b,0xe85252,0x38a86e,
    0xf2ce5e,0x7348a8,0x222a3b,0x25aec5};
static unsigned ink=1;
static char filename[96]="home/painting.oxp";
static int pick_open_drawing(void){
    app_file_request_t r={0};
    oxp_copy(r.path,"home/open-document.cfg",sizeof r.path);
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
    }else if(req.data[3]=='1' && req.size==OXP_PAINT_W*OXP_PAINT_H/4u+4u){
        for(unsigned i=0;i<OXP_PAINT_SIZE;++i)pixels[i]=0;
        for(unsigned y=0;y<OXP_PAINT_H;++y)for(unsigned x=0;x<OXP_PAINT_W;++x){
            unsigned i=y*OXP_PAINT_W+x;
            oxp_paint_put(pixels,x,y,(req.data[4+i/4]>>((i%4u)*2u))&3u);
        }
        status="OLDER DRAWING IMPORTED";
    }else status="UNSUPPORTED DRAWING";
}
static void draw_palette(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    oxp_paint_layout_t l=oxp_paint_layout(w,h);
    unsigned panel_w=w-l.palette_x;
    oxp_fill(l.palette_x,29,panel_w,h>43u?h-40u:1u,0xe9f2ff);
    for(unsigned i=0;i<8u;++i){
        oxp_ui_rect_t b=oxp_paint_palette_box(l,i);
        if(b.y+b.h>=h)continue;
        oxp_fill(b.x,b.y,b.w,b.h,palette[i]);
        if(i==ink && b.x+b.w+6u<w)oxp_label(b.x+b.w,b.y+4u,"<",1,0x11345f);
    }
}
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    oxp_paint_layout_t l=oxp_paint_layout(w,h);
    oxp_draw_cmd_t batch[32];unsigned n=0;
    batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_CLEAR,0,0,0,0,0xe9f2ff};
    batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,l.board.x-2u,l.board.y-2u,
        l.board.w+4u,l.board.h+4u,0x7b98ba};
    for(unsigned y=0;y<OXP_PAINT_H;++y)for(unsigned x=0;x<OXP_PAINT_W;++x){
        batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,l.board.x+x*l.pixel,
            l.board.y+y*l.pixel,l.pixel,l.pixel,palette[oxp_paint_get(pixels,x,y)]};
        if(n==32u){(void)oxp_draw(batch,n);n=0;}
    }
    if(n)(void)oxp_draw(batch,n);
    oxp_label(12,9,"PAINT 8 COLORS",1,0x1d4e82);
    draw_palette();
    if(h>=110u)oxp_label(12,h-12u,status,1,0x1d4e82);
}
void oxp_main(void){
    if(oxp_open("Paint - native ELF64"))oxp_exit(1);
    if(pick_open_drawing())load();
    render();
    for(;;){uint32_t e=oxp_wait(0);
        if(e==OXP_CLOSE||e==27)break;
        if(e==OXP_SAVE||e=='s'||e=='S'){save();render();}
        else if(e==OXP_OPEN||e=='o'||e=='O'){load();render();}
        else if(e>='1'&&e<='8'){ink=e-'1';draw_palette();}
        else if(e==OXP_RESIZE)render();
        else if(e&OXP_CLICK){
            unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
            oxp_paint_layout_t l=oxp_paint_layout(w,h);
            int chosen=0;
            for(unsigned i=0;i<8u;++i){
                oxp_ui_rect_t b=oxp_paint_palette_box(l,i);
                if(b.y+b.h<h && oxp_hit(e,b.x,b.y,b.w,b.h)){
                    ink=i;draw_palette();chosen=1;break;
                }
            }
            if(!chosen && oxp_hit(e,l.board.x,l.board.y,l.board.w,l.board.h)){
                unsigned x=(oxp_click_x(e)-16u-l.board.x)/l.pixel;
                unsigned y=(oxp_click_y(e)-43u-l.board.y)/l.pixel;
                if(x<OXP_PAINT_W&&y<OXP_PAINT_H){
                    oxp_paint_put(pixels,x,y,ink);
                    (void)oxp_fill(l.board.x+x*l.pixel,l.board.y+y*l.pixel,
                                   l.pixel,l.pixel,palette[ink]);
                }
            }
        }
    }oxp_exit(0);
}
