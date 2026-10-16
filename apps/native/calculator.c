#include "oxp.h"
#include "ui_widgets.h"
#include "calculator_layout.h"
/* Arithmetic remains intentionally unsigned and bounded as in the original. */
static const char keys[4][5]={"789/","456*","123-","C0=+"};
static char mouse_key(uint32_t event){
    if(!(event&OXP_CLICK))return 0;
    unsigned ex=oxp_click_x(event),ey=oxp_click_y(event);
    if(ex<16u||ey<43u)return 0;
    int i=oxp_calc_key_at(oxp_canvas_width(),oxp_canvas_height(),ex-16u,ey-43u);
    return i<0?0:keys[(unsigned)i/4u][(unsigned)i%4u];
}
static void render(uint64_t value){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height(),top=oxp_calc_top(h);
    oxp_ui_clear();oxp_ui_appbar(w,"CALCULATOR");
    if(w>34u&&top>31u){
        oxp_ui_sheet((oxp_ui_rect){10u,29u,w-20u,top-34u});
    }
    char digits[32]={0};size_t n=0;oxp_u64(digits,&n,sizeof digits,value);
    unsigned ty=top>59u?top-24u:top>38u?top-13u:27u;
    oxp_ui_text((oxp_ui_rect){16u,ty,w>32u?w-32u:1u,14u},digits,ty,
                oxp_ui_edit_ink(),0);
    for(unsigned r=0;r<4u;++r)for(unsigned c=0;c<4u;++c){
        oxp_calc_rect b=oxp_calc_key(w,h,c,r);
        if(!b.w||!b.h)continue;
        char label[2]={keys[r][c],0};
        oxp_ui_tone tone=keys[r][c]=='='?OXP_UI_PRIMARY:
                            keys[r][c]=='C'?OXP_UI_DANGER:OXP_UI_NORMAL;
        oxp_ui_button((oxp_ui_rect){b.x,b.y,b.w,b.h},label,tone,0);
    }
}
void oxp_main(void){
    if(oxp_open("Calculator - native ELF64"))oxp_exit(1);
    oxp_ui_atlas_init_widgets();
    uint64_t number=0,acc=0;char op=0;int reset=1;
    for(;;){
        render(number);
        uint32_t key=oxp_ui_wait_theme();
        if(key==OXP_CLOSE||key==27)break;
        char ch=(key&OXP_CLICK)?mouse_key(key):key<128u?(char)key:0;
        if(ch>='0'&&ch<='9'){
            if(reset){number=0;reset=0;}
            if(number<UINT64_MAX/10u-10u)number=number*10u+(unsigned)(ch-'0');
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
