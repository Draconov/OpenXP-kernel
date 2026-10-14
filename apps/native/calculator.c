#include "oxp.h"
static const char keys[4][5]={"789/","456*","123-","C0=+"};
static unsigned keys_top(unsigned h){return h>=160u?76u:h>=100u?48u:31u;}
static unsigned key_step_x(unsigned w){return (w-24u)/4u;}
static unsigned key_step_y(unsigned h){unsigned top=keys_top(h);return h>top+5u?(h-top-4u)/4u:1u;}
static void render(uint64_t value) {
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    unsigned top=keys_top(h),sx=key_step_x(w),sy=key_step_y(h);
    oxp_draw_cmd_t shapes[18];size_t at=0;
    shapes[at++]=(oxp_draw_cmd_t){OXP_DRAW_CLEAR,0,0,0,0,0xe9f1ff};
    shapes[at++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,12,h>=160u?29u:22u,w-24,top-(h>=160u?29u:22u)-5u,0xffffff};
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
        shapes[at++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,12+c*sx,top+r*sy,sx>5u?sx-5u:1u,sy>3u?sy-3u:1u,
                                     (r==3||c==3)?0xb9d9ff:0xf8fbff};
    oxp_draw(shapes,at);
    oxp_label(18,8,"OPENXP CALCULATOR",1,0x17457e);
    char digits[32]={0};size_t n=0;
    oxp_u64(digits,&n,sizeof(digits),value);
    oxp_label(22,h>=160u?41u:23u,digits,h>=160u?2u:1u,0x142e48);
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){
        char ch[2]={keys[r][c],0};
        oxp_label(12+c*sx+sx/2u-3u,top+r*sy+(sy>8u?(sy-7u)/2u:0u),ch,1,0x17457e);
    }
}
static char mouse_key(uint32_t event){
    if(!(event&OXP_CLICK))return 0;
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    unsigned x=oxp_click_x(event),y=oxp_click_y(event);
    unsigned top=keys_top(h),sx=key_step_x(w),sy=key_step_y(h);
    if(x<28u||y<43u+top)return 0;
    x-=28u;y-=43u+top;
    if(x>=4u*sx||y>=4u*sy)return 0;
    unsigned c=x/sx,r=y/sy;
    return (x%sx<sx-5u && y%sy<sy-3u)?keys[r][c]:0;
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
