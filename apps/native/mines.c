#include "oxp.h"
#include "game_logic.h"
static oxp_mines_t game;
static void render(void){
    oxp_draw_cmd_t batch[32];unsigned n=0;
    batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_CLEAR,0,0,0,0,0xf3f5f9};
    batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,12,38,153,126,0x8a9aaf};
    for(unsigned i=0;i<81;++i){
        unsigned x=14+(i%9)*17,y=40+(i/9)*13;
        unsigned show=game.revealed[i]||(game.ended&&game.mines[i]);
        batch[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,x,y,15,11,show?0xffffff:0xc6d5ea};
        if(n==32){oxp_draw(batch,n);n=0;}
    }
    if(n)oxp_draw(batch,n);
    oxp_label(14,10,"MINES",2,0x234e85);
    char label[24]="FLAGS ";size_t at=6;oxp_u64(label,&at,sizeof label,game.flags);
    oxp_label(194,10,label,1,0x234e85);
    for(unsigned i=0;i<81;++i){
        unsigned x=14+(i%9)*17,y=40+(i/9)*13;
        if(game.flagged[i]&&!game.revealed[i])oxp_label(x+4,y+3,"F",1,0xd14444);
        else if(game.ended&&game.mines[i])oxp_label(x+4,y+3,"*",1,0xc7212d);
        else if(game.revealed[i]){unsigned count=oxp_mines_number(&game,i);
            if(count){char num[]={ (char)('0'+count),0 };oxp_label(x+4,y+3,num,1,0x315a96);}
        }
    }
    oxp_label(182,68,"LEFT REVEAL",1,0x234e85);
    oxp_label(182,83,"RIGHT FLAG",1,0x234e85);
    oxp_label(182,98,"N NEW GAME",1,0x234e85);
    if(game.ended)oxp_label(182,136,game.won?"YOU WIN":"GAME OVER",1,game.won?0x27854e:0xc13030);
}
void oxp_main(void){
    oxp_mines_reset(&game,(uint32_t)oxp_ticks());
    if(oxp_open("Mines - native ELF64"))oxp_exit(1);
    for(;;){render();uint32_t e=oxp_wait(0);if(e==OXP_CLOSE||e==27)break;
        if(e=='n'||e=='N'){oxp_mines_reset(&game,(uint32_t)oxp_ticks());continue;}
        if((e&OXP_CLICK)||(e&OXP_RIGHT_CLICK)){
            unsigned x=oxp_click_x(e),y=oxp_click_y(e);
            if(x<16||y<43)continue;x-=16;y-=43;
            if(x>=14 && x<167 && y>=40 && y<157){
                unsigned c=(x-14)/17,r=(y-40)/13;
                if(c<9&&r<9){unsigned idx=r*9+c;
                    if(e&OXP_RIGHT_CLICK)oxp_mines_flag(&game,idx);
                    else oxp_mines_reveal(&game,idx);
                }
            }
        }
    }
    oxp_exit(0);
}
