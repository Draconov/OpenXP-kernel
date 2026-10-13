#include "oxp.h"
static const char keys[4][5]={"789/","456*","123-","C0=+"};
static void render(uint64_t value) {
    oxp_draw_cmd_t shapes[1+1+4*4];size_t at=0;
    shapes[at++]=(oxp_draw_cmd_t){OXP_DRAW_CLEAR,0,0,0,0,0xe9f1ff};
    shapes[at++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,12,29,296,42,0xffffff};
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
        shapes[at++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,12+c*77,78+r*24,69,20,
                                     (r==3||c==3)?0xb9d9ff:0xf8fbff};
    oxp_draw(shapes,at);
    oxp_label(18,10,"OPENXP CALCULATOR",1,0x17457e);
    char digits[32]={0};size_t n=0;
    oxp_u64(digits,&n,sizeof(digits),value);
    oxp_label(22,44,digits,2,0x142e48);
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){
        char ch[2]={keys[r][c],0};
        oxp_label(42+c*77,84+r*24,ch,1,0x17457e);
    }
}
static char mouse_key(uint32_t event){
    if(!(event&OXP_CLICK))return 0;
    unsigned x=oxp_click_x(event),y=oxp_click_y(event);
    if(x<28||y<121)return 0; /* relative content offset: x+16,y+43 */
    x-=28;y-=121;
    if(x>=4*77||y>=4*24)return 0;
    unsigned c=x/77,r=y/24;
    return (x%77<69 && y%24<20)?keys[r][c]:0;
}
void oxp_main(void){
    if(oxp_open("Calculator - native ELF64"))oxp_exit(1);
    uint64_t number=0,acc=0;char op=0;int reset=1;
    for(;;){
        render(number);
        uint32_t key=oxp_wait(0);
        if(!key)continue;
        if(key==OXP_CLOSE||key==27)break;
        char ch=(key&OXP_CLICK)?mouse_key(key):key<128?(char)key:0;
        if(ch>='0'&&ch<='9'){
            if(reset){number=0;reset=0;}
            if(number<UINT64_MAX/10-10)number=number*10+(unsigned)(ch-'0');
        }else if(ch=='c'||ch=='C'){number=acc=0;op=0;reset=1;}
        else if(ch=='+'||ch=='-'||ch=='*'||ch=='/'||ch=='='||ch==13||ch==10){
            if(op){
                if(op=='+')acc+=number;
                else if(op=='-')acc=acc>number?acc-number:0;
                else if(op=='*')acc*=number;
                else if(op=='/'&&number)acc/=number;
            }else acc=number;
            number=acc;reset=1;
            op=(ch=='='||ch==13||ch==10)?0:ch;
        }
    }
    oxp_exit(0);
}
