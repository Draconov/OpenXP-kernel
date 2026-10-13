#ifndef OPENXP_NATIVE_GAME_LOGIC_H
#define OPENXP_NATIVE_GAME_LOGIC_H
#include <stdint.h>
/* Deterministic and host-testable game/calendar logic. No privileged calls. */
static inline unsigned oxp_leap(unsigned y){return y%4u==0u&&(y%100u!=0u||y%400u==0u);}
static inline unsigned oxp_days(unsigned y,unsigned m){
    static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return m>=1u&&m<=12u?days[m-1u]+(m==2u?oxp_leap(y):0u):0u;
}
/* 0=Monday, ... 6=Sunday. Gregorian calendar. */
static inline unsigned oxp_weekday(unsigned y,unsigned m,unsigned d){
    static const unsigned t[]={0,3,2,5,0,3,5,1,4,6,2,4};
    if(m<1||m>12||d<1||d>oxp_days(y,m))return 0;
    if(m<3)--y;
    return (y+y/4-y/100+y/400+t[m-1]+d+6)%7;
}
static inline uint32_t oxp_rand(uint32_t *s){
    uint32_t x=*s?*s:0x9e3779b9u;
    x^=x<<13;x^=x>>17;x^=x<<5;*s=x;return x;
}
#define OXP_MINES_W 9u
#define OXP_MINES_CELLS 81u
typedef struct {
    uint8_t mines[OXP_MINES_CELLS],revealed[OXP_MINES_CELLS],flagged[OXP_MINES_CELLS];
    uint32_t rng;
    unsigned started,ended,won,flags,revealed_count;
} oxp_mines_t;
static inline void oxp_mines_reset(oxp_mines_t *g,uint32_t seed){
    for(unsigned i=0;i<OXP_MINES_CELLS;++i)g->mines[i]=g->revealed[i]=g->flagged[i]=0;
    g->rng=seed?seed:1;g->started=g->ended=g->won=g->flags=g->revealed_count=0;
}
static inline unsigned oxp_neighbours(unsigned a,unsigned b){
    int dx=(int)(a%9)-(int)(b%9),dy=(int)(a/9)-(int)(b/9);
    return dx>=-1&&dx<=1&&dy>=-1&&dy<=1;
}
static inline unsigned oxp_mines_number(const oxp_mines_t *g,unsigned cell){
    unsigned total=0;
    for(unsigned i=0;i<OXP_MINES_CELLS;++i)if(g->mines[i]&&i!=cell&&oxp_neighbours(i,cell))++total;
    return total;
}
static inline void oxp_mines_reveal(oxp_mines_t *g,unsigned cell){
    if(cell>=81||g->ended||g->flagged[cell]||g->revealed[cell])return;
    if(!g->started){
        g->started=1;unsigned placed=0;
        while(placed<10){unsigned p=oxp_rand(&g->rng)%81;
            if(g->mines[p]||oxp_neighbours(p,cell))continue;
            g->mines[p]=1;++placed;
        }
    }
    if(g->mines[cell]){g->ended=1;return;}
    /* A bounded queue also flood-fills adjacent zero cells, never recurses. */
    unsigned q[81],head=0,tail=0;q[tail++]=cell;
    while(head<tail){unsigned p=q[head++];
        if(g->revealed[p]||g->flagged[p]||g->mines[p])continue;
        g->revealed[p]=1;++g->revealed_count;
        if(oxp_mines_number(g,p))continue;
        for(unsigned i=0;i<81;++i)if(!g->revealed[i]&&!g->flagged[i]&&
            oxp_neighbours(i,p)&&i!=p){
            unsigned found=0;for(unsigned j=head;j<tail;++j)if(q[j]==i){found=1;break;}
            if(!found&&tail<81)q[tail++]=i;
        }
    }
    if(g->revealed_count==71){g->won=1;g->ended=1;}
}
static inline void oxp_mines_flag(oxp_mines_t *g,unsigned cell){
    if(cell>=81||g->ended||g->revealed[cell])return;
    g->flagged[cell]^=1u;
    if(g->flagged[cell])++g->flags;else --g->flags;
}
#define OXP_SNAKE_W 20u
#define OXP_SNAKE_H 10u
#define OXP_SNAKE_CELLS 200u
typedef struct {
    uint16_t body[OXP_SNAKE_CELLS];unsigned length,food,dir,started,paused,dead,won,score;
    uint32_t rng;
} oxp_snake_t;
static inline void oxp_snake_food(oxp_snake_t *g){
    if(g->length==200){g->won=1;return;}
    unsigned begin=oxp_rand(&g->rng)%200;
    for(unsigned a=0;a<200;++a){unsigned p=(begin+a)%200,occupied=0;
        for(unsigned j=0;j<g->length;++j)if(g->body[j]==p){occupied=1;break;}
        if(!occupied){g->food=p;return;}
    }
}
static inline void oxp_snake_reset(oxp_snake_t *g,uint32_t seed){
    g->rng=seed?seed:1;g->body[0]=5*20+10;g->body[1]=5*20+9;g->body[2]=5*20+8;
    g->length=3;g->dir=1;g->score=g->started=g->paused=g->dead=g->won=0;
    oxp_snake_food(g);
}
static inline void oxp_snake_turn(oxp_snake_t *g,unsigned dir){
    if(dir<4&&dir!=(g->dir+2u)%4u)g->dir=dir;
}
static inline void oxp_snake_step(oxp_snake_t *g){
    if(!g->started||g->paused||g->dead||g->won)return;
    unsigned old=g->body[0],x=old%20,y=old/20;
    if(g->dir==0){if(!y){g->dead=1;return;}--y;}
    if(g->dir==1){if(x==19){g->dead=1;return;}++x;}
    if(g->dir==2){if(y==9){g->dead=1;return;}++y;}
    if(g->dir==3){if(!x){g->dead=1;return;}--x;}
    unsigned next=y*20+x,eat=next==g->food;
    for(unsigned i=0;i<g->length-(eat?0u:1u);++i)
        if(g->body[i]==next){g->dead=1;return;}
    if(eat&&g->length<200){++g->length;++g->score;}
    for(unsigned i=g->length-1;i>0;--i)g->body[i]=g->body[i-1];
    g->body[0]=(uint16_t)next;
    if(eat)oxp_snake_food(g);
}
#endif
