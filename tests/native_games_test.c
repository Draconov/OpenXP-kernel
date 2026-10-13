#include "../apps/native/game_logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    assert(oxp_leap(2000)&&!oxp_leap(1900)&&oxp_leap(2024)&&!oxp_leap(2025));
    assert(oxp_days(2024,2)==29&&oxp_days(2025,2)==28);
    assert(oxp_weekday(2026,10,13)==1); /* Tuesday */
    assert(oxp_weekday(2000,1,1)==5); /* Saturday */
    oxp_mines_t m;oxp_mines_reset(&m,53);
    assert(!m.started&&!m.revealed_count);
    oxp_mines_flag(&m,0);assert(m.flagged[0]&&m.flags==1);
    oxp_mines_flag(&m,0);assert(!m.flagged[0]&&!m.flags);
    oxp_mines_reveal(&m,40);
    assert(m.started && !m.mines[40] && m.revealed[40] && !m.ended);
    unsigned count=0;
    for(unsigned i=0;i<81;++i)if(m.mines[i]){++count;assert(!oxp_neighbours(i,40));}
    assert(count==10);
    oxp_mines_flag(&m,40);assert(m.flags==0);
    oxp_mines_reveal(&m,81);assert(m.revealed_count<=71);
    oxp_snake_t s;oxp_snake_reset(&s,9);
    assert(s.length==3 && s.dir==1 && !s.dead && !s.started);
    oxp_snake_turn(&s,3);assert(s.dir==1); /* no instant reversal */
    s.started=1;s.food=0;
    oxp_snake_step(&s);assert(s.body[0]==5*20+11 && s.length==3);
    oxp_snake_turn(&s,2);oxp_snake_step(&s);
    assert(s.body[0]==6*20+11);
    oxp_snake_reset(&s,1);s.started=1;
    for(unsigned n=0;n<30 && !s.dead;++n){s.food=0;oxp_snake_step(&s);}
    assert(s.dead);
    puts("PASS: native Mines/Snake/Calendar game logic");
}
