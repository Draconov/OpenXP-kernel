#include "oxp.h"
#include "paint_codec.h"
/* Expanded to eight colors; files are RAM-backed and the format remains small. */
static uint8_t pixels[OXP_PAINT_SIZE];
static const uint32_t palette[]={0xffffff,0x204b8b,0xe85252,0x38a86e,
    0xf2ce5e,0x7348a8,0x222a3b,0x25aec5};
static unsigned ink=1;
static const char filename[]="home/painting.oxp";
static const char *status="1-8 COLORS  S SAVE O OPEN";
static void save(void){
    app_file_request_t req={0};oxp_copy(req.path,filename,sizeof req.path);
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
    oxp_fill(218,30,102,130,0xe9f2ff);
    for(unsigned i=0;i<8u;++i){
        unsigned x=224+(i/4u)*46u,y=35+(i%4u)*30u;
        oxp_fill(x,y,27,24,palette[i]);
        if(i==ink)oxp_label(x+29,y+8,"<",1,0x11345f);
    }
}
static void render(void){
    oxp_draw_cmd_t batch[32];unsigned n=0;
    batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_CLEAR,0,0,0,0,0xe9f2ff};
    batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,12,34,196,124,0x7b98ba};
    for(unsigned y=0;y<OXP_PAINT_H;++y)for(unsigned x=0;x<OXP_PAINT_W;++x){
        batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,14+x*3,36+y*3,3,3,
            palette[oxp_paint_get(pixels,x,y)]};
        if(n==32u){(void)oxp_draw(batch,n);n=0;}
    }
    if(n)(void)oxp_draw(batch,n);
    oxp_label(12,9,"PAINT 8 COLORS",1,0x1d4e82);
    draw_palette();
    oxp_label(14,163,status,1,0x1d4e82);
}
void oxp_main(void){
    if(oxp_open("Paint - native ELF64"))oxp_exit(1);
    render();
    for(;;){uint32_t e=oxp_wait(0);
        if(e==OXP_CLOSE||e==27)break;
        if(e==OXP_SAVE||e=='s'||e=='S'){save();render();}
        else if(e==OXP_OPEN||e=='o'||e=='O'){load();render();}
        else if(e>='1'&&e<='8'){ink=e-'1';draw_palette();}
        else if(e&OXP_CLICK){
            int chosen=0;
            for(unsigned i=0;i<8u;++i){
                unsigned x=224+(i/4u)*46u,y=35+(i%4u)*30u;
                if(oxp_hit(e,x,y,27,24)){ink=i;draw_palette();chosen=1;break;}
            }
            if(!chosen && oxp_hit(e,14,36,OXP_PAINT_W*3,OXP_PAINT_H*3)){
                unsigned x=(oxp_click_x(e)-30u)/3u,y=(oxp_click_y(e)-79u)/3u;
                if(x<OXP_PAINT_W&&y<OXP_PAINT_H){
                    oxp_paint_put(pixels,x,y,ink);
                    (void)oxp_fill(14+x*3,36+y*3,3,3,palette[ink]);
                }
            }
        }
    }oxp_exit(0);
}
