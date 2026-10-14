#include "oxp.h"
#include "game_logic.h"
static unsigned year=2000,month=1,day=1,has_today,ty,tm,td;
static const char *const names[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
static void month_move(int change){
    if(change<0){if(month==1){if(year==1600)return;--year;month=12;}else --month;}
    else {if(month==12){if(year==9999)return;++year;month=1;}else ++month;}
    if(day>oxp_days(year,month))day=oxp_days(year,month);
}
static unsigned today_x(void){return oxp_canvas_width()-66u;}
static unsigned grid_step(void){return (oxp_canvas_width()-16u)/7u;}
static unsigned grid_row(void){
    unsigned h=oxp_canvas_height();
    return h>83u?(h-81u)/6u:0u;
}
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    oxp_draw_cmd_t shapes[4];unsigned n=0;
    shapes[n++]=(oxp_draw_cmd_t){OXP_DRAW_CLEAR,0,0,0,0,0xf3f8ff};
    shapes[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,8,8,38,22,0xc5defb};
    shapes[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,51,8,38,22,0xc5defb};
    shapes[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,today_x(),8,57,22,0xc5defb};
    oxp_draw(shapes,n);
    oxp_label(22,16,"<",1,0x17457e);oxp_label(66,16,">",1,0x17457e);
    oxp_label(today_x()+10u,16,"TODAY",1,0x17457e);
    oxp_label(101,10,names[month-1],w<290u?1u:2u,0x124c8c);
    char digits[24]={0};size_t pos=0;oxp_u64(digits,&pos,sizeof digits,year);
    oxp_label(109,28,digits,1,0x124c8c);
    unsigned step=grid_step(),rowstep=grid_row();
    if(rowstep<10u){oxp_label(12,49,"RESIZE TO VIEW MONTH",1,0x496a8e);return;}
    static const char *const wd[]={"M","T","W","T","F","S","S"};
    for(unsigned i=0;i<7;++i)oxp_label(8u+i*step+step/2u-3u,49,wd[i],1,0x436384);
    unsigned first=oxp_weekday(year,month,1),days=oxp_days(year,month);
    for(unsigned d=1;d<=days;++d){
        unsigned index=first+d-1,x=8u+(index%7u)*step,y=62u+(index/7u)*rowstep;
        if(y+rowstep>h-10u)continue;
        uint32_t bg=d==day?0x4c86d8:has_today&&year==ty&&month==tm&&d==td?0xb7dbfe:0xffffff;
        oxp_fill(x,y,step-2u,rowstep-2u,bg);
        char number[4]={0};size_t t=0;oxp_u64(number,&t,sizeof number,d);
        oxp_label(x+step/2u-(d<10u?3u:6u),y+(rowstep-7u)/2u,number,1,
                  d==day?0xffffff:0x1e4167);
    }
    if(h>155u)oxp_label(12,h-11u,has_today?"ARROWS MONTH/YEAR  T TODAY":"NO RTC DATE",1,0x496a8e);
}
void oxp_main(void){
    uint64_t date=oxp_call(25,0,0);
    if(date){unsigned y=(unsigned)(date>>16),m=(unsigned)((date>>8)&255u),d=(unsigned)(date&255u);
        if(y>=1600&&y<=9999&&m>=1&&m<=12&&d>=1&&d<=oxp_days(y,m)){
            year=ty=y;month=tm=m;day=td=d;has_today=1;
        }
    }
    if(oxp_open("Calendar - native ELF64"))oxp_exit(1);
    for(;;){render();uint32_t e=oxp_wait(0);if(e==OXP_CLOSE||e==27)break;
        if(e==OXP_LEFT)month_move(-1);
        else if(e==OXP_RIGHT)month_move(1);
        else if(e==OXP_UP && year<9999){++year;if(day>oxp_days(year,month))day=oxp_days(year,month);}
        else if(e==OXP_DOWN && year>1600){--year;if(day>oxp_days(year,month))day=oxp_days(year,month);}
        else if((e=='t'||e=='T')&&has_today){year=ty;month=tm;day=td;}
        else if(e&OXP_CLICK){unsigned x=oxp_click_x(e),y=oxp_click_y(e);
            if(x<16||y<43)continue;x-=16;y-=43;
            if(y>=8&&y<30){if(x>=8&&x<46)month_move(-1);
                else if(x>=51&&x<89)month_move(1);
                else if(x>=today_x()&&x<today_x()+57u&&has_today){year=ty;month=tm;day=td;}}
            else if(grid_row()>=10u && x>=8&&x<oxp_canvas_width()-8u&&
                    y>=62&&y<oxp_canvas_height()-10u){
                unsigned c=(x-8)/grid_step(),r=(y-62)/grid_row(),idx=r*7+c;
                if(c<7 && idx>=oxp_weekday(year,month,1)){
                    unsigned selected=idx-oxp_weekday(year,month,1)+1;
                    if(selected<=oxp_days(year,month))day=selected;
                }
            }
        }
    }
    oxp_exit(0);
}
