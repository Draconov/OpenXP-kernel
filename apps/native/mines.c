#include "oxp.h"
#include "game_logic.h"
#include "ui_layout.h"
static oxp_mines_t game;
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    oxp_grid_layout_t g=oxp_mines_layout(w,h);
    oxp_draw_cmd_t batch[32];unsigned n=0;
    batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_CLEAR,0,0,0,0,0xf3f5f9};
    batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,g.x-2u,g.y-2u,g.tile*9u+4u,g.tile*9u+4u,0x8a9aaf};
    for(unsigned i=0;i<81;++i){
        unsigned x=g.x+(i%9u)*g.tile,y=g.y+(i/9u)*g.tile;
        unsigned show=game.revealed[i]||(game.ended&&game.mines[i]);
        batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,x,y,g.tile>1u?g.tile-1u:1u,
                                   g.tile>1u?g.tile-1u:1u,show?0xffffff:0xc6d5ea};
        if(n==32){oxp_draw(batch,n);n=0;}
    }
    if(n)oxp_draw(batch,n);
    oxp_label(14,10,"MINES",2,0x234e85);
    char label[24]="FLAGS ";size_t at=6;oxp_u64(label,&at,sizeof label,game.flags);
    oxp_label(w-90u,10,label,1,0x234e85);
    for(unsigned i=0;i<81;++i){
        unsigned x=g.x+(i%9u)*g.tile,y=g.y+(i/9u)*g.tile;
        if(g.tile<8u)continue;
        if(game.flagged[i]&&!game.revealed[i])oxp_label(x+g.tile/2u-3u,y+2u,"F",1,0xd14444);
        else if(game.ended&&game.mines[i])oxp_label(x+g.tile/2u-3u,y+2u,"*",1,0xc7212d);
        else if(game.revealed[i]){unsigned count=oxp_mines_number(&game,i);
            if(count){char num[]={ (char)('0'+count),0 };
                oxp_label(x+g.tile/2u-3u,y+2u,num,1,0x315a96);}
        }
    }
    if(h>g.y+9u*g.tile+14u){
        oxp_label(12,h-12u,game.ended?(game.won?"YOU WIN":"GAME OVER"):
                  "LEFT REVEAL   RIGHT FLAG   N NEW",1,0x234e85);
    }
}
void oxp_main(void){
    oxp_mines_reset(&game,(uint32_t)oxp_ticks());
    if(oxp_open("Mines - native ELF64"))oxp_exit(1);
    for(;;){render();uint32_t e=oxp_wait(0);if(e==OXP_CLOSE||e==27)break;
        if(e=='n'||e=='N'){oxp_mines_reset(&game,(uint32_t)oxp_ticks());continue;}
        if((e&OXP_CLICK)||(e&OXP_RIGHT_CLICK)){
            unsigned x=oxp_click_x(e),y=oxp_click_y(e);
            if(x<16||y<43)continue;x-=16;y-=43;
            oxp_grid_layout_t g=oxp_mines_layout(oxp_canvas_width(),oxp_canvas_height());
            if(x>=g.x && x<g.x+9u*g.tile && y>=g.y && y<g.y+9u*g.tile){
                unsigned c=(x-g.x)/g.tile,r=(y-g.y)/g.tile;
                if(c<9&&r<9){unsigned idx=r*9+c;
                    if(e&OXP_RIGHT_CLICK)oxp_mines_flag(&game,idx);
                    else oxp_mines_reveal(&game,idx);
                }
            }
        }
    }
    oxp_exit(0);
}
