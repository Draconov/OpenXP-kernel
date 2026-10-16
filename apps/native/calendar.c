#include "oxp.h"
#include "game_logic.h"
#include "ui_widgets.h"
static unsigned year=2000,month=1,day=1,has_today,ty,tm,td;
static const char *const names[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
static void month_move(int change){
    if(change<0){if(month==1){if(year==1600)return;--year;month=12;}else --month;}
    else {if(month==12){if(year==9999)return;++year;month=1;}else ++month;}
    if(day>oxp_days(year,month))day=oxp_days(year,month);
}
static unsigned today_x(void){unsigned w=oxp_canvas_width();return w>70u?w-66u:4u;}
static unsigned grid_step(void){return (oxp_canvas_width()-16u)/7u;}
static unsigned grid_row(void){
    unsigned h=oxp_canvas_height();return h>105u?(h-105u)/6u:0u;
}
static void jump_today(void){if(has_today){year=ty;month=tm;day=td;}}
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    oxp_ui_clear();oxp_ui_appbar(w,"CALENDAR");
    oxp_ui_button((oxp_ui_rect){8u,29u,38u,22u},"<",OXP_UI_NORMAL,0);
    oxp_ui_button((oxp_ui_rect){51u,29u,38u,22u},">",OXP_UI_NORMAL,0);
    oxp_ui_button((oxp_ui_rect){today_x(),29u,57u,22u},"TODAY",OXP_UI_PRIMARY,0);
    if(w>185u){
        oxp_label(100u,31u,names[month-1u],w>295u?2u:1u,oxp_ui_ink());
        char digits[24]={0};size_t pos=0;oxp_u64(digits,&pos,sizeof digits,year);
        oxp_label(105u,48u,digits,1u,oxp_ui_ink());
    }
    unsigned step=grid_step(),rowstep=grid_row();
    if(rowstep<10u){oxp_label(12,69,"RESIZE TO VIEW MONTH",1u,oxp_ui_ink());return;}
    static const char *const weekdays[]={"M","T","W","T","F","S","S"};
    for(unsigned i=0;i<7u;++i){
        unsigned x=8u+i*step;
        oxp_fill(x,61u,step>2u?step-2u:1u,16u,oxp_theme_toolbar(oxp_ui_theme));
        oxp_label(x+step/2u-3u,65u,weekdays[i],1u,oxp_ui_ink());
    }
    unsigned first=oxp_weekday(year,month,1u),days=oxp_days(year,month);
    for(unsigned d=1;d<=days;++d){
        unsigned index=first+d-1u,x=8u+(index%7u)*step,y=81u+(index/7u)*rowstep;
        if(y+rowstep>h-10u)continue;
        uint32_t bg=d==day?oxp_theme_accent(oxp_ui_theme):
           has_today&&year==ty&&month==tm&&d==td?oxp_theme_selection(oxp_ui_theme):0xffffff;
        oxp_fill(x,y,step>2u?step-2u:1u,rowstep>2u?rowstep-2u:1u,bg);
        char number[4]={0};size_t z=0;oxp_u64(number,&z,sizeof number,d);
        oxp_label(x+step/2u-(d<10u?3u:6u),y+(rowstep-7u)/2u,number,1u,
                  d==day?0xffffff:0x1e4167);
    }
    if(h>155u)oxp_ui_statusbar(w,h,has_today?"ARROWS: MONTH  T: TODAY":"NO RTC DATE");
}
void oxp_main(void){
    uint64_t date=oxp_call(25,0,0);
    if(date){unsigned y=(unsigned)(date>>16),m=(unsigned)((date>>8)&255u),d=(unsigned)(date&255u);
        if(y>=1600u&&y<=9999u&&m>=1u&&m<=12u&&d>=1u&&d<=oxp_days(y,m)){
            year=ty=y;month=tm=m;day=td=d;has_today=1;
        }
    }
    if(oxp_open("Calendar - native ELF64"))oxp_exit(1);
    oxp_ui_atlas_init_widgets();
    for(;;){
        render();uint32_t e=oxp_ui_wait_theme();if(e==OXP_CLOSE||e==27)break;
        if(e==OXP_LEFT)month_move(-1);
        else if(e==OXP_RIGHT)month_move(1);
        else if(e==OXP_UP&&year<9999u){++year;if(day>oxp_days(year,month))day=oxp_days(year,month);}
        else if(e==OXP_DOWN&&year>1600u){--year;if(day>oxp_days(year,month))day=oxp_days(year,month);}
        else if(e=='t'||e=='T')jump_today();
        else if(e&OXP_CLICK){
            if(oxp_hit(e,8u,29u,38u,22u)){month_move(-1);continue;}
            if(oxp_hit(e,51u,29u,38u,22u)){month_move(1);continue;}
            if(oxp_hit(e,today_x(),29u,57u,22u)){jump_today();continue;}
            unsigned x=oxp_click_x(e),y=oxp_click_y(e);
            if(x<16u||y<43u||grid_row()<10u)continue;
            x-=16u;y-=43u;
            if(x>=8u&&x<oxp_canvas_width()-8u&&y>=81u&&y<oxp_canvas_height()-10u){
                unsigned c=(x-8u)/grid_step(),r=(y-81u)/grid_row(),idx=r*7u+c;
                unsigned first=oxp_weekday(year,month,1u);
                if(c<7u && idx>=first){
                    unsigned selected=idx-first+1u;
                    if(selected<=oxp_days(year,month))day=selected;
                }
            }
        }
    }
    oxp_exit(0);
}
