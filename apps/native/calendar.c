#include "oxp.h"
#include "game_logic.h"
static unsigned year=2000,month=1,day=1,has_today,ty,tm,td;
static const char *const names[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
static void month_move(int change){
    if(change<0){if(month==1){if(year==1600)return;--year;month=12;}else --month;}
    else {if(month==12){if(year==9999)return;++year;month=1;}else ++month;}
    if(day>oxp_days(year,month))day=oxp_days(year,month);
}
static void render(void){
    oxp_draw_cmd_t shapes[32];unsigned n=0;
    shapes[n++]=(oxp_draw_cmd_t){OXP_DRAW_CLEAR,0,0,0,0,0xf3f8ff};
    shapes[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,8,8,38,22,0xc5defb};
    shapes[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,51,8,38,22,0xc5defb};
    shapes[n++]=(oxp_draw_cmd_t){OXP_DRAW_FILL,254,8,57,22,0xc5defb};
    oxp_draw(shapes,n);
    oxp_label(22,16,"<",1,0x17457e);oxp_label(66,16,">",1,0x17457e);
    oxp_label(264,16,"TODAY",1,0x17457e);
    oxp_label(102,10,names[month-1],2,0x124c8c);
    char digits[24]={0};size_t pos=0;oxp_u64(digits,&pos,sizeof digits,year);
    oxp_label(116,28,digits,1,0x124c8c);
    static const char *const wd[]={"M","T","W","T","F","S","S"};
    for(unsigned i=0;i<7;++i)oxp_label(26+i*42,48,wd[i],1,0x436384);
    unsigned first=oxp_weekday(year,month,1),days=oxp_days(year,month);
    for(unsigned d=1;d<=days;++d){
        unsigned index=first+d-1,x=8+(index%7)*44,y=62+(index/7)*19;
        uint32_t bg=d==day?0x4c86d8:has_today&&year==ty&&month==tm&&d==td?0xb7dbfe:0xffffff;
        oxp_fill(x,y,40,17,bg);
        char number[4]={0};size_t t=0;oxp_u64(number,&t,sizeof number,d);
        oxp_label(x+(d<10?15:11),y+5,number,1,d==day?0xffffff:0x1e4167);
    }
    oxp_label(12,169,has_today?"ARROWS MONTH/YEAR  T TODAY":"NO RTC DATE - BROWSE MONTHS",1,0x496a8e);
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
                else if(x>=254&&x<311&&has_today){year=ty;month=tm;day=td;}}
            else if(x>=8&&x<316&&y>=62&&y<176){
                unsigned c=(x-8)/44,r=(y-62)/19,idx=r*7+c;
                if(c<7 && idx>=oxp_weekday(year,month,1)){
                    unsigned selected=idx-oxp_weekday(year,month,1)+1;
                    if(selected<=oxp_days(year,month))day=selected;
                }
            }
        }
    }
    oxp_exit(0);
}
