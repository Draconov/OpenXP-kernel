#include "oxp.h"
#include "game_logic.h"
static oxp_snake_t game;
static void render(void){
    oxp_draw_cmd_t cmds[32];unsigned n=0;
    cmds[n++]=(oxp_draw_cmd_t){OXP_DRAW_CLEAR,0,0,0,0,0xe8f2e2};
    cmds[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,9,33,300,130,0xc2d6b7};
    oxp_draw(cmds,n);
    oxp_label(10,12,"SNAKE",2,0x19542c);
    char score[28]="SCORE ";size_t a=6;oxp_u64(score,&a,sizeof score,game.score);
    oxp_label(153,17,score,1,0x19542c);
    oxp_fill(9+(game.food%20)*15,33+(game.food/20)*13,13,11,0xce3b42);
    for(unsigned k=0;k<game.length;++k){unsigned p=game.body[k];
        oxp_fill(9+(p%20)*15,33+(p/20)*13,13,11,k?0x438b4a:0x156b36);
    }
    if(game.dead)oxp_label(75,80,"GAME OVER  N RESTART",1,0x962d31);
    else if(game.won)oxp_label(98,80,"YOU WIN!",2,0x1a612f);
    else if(!game.started)oxp_label(70,80,"SPACE TO BEGIN",1,0x19542c);
    else if(game.paused)oxp_label(100,80,"PAUSED",2,0x19542c);
    oxp_label(10,169,"ARROWS MOVE  SPACE PAUSE  N NEW",1,0x19542c);
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
