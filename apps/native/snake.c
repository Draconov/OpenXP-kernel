#include "oxp.h"
#include "game_logic.h"
#include "ui_layout.h"
static oxp_snake_t game;
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    oxp_grid_layout_t l=oxp_snake_layout(w,h);
    oxp_draw_cmd_t cmds[]={
        {OXP_DRAW_CLEAR,0,0,0,0,0xe8f2e2},
        {OXP_DRAW_FILL,l.x,l.y,20u*l.tile,10u*l.tile,0xc2d6b7}};
    oxp_draw(cmds,2);
    oxp_label(10,10,"SNAKE",2,0x19542c);
    char score[28]="SCORE ";size_t a=6;oxp_u64(score,&a,sizeof score,game.score);
    oxp_label(w-87u,17,score,1,0x19542c);
    oxp_fill(l.x+(game.food%20u)*l.tile,l.y+(game.food/20u)*l.tile,
             l.tile>1u?l.tile-1u:1u,l.tile>1u?l.tile-1u:1u,0xce3b42);
    for(unsigned k=0;k<game.length;++k){unsigned p=game.body[k];
        oxp_fill(l.x+(p%20u)*l.tile,l.y+(p/20u)*l.tile,
                 l.tile>1u?l.tile-1u:1u,l.tile>1u?l.tile-1u:1u,
                 k?0x438b4a:0x156b36);
    }
    const char *message=game.dead?"GAME OVER  N RESTART":game.won?"YOU WIN!":
        !game.started?"SPACE TO BEGIN":game.paused?"PAUSED":NULL;
    if(message && l.tile>=5u)oxp_label(l.x+7u,l.y+4u*l.tile,message,1,0x19542c);
    if(h>l.y+10u*l.tile+9u)oxp_label(10,h-10u,"ARROWS MOVE  SPACE PAUSE  N NEW",1,0x19542c);
}
void oxp_main(void){
    oxp_snake_reset(&game,(uint32_t)oxp_ticks());
    if(oxp_open("Snake - native ELF64"))oxp_exit(1);
    uint64_t last=oxp_ticks();
    for(;;){
        render();
        uint32_t e=oxp_wait(8);if(e==OXP_CLOSE||e==27)break;
        if(e==OXP_UP)oxp_snake_turn(&game,0);
        else if(e==OXP_RIGHT)oxp_snake_turn(&game,1);
        else if(e==OXP_DOWN)oxp_snake_turn(&game,2);
        else if(e==OXP_LEFT)oxp_snake_turn(&game,3);
        else if(e=='n'||e=='N'){oxp_snake_reset(&game,(uint32_t)oxp_ticks());last=oxp_ticks();}
        else if(e==' '||e=='p'||e=='P'){
            if(game.dead||game.won)oxp_snake_reset(&game,(uint32_t)oxp_ticks());
            if(!game.started){game.started=1;game.paused=0;}
            else game.paused=!game.paused;
            last=oxp_ticks();
        }
        uint64_t now=oxp_ticks();if(now-last>=14u){last=now;oxp_snake_step(&game);}
    }
    oxp_exit(0);
}
