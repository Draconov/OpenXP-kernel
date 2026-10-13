#include "gui/desktop.h"
#include "fs/initrd.h"
#include <stdint.h>
#include <stddef.h>
#ifndef OPENXP_WM_HOST_TEST
#include "serial.h"
#endif

extern const char *const desktop_titles[DESKTOP_WINDOWS];
static uint32_t rgb(desktop_t *d,uint8_t r,uint8_t g,uint8_t b){return fb_rgb(d->surface,r,g,b);}
static void fill(desktop_t *d,int x,int y,int w,int h,uint8_t r,uint8_t g,uint8_t b) {
    fb_rect(d->surface,x,y,w,h,rgb(d,r,g,b));
}
static void text(desktop_t *d,int x,int y,const char *str,int scale,uint8_t r,uint8_t g,uint8_t b) {
    fb_text(d->surface,x,y,str,scale,rgb(d,r,g,b));
}
static void truncate_text(desktop_t *d,int x,int y,const char *s,int width,int scale,uint32_t ink) {
    if(width>0) fb_text_limit(d->surface,x,y,s,width/(6*scale),scale,ink);
}
static bool inside(int x,int y,int xx,int yy,int w,int h) {
    return x>=xx&&y>=yy&&(int64_t)x<(int64_t)xx+w&&(int64_t)y<(int64_t)yy+h;
}
static void console_output(void *ctx,const char *str) {
    desktop_t *d=ctx;
    if(!str)return;
    if(str[0]=='\f') {d->line_total=0;return;}
    char *row=d->output[d->line_total%56];
    size_t n=0;
    while(str[n]&&n<sizeof(d->output[0])-1) {row[n]=str[n];++n;}
    row[n]=0;
    d->line_total++;
#ifndef OPENXP_WM_HOST_TEST
    serial_puts(str);serial_puts("\n");
#endif
}
static void focus(desktop_t *d,int which) {
    int found=-1;
    for(int i=0;i<DESKTOP_WINDOWS;++i)if(d->order[i]==which)found=i;
    if(found<0)return;
    for(int i=found;i<DESKTOP_WINDOWS-1;++i)d->order[i]=d->order[i+1];
    d->order[DESKTOP_WINDOWS-1]=which;
}
int desktop_top_window(const desktop_t *d) {
    for(int i=DESKTOP_WINDOWS-1;i>=0;--i) {
        int idx=d->order[i];
        if(d->windows[idx].open&&!d->windows[idx].minimized)return idx;
    }
    return -1;
}
static void open_window(desktop_t *d,int which) {
    if(which<0||which>=DESKTOP_WINDOWS)return;
    d->windows[which].open=true;d->windows[which].minimized=false;
    if(which==DESKTOP_TERMINAL)d->shell.visible=true;
    d->showing_desktop=false;
    focus(d,which);
}
void desktop_init(desktop_t *d,fb_surface_t *s,shell_services_t services) {
    *d=(desktop_t){0};d->surface=s;
    d->windows[0]=(desktop_window_t){70,90,s->width>810?670:s->width-110,s->height>610?370:s->height-185,true,false};
    d->windows[1]=(desktop_window_t){110,100,s->width>810?540:s->width-130,s->height>610?330:s->height-185,false,false};
    d->windows[2]=(desktop_window_t){160,105,s->width>810?440:s->width-180,s->height>450?235:s->height-170,true,false};
    d->windows[3]=(desktop_window_t){130,130,500,330,false,false};
    d->windows[4]=(desktop_window_t){230,150,270,310,false,false};
    d->windows[5]=(desktop_window_t){120,58,380,356,false,false};
    d->windows[6]=(desktop_window_t){164,105,400,344,false,false};
    d->windows[7]=(desktop_window_t){198,100,368,285,false,false};
    d->windows[8]=(desktop_window_t){270,64,296,353,false,false};
    if(d->windows[3].w>s->width-25)d->windows[3].w=s->width-25;
    if(d->windows[4].w>s->width-25)d->windows[4].w=s->width-25;
    for(int i=0;i<DESKTOP_WINDOWS;++i){
        d->order[i]=i;
        if(d->windows[i].w>s->width-20)d->windows[i].w=s->width-20;
        if(d->windows[i].h>s->height-49)d->windows[i].h=s->height-49;
    }
    focus(d,DESKTOP_ABOUT);
    d->calc_new_entry=true;
    d->mouse_x=s->width/2;d->mouse_y=s->height/2;
    d->selected_file=-1;
    d->paint_color=1;
    shell_init(&d->shell,console_output,d,services);
}
static void draw_wallpaper(desktop_t *d) {
    fb_surface_t *s=d->surface;
    int ybegin=s->clip_active && s->clip_y>0 ? s->clip_y:0;
    int yend=s->height-42;
    if(s->clip_active && s->clip_y+s->clip_h<yend)yend=s->clip_y+s->clip_h;
    for(int y=ybegin;y<yend;++y) {
        unsigned shade=(unsigned)(y*80/(s->height>43?s->height-42:1));
        if(d->appearance==1)
            fill(d,0,y,s->width,1,(uint8_t)(75+shade/4),(uint8_t)(117+shade/3),(uint8_t)(61+shade/4));
        else if(d->appearance==2)
            fill(d,0,y,s->width,1,(uint8_t)(31+shade/4),(uint8_t)(40+shade/4),(uint8_t)(67+shade/3));
        else
            fill(d,0,y,s->width,1,(uint8_t)(12+shade/3),(uint8_t)(91+shade),(uint8_t)(174+shade/2));
    }
    int horizon=s->height*3/4;
    int from=horizon>ybegin?horizon:ybegin;
    for(int y=from;y<yend;++y) {
        int slope=(y-horizon)*5/4;
        if(d->appearance==2)fill(d,0,y,s->width,1,(uint8_t)(44+slope/12),(uint8_t)(53+slope/12),(uint8_t)(81+slope/10));
        else if(d->appearance==1)fill(d,0,y,s->width,1,(uint8_t)(102+slope/12),(uint8_t)(116+slope/10),(uint8_t)(51+slope/12));
        else fill(d,0,y,s->width,1,(uint8_t)(22+slope/9),(uint8_t)(115+slope/10),(uint8_t)(50+slope/20));
    }
    fill(d,0,s->height-43,s->width,2,83,145,241);
}
static void draw_icons(desktop_t *d) {
    static const char *names[]={"MY COMPUTER","DOCUMENTS","TERMINAL"};
    for(int i=0;i<3;++i) {
        int y=35+i*90;
        fill(d,25,y,32,30,239,244,255);
        fill(d,28,y+3,26,21,i==1?235:70,i==1?199:143,i==1?66:231);
        fill(d,30,y+26,22,3,37,64,93);
        text(d,16,y+39,names[i],1,255,255,255);
    }
}
static void draw_terminal(desktop_t *d,desktop_window_t *w) {
    int xx=w->x+5,yy=w->y+35,width=w->w-10,height=w->h-42;
    fill(d,xx,yy,width,height,13,22,31);
    int rows=(height-40)/12;
    if(rows<1)return;
    int avail=rows-1;
    unsigned start=d->line_total>(unsigned)avail?d->line_total-(unsigned)avail:0;
    for(unsigned n=start;n<d->line_total;++n) {
        int y=yy+8+(int)(n-start)*12;
        if(y+7>=yy+height-18)break;
        truncate_text(d,xx+9,y,d->output[n%56],width-19,1,rgb(d,198,243,208));
    }
    fill(d,xx+4,yy+height-28,width-8,24,19,45,45);
    text(d,xx+11,yy+height-20,"C:\\>",1,164,238,173);
    int columns=(width-51)/6;
    if(columns<1)columns=1;
    size_t first=0;
    if(d->shell.cursor>=(size_t)columns)first=d->shell.cursor-(size_t)columns+1;
    truncate_text(d,xx+35,yy+height-20,d->shell.input+first,width-51,1,rgb(d,243,255,244));
    int caret_x=xx+35+(int)(d->shell.cursor-first)*6;
    if(caret_x<xx+width-9)fill(d,caret_x,yy+height-12,5,2,240,251,241);
}
static void draw_files(desktop_t *d,desktop_window_t *w) {
    fill(d,w->x+5,w->y+34,w->w-10,w->h-40,255,255,255);
    fill(d,w->x+7,w->y+38,w->w-14,27,228,237,249);
    text(d,w->x+17,w->y+47,"C:\\RAMDISK",1,35,65,103);
    text(d,w->x+17,w->y+80,"NAME",1,24,67,116);
    text(d,w->x+17,w->y+97,"---------------------------------",1,151,169,194);
    size_t count=initrd_count();
    for(size_t i=0;i<count && i<15;++i) {
        char path[64];
        if(!initrd_name_at(i,path,sizeof(path)))continue;
        if(d->selected_file==(int)i)
            fill(d,w->x+13,w->y+110+(int)i*23,w->w-28,22,198,222,251);
        fill(d,w->x+17,w->y+114+(int)i*23,12,14,244,202,78);
        truncate_text(d,w->x+39,w->y+117+(int)i*23,path,w->w-62,1,rgb(d,45,59,91));
    }
    if(!count)text(d,w->x+17,w->y+120,"NO FILES",1,104,107,115);
    fill(d,w->x+5,w->y+w->h-40,w->w-10,36,228,237,249);
    fill(d,w->x+w->w-109,w->y+w->h-34,99,25,48,113,191);
    text(d,w->x+w->w-94,w->y+w->h-26,"RUN FILE",1,255,255,255);
    text(d,w->x+17,w->y+w->h-26,"READ ONLY",1,98,112,132);
}
static void draw_about(desktop_t *d,desktop_window_t *w) {
    fill(d,w->x+5,w->y+34,w->w-10,w->h-40,248,251,255);
    fill(d,w->x+22,w->y+52,76,75,30,98,190);
    text(d,w->x+31,w->y+77,"XP",3,255,255,255);
    text(d,w->x+115,w->y+59,"OPENXP",3,17,68,141);
    text(d,w->x+115,w->y+92,"EXPERIMENTAL BUILD",1,84,101,124);
    text(d,w->x+25,w->y+152,"EXPERIMENTAL X86 64 OPERATING SYSTEM",1,39,62,92);
    text(d,w->x+25,w->y+172,"ORIGINAL KERNEL AND GRAPHICAL SHELL",1,39,62,92);
    text(d,w->x+25,w->y+197,"NOT FOR PRODUCTION USE",1,159,48,50);
    text(d,w->x+25,w->y+218,"WIN: START | WIN+E: FILES | WIN+R: TERMINAL",1,39,62,92);
}
/* Notepad is intentionally RAM-only: there is no writable filesystem yet. */
static void draw_notepad(desktop_t *d,desktop_window_t *w) {
    int x=w->x+7,y=w->y+37,wide=w->w-14,high=w->h-65;
    fill(d,x,y,wide,high,255,255,255);
    fill(d,x,w->y+w->h-27,wide,20,226,233,245);
    text(d,x+7,w->y+w->h-21,"SCRATCHPAD - UNSAVED RAM",1,60,80,108);
    int cols=(wide-16)/6,rows=(high-15)/12;
    if(cols<1||rows<1)return;
    /* Find the caret's logical wrapped row without unbounded allocations. */
    int caret_row=0,caret_col=0;
    for(size_t i=0;i<d->note_cursor && i<d->note_length;++i) {
        if(d->note[i]=='\n'){++caret_row;caret_col=0;}
        else if(++caret_col>=cols){++caret_row;caret_col=0;}
    }
    int first=caret_row>=rows?caret_row-rows+1:0;
    char line[128];int line_used=0,row=0;
    size_t i=0;
    for(;;) {
        char c=i<d->note_length?d->note[i]:'\0';
        if(c=='\n'||c=='\0'||line_used>=cols||line_used==127) {
            line[line_used]=0;
            if(row>=first && row<first+rows)
                text(d,x+8,y+7+(row-first)*12,line,1,30,45,63);
            ++row;line_used=0;
            if(c=='\0')break;
            if(c=='\n'){++i;continue;}
            /* A full line is emitted before consuming this character. */
            continue;
        }
        line[line_used++]=c;++i;
    }
    if(caret_row>=first && caret_row<first+rows)
        fill(d,x+8+caret_col*6,y+7+(caret_row-first)*12+8,5,2,24,74,180);
}
static void uint_text(uint64_t value,char *buffer) {
    char reverse[24];int n=0,k=0;
    do{reverse[n++]=(char)('0'+value%10);value/=10;}while(value && n<23);
    while(n)buffer[k++]=reverse[--n];
    buffer[k]=0;
}
static void draw_calculator(desktop_t *d,desktop_window_t *w) {
    static const char *keys[4][4]={{"7","8","9","/"},{"4","5","6","*"},
                                    {"1","2","3","-"},{"C","0","=","+"}};
    fill(d,w->x+5,w->y+34,w->w-10,w->h-40,232,237,246);
    fill(d,w->x+13,w->y+46,w->w-26,37,255,255,255);
    fb_outline(d->surface,w->x+13,w->y+46,w->w-26,37,rgb(d,122,142,175));
    char result[25];uint_text(d->calc_entry,result);
    text(d,w->x+23,w->y+59,d->calc_error?"ERROR":result,2,35,61,85);
    int cell=(w->w-30)/4;
    for(int row=0;row<4;++row)for(int col=0;col<4;++col){
        int xx=w->x+13+col*cell,yy=w->y+96+row*46;
        fill(d,xx,yy,cell-5,40,col==3?94:255,col==3?142:255,col==3?201:255);
        fb_outline(d->surface,xx,yy,cell-5,40,rgb(d,118,140,174));
        text(d,xx+cell/2-5,yy+12,keys[row][col],2,col==3?255:28,col==3?255:57,col==3?255:95);
    }
    text(d,w->x+17,w->y+w->h-17,"KEYBOARD: 0-9 +-*/ ENTER",1,68,82,109);
}
/* Overflow is prohibited; the calculator is deliberately integer-only. */
static void calculator_press(desktop_t *d,char c) {
    if(c=='c'||c=='C'||c==27){
        d->calc_entry=0;d->calc_accumulator=0;d->calc_operator=0;
        d->calc_has_accumulator=false;d->calc_new_entry=true;d->calc_error=false;
        return;
    }
    if(d->calc_error)return;
    if(c>='0'&&c<='9'){
        if(d->calc_new_entry){d->calc_entry=0;d->calc_new_entry=false;}
        if(d->calc_entry<=99999999u)d->calc_entry=d->calc_entry*10u+(unsigned)(c-'0');
        return;
    }
    if(c!='+'&&c!='-'&&c!='*'&&c!='/'&&c!='='&&c!='\n')return;
    if(!d->calc_has_accumulator){d->calc_accumulator=d->calc_entry;d->calc_has_accumulator=true;}
    else if(!d->calc_new_entry && d->calc_operator){
        uint64_t a=d->calc_accumulator,b=d->calc_entry,value=0;
        if(d->calc_operator=='+')value=a+b;
        else if(d->calc_operator=='-'){if(b>a){d->calc_error=true;return;}value=a-b;}
        else if(d->calc_operator=='*')value=a*b;
        else if(d->calc_operator=='/') {if(!b){d->calc_error=true;return;}value=a/b;}
        if(value>999999999u){d->calc_error=true;return;}
        d->calc_accumulator=value;d->calc_entry=value;
    }
    d->calc_new_entry=true;
    if(c=='='||c=='\n'){d->calc_operator=0;d->calc_has_accumulator=false;}
    else d->calc_operator=c;
}
static void note_key(desktop_t *d,const input_event_t *e) {
    size_t at=d->note_cursor;
    if((e->modifiers&KEYMOD_CTRL) && (e->key=='n'||e->key=='N')){
        d->note_length=0;d->note_cursor=0;d->note[0]=0;return;
    }
    if(e->code==KEY_LEFT){if(at)--d->note_cursor;return;}
    if(e->code==KEY_RIGHT){if(at<d->note_length)++d->note_cursor;return;}
    if(e->code==KEY_HOME){d->note_cursor=0;return;}
    if(e->code==KEY_END){d->note_cursor=d->note_length;return;}
    if(e->code==KEY_BACKSPACE){if(!at)return;--at;--d->note_cursor;}
    else if(e->code==KEY_DELETE){if(at>=d->note_length)return;}
    else {
        char c=e->code==KEY_ENTER?'\n':e->key;
        if((e->modifiers&(KEYMOD_CTRL|KEYMOD_ALT|KEYMOD_WIN)) || (c!='\n' && (c<32||c>126)) ||
           d->note_length+1>=sizeof(d->note))return;
        for(size_t i=d->note_length+1;i>at;--i)d->note[i]=d->note[i-1];
        d->note[at]=c;++d->note_length;++d->note_cursor;return;
    }
    for(size_t i=at;i<d->note_length;++i)d->note[i]=d->note[i+1];
    --d->note_length;
}
/* Lightweight built-ins use only kernel-owned RAM until a writable filesystem exists. */
static const uint8_t paint_palette[8][3]={
    {255,255,255},{20,29,45},{216,63,63},{39,125,223},
    {52,158,91},{245,183,62},{146,82,193},{239,117,179}
};
static void paint_set(desktop_t *d,int x,int y) {
    if(x<0||x>=160||y<0||y>=100)return;
    uint8_t *p=&d->paint[y*160+x];
    if(*p==d->paint_color)return;
    *p=(uint8_t)d->paint_color;
    if(!d->paint_dirty){
        d->paint_min_x=d->paint_max_x=x;
        d->paint_min_y=d->paint_max_y=y;
        d->paint_dirty=true;
    } else {
        if(x<d->paint_min_x)d->paint_min_x=x;
        if(x>d->paint_max_x)d->paint_max_x=x;
        if(y<d->paint_min_y)d->paint_min_y=y;
        if(y>d->paint_max_y)d->paint_max_y=y;
    }
}
static void paint_line(desktop_t *d,int x0,int y0,int x1,int y1) {
    /* Bounded Bresenham strokes prevent gaps when PS/2 packets are batched. */
    int dx=x1>x0?x1-x0:x0-x1,dy=y1>y0?y1-y0:y0-y1;
    int sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx-dy;
    for(unsigned n=0;n<512;++n) {
        paint_set(d,x0,y0);
        if(x0==x1&&y0==y1)break;
        int twice=2*err;
        if(twice>-dy){err-=dy;x0+=sx;}
        if(twice<dx){err+=dx;y0+=sy;}
    }
}
static void paint_clear(desktop_t *d){
    for(unsigned i=0;i<160u*100u;++i)d->paint[i]=0;
    d->paint_dirty=true;
    d->paint_min_x=0;d->paint_min_y=0;
    d->paint_max_x=159;d->paint_max_y=99;
}
static void draw_paint(desktop_t *d,desktop_window_t *w) {
    int x=w->x+16,y=w->y+96;
    fill(d,w->x+5,w->y+34,w->w-10,w->h-40,237,243,252);
    text(d,w->x+16,w->y+41,"PALETTE",1,42,61,99);
    for(int i=0;i<8;++i) {
        int xx=w->x+16+i*38;
        fill(d,xx,w->y+54,29,24,paint_palette[i][0],paint_palette[i][1],paint_palette[i][2]);
        fb_outline(d->surface,xx-2,w->y+52,33,28,rgb(d,i==(int)d->paint_color?18:137,i==(int)d->paint_color?51:153,i==(int)d->paint_color?128:180));
    }
    fill(d,x-1,y-1,322,202,88,106,131);
    /* Collapse color runs; limit scanning to the clipped canvas region. */
    int first_row=0,last_row=100,first_col=0,last_col=160;
    fb_surface_t *s=d->surface;
    if(s->clip_active){
        if(s->clip_y>y)first_row=(s->clip_y-y)/2;
        int edge=s->clip_y+s->clip_h;
        if(edge<y+200)last_row=(edge-y+1)/2;
        if(s->clip_x>x)first_col=(s->clip_x-x)/2;
        edge=s->clip_x+s->clip_w;
        if(edge<x+320)last_col=(edge-x+1)/2;
        if(first_row<0)first_row=0;
        if(last_row>100)last_row=100;
        if(first_col<0)first_col=0;
        if(last_col>160)last_col=160;
    }
    for(int row=first_row;row<last_row;++row) {
        for(int col=first_col;col<last_col;) {
            unsigned ink=d->paint[row*160+col];int end=col+1;
            while(end<last_col && d->paint[row*160+end]==ink)++end;
            fill(d,x+col*2,y+row*2,(end-col)*2,2,paint_palette[ink][0],paint_palette[ink][1],paint_palette[ink][2]);
            col=end;
        }
    }
    fill(d,w->x+16,w->y+w->h-35,75,23,204,219,240);
    text(d,w->x+26,w->y+w->h-28,"CLEAR",1,32,62,101);
    text(d,w->x+108,w->y+w->h-28,"RAM-ONLY IMAGE",1,73,91,113);
}
static void draw_monitor(desktop_t *d,desktop_window_t *w){
    fill(d,w->x+5,w->y+34,w->w-10,w->h-40,250,251,254);
    text(d,w->x+17,w->y+48,"SYSTEM STATUS",2,35,76,142);
    char digits[25];
    uint_text(d->clock_ticks/100u,digits);
    text(d,w->x+18,w->y+94,"UPTIME (SECONDS):",1,64,76,95);
    text(d,w->x+176,w->y+94,digits,1,21,99,68);
    if(d->shell.services.free_pages){
        uint_text(d->shell.services.free_pages(d->shell.services.ctx),digits);
        text(d,w->x+18,w->y+116,"FREE 4K PAGES:",1,64,76,95);
        text(d,w->x+176,w->y+116,digits,1,21,99,68);
    } else text(d,w->x+18,w->y+116,"MEMORY STATS UNAVAILABLE",1,115,91,87);
    unsigned open=0;for(int i=0;i<DESKTOP_WINDOWS;++i)if(d->windows[i].open)++open;
    uint_text(open,digits);
    text(d,w->x+18,w->y+138,"OPEN WINDOWS:",1,64,76,95);
    text(d,w->x+176,w->y+138,digits,1,21,99,68);
    text(d,w->x+18,w->y+168,"DESKTOP APPLICATIONS",1,35,76,142);
    int row=0;
    for(int i=0;i<DESKTOP_WINDOWS;++i)if(d->windows[i].open){
        truncate_text(d,w->x+20,w->y+186+row*15,desktop_titles[i],w->w-116,1,rgb(d,47,62,88));
        text(d,w->x+w->w-104,w->y+186+row*15,d->windows[i].minimized?"MINIMIZED":"OPEN",1,50,111,88);
        ++row;
        if(row>=6)break;
    }
    text(d,w->x+18,w->y+w->h-24,"KERNEL-HOSTED UI; NOT A PROCESS LIST",1,142,78,62);
}
static void draw_settings(desktop_t *d,desktop_window_t *w){
    static const char *const names[]={"CLASSIC BLUE","OLIVE HILLS","NIGHT SKY"};
    static const uint8_t sample[3][3]={{26,108,204},{105,133,72},{39,56,91}};
    fill(d,w->x+5,w->y+34,w->w-10,w->h-40,246,249,253);
    text(d,w->x+17,w->y+49,"DESKTOP APPEARANCE",2,26,66,130);
    text(d,w->x+17,w->y+83,"SELECT A WALLPAPER THEME:",1,72,82,102);
    for(int i=0;i<3;++i){
        int y=w->y+105+i*44;
        fill(d,w->x+18,y,w->w-36,39,d->appearance==(unsigned)i?199:230,d->appearance==(unsigned)i?220:237,247);
        fill(d,w->x+26,y+7,37,25,sample[i][0],sample[i][1],sample[i][2]);
        text(d,w->x+79,y+15,names[i],1,41,68,103);
        if(d->appearance==(unsigned)i)text(d,w->x+w->w-41,y+15,"*",1,18,92,178);
    }
    text(d,w->x+17,w->y+w->h-27,"PRESS 1/2/3 OR CLICK A THEME",1,71,91,118);
}
static void mine_restart(desktop_t *d) {
    for(int i=0;i<81;++i){d->mine_values[i]=0;d->mine_cells[i]=0;}
    d->mine_flags=0;d->mine_revealed=0;d->mine_started=false;
    d->mine_lost=false;d->mine_won=false;++d->mine_games;
}
static void mine_generate(desktop_t *d,int safe){
    uint32_t seed=(uint32_t)(d->clock_ticks^(d->mine_games*0x9e3779b9u)^(unsigned)(safe*2654435761u));
    if(!seed)seed=0xa531c3u;
    unsigned count=0;
    while(count<10) {
        seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
        int pos=(int)(seed%81u);
        if(pos==safe||d->mine_values[pos]==-1)continue;
        d->mine_values[pos]=-1;++count;
    }
    for(int y=0;y<9;++y)for(int x=0;x<9;++x){
        int at=y*9+x;
        if(d->mine_values[at]==-1)continue;
        int n=0;
        for(int yy=y-1;yy<=y+1;++yy)for(int xx=x-1;xx<=x+1;++xx)
            if(xx>=0&&xx<9&&yy>=0&&yy<9&&d->mine_values[yy*9+xx]==-1)++n;
        d->mine_values[at]=(int8_t)n;
    }
    d->mine_started=true;
}
static void mine_reveal(desktop_t *d,int at){
    if(at<0||at>=81||d->mine_cells[at]||d->mine_won||d->mine_lost)return;
    if(!d->mine_started)mine_generate(d,at);
    if(d->mine_values[at]<0){d->mine_lost=true;d->mine_cells[at]=1;return;}
    /* Flood fill zero-cell neighbors with a bounded queue of 81 entries. */
    uint8_t queue[81];int head=0,tail=0;
    d->mine_cells[at]=1;++d->mine_revealed;queue[tail++]=(uint8_t)at;
    while(head<tail){
        int pos=queue[head++];
        if(d->mine_values[pos]!=0)continue;
        int x=pos%9,y=pos/9;
        for(int yy=y-1;yy<=y+1;++yy)for(int xx=x-1;xx<=x+1;++xx){
            if(xx<0||xx>=9||yy<0||yy>=9)continue;
            int next=yy*9+xx;
            if(d->mine_cells[next]||d->mine_values[next]<0)continue;
            d->mine_cells[next]=1;++d->mine_revealed;queue[tail++]=(uint8_t)next;
        }
    }
    if(d->mine_revealed==71)d->mine_won=true;
}
static void mine_toggle_flag(desktop_t *d,int at){
    if(at<0||at>=81||d->mine_cells[at]==1||d->mine_lost||d->mine_won)return;
    if(d->mine_cells[at]==2){d->mine_cells[at]=0;--d->mine_flags;}
    else if(d->mine_flags<10){d->mine_cells[at]=2;++d->mine_flags;}
}
static int mine_cell_at(const desktop_t *d,int mx,int my){
    const desktop_window_t *w=&d->windows[DESKTOP_MINES];
    int x=mx-(w->x+30),y=my-(w->y+92);
    if(x<0||y<0||x>=9*25||y>=9*25)return -1;
    return (y/25)*9+x/25;
}
static void draw_mines(desktop_t *d,desktop_window_t *w){
    fill(d,w->x+5,w->y+34,w->w-10,w->h-40,225,233,246);
    fill(d,w->x+24,w->y+43,245,35,244,248,253);
    char num[25];uint_text(10-d->mine_flags,num);
    text(d,w->x+34,w->y+55,"MINES LEFT",1,40,63,99);
    text(d,w->x+120,w->y+55,num,1,173,55,55);
    fill(d,w->x+200,w->y+47,60,25,187,216,249);
    text(d,w->x+212,w->y+56,"NEW",1,29,61,113);
    for(int y=0;y<9;++y)for(int x=0;x<9;++x){
        int pos=y*9+x,px=w->x+30+x*25,py=w->y+92+y*25;
        bool shown=d->mine_cells[pos]==1 || (d->mine_lost&&d->mine_values[pos]<0);
        fill(d,px,py,23,23,shown?242:165,shown?246:189,shown?251:222);
        fb_outline(d->surface,px,py,23,23,rgb(d,114,138,176));
        char label[2]={0,0};
        if(shown){
            if(d->mine_values[pos]<0)label[0]='*';
            else if(d->mine_values[pos]>0)label[0]=(char)('0'+d->mine_values[pos]);
        } else if(d->mine_cells[pos]==2)label[0]='F';
        if(label[0])text(d,px+9,py+8,label,1,label[0]=='*'?196:31,label[0]=='*'?43:83,label[0]=='*'?43:167);
    }
    text(d,w->x+29,w->y+w->h-23,d->mine_won?"YOU WIN!":d->mine_lost?"BOOM! PRESS N TO RESTART":"CLICK OPEN  /  HOVER + F TO FLAG",1,39,70,111);
}
const char *const desktop_titles[DESKTOP_WINDOWS]={"OpenXP Command Prompt","My Computer - RAMDISK","About OpenXP", "Notepad - Scratchpad", "Calculator", "Paint", "System Monitor", "Appearance Settings", "Mines"};
static void draw_window(desktop_t *d,int index,bool active) {
    desktop_window_t *w=&d->windows[index];
    if(!w->open||w->minimized)return;
    fill(d,w->x+4,w->y+5,w->w,w->h,18,64,90);
    fill(d,w->x,w->y,w->w,w->h,222,231,243);
    fb_outline(d->surface,w->x,w->y,w->w,w->h,rgb(d,42,81,143));
    fill(d,w->x+3,w->y+3,w->w-6,28,active?17:97,active?88:118,active?195:149);
    truncate_text(d,w->x+13,w->y+11,desktop_titles[index],w->w-92,1,rgb(d,255,255,255));
    fill(d,w->x+w->w-28,w->y+6,21,21,192,63,61);
    text(d,w->x+w->w-22,w->y+13,"X",1,255,255,255);
    fill(d,w->x+w->w-54,w->y+6,21,21,79,133,203);
    text(d,w->x+w->w-48,w->y+13,"-",1,255,255,255);
    if(index==DESKTOP_TERMINAL)draw_terminal(d,w);
    else if(index==DESKTOP_FILES)draw_files(d,w);
    else if(index==DESKTOP_ABOUT)draw_about(d,w);
    else if(index==DESKTOP_NOTEPAD)draw_notepad(d,w);
    else if(index==DESKTOP_CALCULATOR)draw_calculator(d,w);
    else if(index==DESKTOP_PAINT)draw_paint(d,w);
    else if(index==DESKTOP_MONITOR)draw_monitor(d,w);
    else if(index==DESKTOP_SETTINGS)draw_settings(d,w);
    else draw_mines(d,w);
}
static void draw_taskbar(desktop_t *d) {
    int h=d->surface->height,w=d->surface->width;
    fill(d,0,h-42,w,42,20,69,164);
    fill(d,0,h-42,w,3,98,156,243);
    fill(d,2,h-38,105,35,35,153,48);
    fill(d,5,h-37,100,32,48,176,68);
    text(d,19,h-26,"START",2,255,255,255);
    int count=0;
    for(int i=0;i<DESKTOP_WINDOWS;++i)if(d->windows[i].open)++count;
    int button_width=count?(w-216)/count:1;
    if(button_width>142)button_width=142;
    if(button_width<1)button_width=1;
    int slot=0;
    for(int i=0;i<DESKTOP_WINDOWS;++i) {
        if(!d->windows[i].open)continue;
        int bx=115+slot++*button_width;
        int wide=button_width-3;
        if(wide<1)wide=1;
        fill(d,bx,h-36,wide,31,desktop_top_window(d)==i?54:42,desktop_top_window(d)==i?127:98,204);
        fb_outline(d->surface,bx,h-36,wide,31,rgb(d,135,174,237));
        truncate_text(d,bx+5,h-25,desktop_titles[i],wide-9,1,rgb(d,255,255,255));
    }
    fill(d,w-90,h-37,85,32,36,108,188);
    char timer[22]; uint64_t secs=d->clock_ticks/100;
    char rev[22];int n=0;do{rev[n++]=(char)('0'+secs%10);secs/=10;}while(secs&&n<20);
    int k=0; timer[k++]='T';timer[k++]='+';
    while(n && k<21)timer[k++]=rev[--n];
    timer[k]=0;
    text(d,w-78,h-25,timer,1,255,255,255);
}
static void draw_menu(desktop_t *d) {
    if(!d->start_open)return;
    int h=d->surface->height,y=h-42-320;
    fill(d,2,y,500,320,238,245,255);
    fb_outline(d->surface,2,y,500,320,rgb(d,20,56,139));
    fill(d,3,y+3,498,37,26,95,185);
    text(d,19,y+15,"OPENXP PROGRAMS",2,255,255,255);
    static const char *const options[]={"TERMINAL","MY COMPUTER","ABOUT OPENXP","NOTEPAD","CALCULATOR",
                                         "PAINT","SYSTEM MONITOR","APPEARANCE","MINES"};
    for(int i=0;i<DESKTOP_WINDOWS;++i) {
        int xx=12+(i/5)*245,yy=y+51+(i%5)*49;
        bool selected=(unsigned)i==d->menu_selection;
        fill(d,xx,yy,230,40,selected?210:249,selected?231:250,selected?255:253);
        text(d,xx+15,yy+15,options[i],1,28,57,104);
    }
}
static void cursor_restore(desktop_t *d) {
    if(!d->cursor_painted)return;
    fb_surface_t *s=d->surface;
    for(int y=0;y<13;++y)for(int x=0;x<9;++x) {
        int px=d->cursor_saved_x+x,py=d->cursor_saved_y+y;
        if(px>=0 && px<s->width && py>=0 && py<s->height)
            *(volatile uint32_t *)(s->pixels+(size_t)py*s->pitch+(size_t)px*4)=
                d->cursor_background[y*9+x];
    }
    d->cursor_painted=false;
}
static void cursor_paint(desktop_t *d) {
    static const uint16_t shape[]={1,3,7,15,31,63,127,255,511,63,103,195,129};
    fb_surface_t *s=d->surface;
    int x=d->mouse_x,y=d->mouse_y;
    d->cursor_saved_x=x;d->cursor_saved_y=y;
    for(int row=0;row<13;++row)for(int col=0;col<9;++col) {
        int px=x+col,py=y+row;
        if(px>=0 && px<s->width && py>=0 && py<s->height) {
            volatile uint32_t *pixel=(volatile uint32_t *)(s->pixels+(size_t)py*s->pitch+(size_t)px*4);
            d->cursor_background[row*9+col]=*pixel;
            if(shape[row]&(1u<<(8-col)))
                *pixel=fb_rgb(s,255,255,255);
            if(col==0 && row>=1 && row<12)
                *pixel=fb_rgb(s,12,30,51);
        }
    }
    d->cursor_painted=true;
}
void desktop_cursor_update(desktop_t *d) {
    if(!d || !d->surface || !d->surface->pixels)return;
    if(d->cursor_painted && d->cursor_saved_x==d->mouse_x &&
       d->cursor_saved_y==d->mouse_y)return;
    cursor_restore(d);
    cursor_paint(d);
}
void desktop_draw(desktop_t *d) {
    /* A complete repaint overwrites the old cursor and its underlay. */
    d->cursor_painted=false;
    draw_wallpaper(d);draw_icons(d);
    int top=desktop_top_window(d);
    for(int z=0;z<DESKTOP_WINDOWS;++z)
        draw_window(d,d->order[z],d->order[z]==top);
    draw_taskbar(d);draw_menu(d);
    cursor_paint(d);
}
void desktop_redraw_moved_window(desktop_t *d,int oldx,int oldy,int index) {
    if(!d||!d->surface||index<0||index>=DESKTOP_WINDOWS)return;
    desktop_window_t *w=&d->windows[index];
    int x0=oldx<w->x?oldx:w->x;
    int y0=oldy<w->y?oldy:w->y;
    int x1=oldx>w->x?oldx:w->x;
    int y1=oldy>w->y?oldy:w->y;
    /* Include right/bottom 5px shadow and an extra pixel of border. */
    fb_surface_t *s=d->surface;
    cursor_restore(d);
    fb_clip_set(s,x0-1,y0-1,x1-x0+w->w+7,y1-y0+w->h+7);
    draw_wallpaper(d);draw_icons(d);
    int top=desktop_top_window(d);
    for(int z=0;z<DESKTOP_WINDOWS;++z)
        draw_window(d,d->order[z],d->order[z]==top);
    draw_taskbar(d);draw_menu(d);
    fb_clip_clear(s);
    cursor_paint(d);
}
void desktop_redraw_taskbar(desktop_t *d) {
    if(!d || !d->surface)return;
    cursor_restore(d);
    draw_taskbar(d);
    cursor_paint(d);
}
void desktop_redraw_active(desktop_t *d) {
    if(!d || d->start_open)return;
    int top=desktop_top_window(d);
    if(top<0)return;
    cursor_restore(d);
    draw_window(d,top,true);
    cursor_paint(d);
}
void desktop_redraw_paint(desktop_t *d){
    if(!d||!d->paint_dirty || desktop_top_window(d)!=DESKTOP_PAINT)return;
    desktop_window_t *w=&d->windows[DESKTOP_PAINT];
    int x=w->x+16+d->paint_min_x*2,y=w->y+96+d->paint_min_y*2;
    int width=(d->paint_max_x-d->paint_min_x+1)*2;
    int height=(d->paint_max_y-d->paint_min_y+1)*2;
    cursor_restore(d);
    fb_clip_set(d->surface,x,y,width,height);
    draw_window(d,DESKTOP_PAINT,true);
    fb_clip_clear(d->surface);
    cursor_paint(d);
    d->paint_dirty=false;
}
void desktop_redraw_terminal(desktop_t *d){
    if(d && desktop_top_window(d)==DESKTOP_TERMINAL)desktop_redraw_active(d);
}
static void desktop_click(desktop_t *d,int x,int y) {
    int h=d->surface->height;
    if(y>=h-42) {
        if(x<110){d->start_open=!d->start_open;d->menu_selection=0;return;}
        int count=0,slot=0;
        for(int i=0;i<DESKTOP_WINDOWS;++i)if(d->windows[i].open)++count;
        int bw=count?(d->surface->width-216)/count:1;
        if(bw>142)bw=142;
        if(bw<1)bw=1;
        for(int i=0;i<DESKTOP_WINDOWS;++i) {
            if(!d->windows[i].open)continue;
            int bx=115+slot++*bw;
            if(x>=bx&&x<bx+bw-3) {
                if(desktop_top_window(d)==i)d->windows[i].minimized=true;
                else open_window(d,i);
                d->start_open=false;return;
            }
        }
        d->start_open=false;return;
    }
    if(d->start_open) {
        int top=h-42-320;
        for(int i=0;i<DESKTOP_WINDOWS;++i) {
            if(inside(x,y,12+(i/5)*245,top+51+(i%5)*49,230,40)) {
                open_window(d,i);break;
            }
        }
        d->start_open=false;return;
    }
    for(int z=DESKTOP_WINDOWS-1;z>=0;--z) {
        int i=d->order[z];desktop_window_t *w=&d->windows[i];
        if(!w->open||w->minimized||!inside(x,y,w->x,w->y,w->w,w->h))continue;
        if(y<w->y+33 && x>=w->x+w->w-31){w->open=false;return;}
        if(y<w->y+33 && x>=w->x+w->w-57){w->minimized=true;return;}
        focus(d,i);
        if(i==DESKTOP_PAINT){
            if(inside(x,y,w->x+16,w->y+54,8*38,26)){
                int color=(x-(w->x+16))/38;
                if(color>=0&&color<8)d->paint_color=(unsigned)color;
                return;
            }
            if(inside(x,y,w->x+16,w->y+w->h-35,75,23)){paint_clear(d);return;}
            if(inside(x,y,w->x+16,w->y+96,320,200)){
                d->paint_active=true;
                d->paint_last_x=(x-w->x-16)/2;d->paint_last_y=(y-w->y-96)/2;
                paint_set(d,d->paint_last_x,d->paint_last_y);
                return;
            }
        }
        if(i==DESKTOP_SETTINGS){
            for(unsigned index=0;index<3;++index)
                if(inside(x,y,w->x+18,w->y+105+(int)index*44,w->w-36,39)){
                    d->appearance=index;return;
                }
        }
        if(i==DESKTOP_MINES){
            if(inside(x,y,w->x+200,w->y+47,60,25)){mine_restart(d);return;}
            int cell=mine_cell_at(d,x,y);
            if(cell>=0){mine_reveal(d,cell);return;}
        }
        if(i==DESKTOP_FILES && y>=w->y+112 && y<w->y+112+(int)initrd_count()*23) {
            int choice=(y-w->y-112)/23;
            if(choice>=0 && choice<(int)initrd_count()) {
                if(d->selected_file==choice && d->clock_ticks-d->last_file_click<40) {
                    char path[64];
                    if(initrd_name_at((size_t)choice,path,sizeof(path)) && d->shell.services.run)
                        console_output(d,d->shell.services.run(d->shell.services.ctx,path)?
                            "Application exited successfully.":"Application not executable (or faulted).");
                }
                d->selected_file=choice;d->last_file_click=d->clock_ticks;
            }
            return;
        }
        if(i==DESKTOP_FILES && d->selected_file>=0 &&
           inside(x,y,w->x+w->w-109,w->y+w->h-34,99,25)) {
            char path[64];
            if(initrd_name_at((size_t)d->selected_file,path,sizeof(path)) && d->shell.services.run)
                console_output(d,d->shell.services.run(d->shell.services.ctx,path)?
                    "Application exited successfully.":"Application not executable (or faulted).");
            return;
        }
        if(i==DESKTOP_CALCULATOR && inside(x,y,w->x+13,w->y+96,w->w-25,184)) {
            int cell=(w->w-30)/4;
            int col=(x-(w->x+13))/cell,row=(y-(w->y+96))/46;
            static const char *keys[4]={"789/","456*","123-","C0=+"};
            if(row>=0&&row<4&&col>=0&&col<4 && (x-(w->x+13))%cell<cell-5)
                calculator_press(d,keys[row][col]);
            return;
        }
        if(y<w->y+33){d->dragging=true;d->drag_window=i;d->last_x=x;d->last_y=y;}
        return;
    }
    if(inside(x,y,12,25,104,65))open_window(d,DESKTOP_FILES);
    else if(inside(x,y,12,115,104,65))open_window(d,DESKTOP_FILES);
    else if(inside(x,y,12,205,104,65))open_window(d,DESKTOP_TERMINAL);
}
void desktop_mouse(desktop_t *d,int x,int y,bool down) {
    if(!d||!d->surface)return;
    if(x<0)x=0;
    if(y<0)y=0;
    if(x>=d->surface->width)x=d->surface->width-1;
    if(y>=d->surface->height)y=d->surface->height-1;
    if(down&&!d->mouse_down)desktop_click(d,x,y);
    else if(down && d->paint_active && desktop_top_window(d)==DESKTOP_PAINT){
        desktop_window_t *w=&d->windows[DESKTOP_PAINT];
        int px=(x-w->x-16)/2,py=(y-w->y-96)/2;
        if(inside(x,y,w->x+16,w->y+96,320,200)){
            paint_line(d,d->paint_last_x,d->paint_last_y,px,py);
            d->paint_last_x=px;d->paint_last_y=py;
        } else d->paint_active=false;
    }
    else if(down && d->dragging) {
        desktop_window_t *w=&d->windows[d->drag_window];
        int nx=w->x+x-d->last_x,ny=w->y+y-d->last_y;
        if(nx<0)nx=0;
        if(ny<0)ny=0;
        if(nx+w->w>d->surface->width)nx=d->surface->width-w->w;
        if(ny+w->h>d->surface->height-42)ny=d->surface->height-42-w->h;
        w->x=nx;w->y=ny;d->last_x=x;d->last_y=y;
    }
    if(!down){d->dragging=false;d->paint_active=false;}
    d->mouse_down=down;
    d->mouse_x=x;d->mouse_y=y;
}
void desktop_right_mouse(desktop_t *d,int x,int y,bool down){
    if(!d)return;
    if(down && !d->right_down && desktop_top_window(d)==DESKTOP_MINES)
        mine_toggle_flag(d,mine_cell_at(d,x,y));
    d->right_down=down;
}
void desktop_key(desktop_t *d,char c) {
    if(c=='\t') {
        for(int i=0;i<DESKTOP_WINDOWS;++i) {
            int idx=d->order[i];
            if(d->windows[idx].open&&!d->windows[idx].minimized){focus(d,idx);break;}
        }
        return;
    }
    if(desktop_top_window(d)==DESKTOP_TERMINAL) {
        (void)shell_key(&d->shell,c);
        if(!d->shell.visible)d->windows[DESKTOP_TERMINAL].minimized=true;
    }
}
static void show_desktop(desktop_t *d) {
    if(!d->showing_desktop) {
        for(int i=0;i<DESKTOP_WINDOWS;++i) {
            d->saved_minimized[i]=d->windows[i].minimized;
            d->windows[i].minimized=true;
        }
        d->showing_desktop=true;
    } else {
        for(int i=0;i<DESKTOP_WINDOWS;++i)
            d->windows[i].minimized=d->saved_minimized[i];
        d->showing_desktop=false;
    }
    d->start_open=false;
}
static void switch_window(desktop_t *d) {
    int top=desktop_top_window(d);
    for(int z=DESKTOP_WINDOWS-1;z>=0;--z) {
        int idx=d->order[z];
        if(idx!=top && d->windows[idx].open && !d->windows[idx].minimized) {
            focus(d,idx);return;
        }
    }
}
void desktop_key_event(desktop_t *d,const input_event_t *event) {
    if(!d||!event||event->kind!=INPUT_KEY)return;
    uint8_t mods=event->modifiers;
    char c=event->key;
    char lc=(c>='A'&&c<='Z')?(char)(c+32):c;
    if(event->code==KEY_WINDOWS ||
       (event->code==KEY_ESCAPE && (mods&KEYMOD_CTRL))) {
        d->start_open=!d->start_open;
        d->menu_selection=0;
        return;
    }
    if(mods&KEYMOD_WIN) {
        switch(lc) {
        case 'e':open_window(d,DESKTOP_FILES);d->start_open=false;return;
        case 'r':open_window(d,DESKTOP_TERMINAL);d->start_open=false;return;
        case 'd':show_desktop(d);return;
        case 'n':open_window(d,DESKTOP_NOTEPAD);d->start_open=false;return;
        case 'c':open_window(d,DESKTOP_CALCULATOR);d->start_open=false;return;
        case 'p':open_window(d,DESKTOP_PAINT);d->start_open=false;return;
        case 'm':open_window(d,DESKTOP_MONITOR);d->start_open=false;return;
        case 's':open_window(d,DESKTOP_SETTINGS);d->start_open=false;return;
        case 'g':open_window(d,DESKTOP_MINES);d->start_open=false;return;
        default: break;
        }
    }
    if(mods&KEYMOD_ALT) {
        if(event->code==KEY_TAB){switch_window(d);d->start_open=false;return;}
        if(event->code==KEY_F4){
            int top=desktop_top_window(d);
            if(top>=0)d->windows[top].open=false;
            d->start_open=false;return;
        }
    }
    if(event->code==KEY_ESCAPE) {d->start_open=false;return;}
    if(d->start_open) {
        if(event->code==KEY_UP) {
            d->menu_selection=(d->menu_selection+DESKTOP_WINDOWS-1)%DESKTOP_WINDOWS;return;
        }
        if(event->code==KEY_DOWN) {
            d->menu_selection=(d->menu_selection+1)%DESKTOP_WINDOWS;return;
        }
        if(event->code==KEY_RIGHT && d->menu_selection<5){d->menu_selection+=5;return;}
        if(event->code==KEY_LEFT && d->menu_selection>=5){d->menu_selection-=5;return;}
        if(event->code==KEY_ENTER) {
            open_window(d,(int)d->menu_selection);d->start_open=false;return;
        }
        /* Typing while the Start menu is visible must not leak into Terminal. */
        return;
    }
    if(desktop_top_window(d)==DESKTOP_SETTINGS){
        if(c>='1'&&c<='3')d->appearance=(unsigned)(c-'1');
        return;
    }
    if(desktop_top_window(d)==DESKTOP_MINES){
        if(lc=='n')mine_restart(d);
        if(lc=='f')mine_toggle_flag(d,mine_cell_at(d,d->mouse_x,d->mouse_y));
        return;
    }
    if(desktop_top_window(d)==DESKTOP_PAINT){
        if(lc=='x')paint_clear(d);
        else if(c>='1'&&c<='8')d->paint_color=(unsigned)(c-'1');
        return;
    }
    if(desktop_top_window(d)==DESKTOP_NOTEPAD){note_key(d,event);return;}
    if(desktop_top_window(d)==DESKTOP_CALCULATOR){
        if(event->code==KEY_ENTER)calculator_press(d,'=');
        else if(!(mods&(KEYMOD_CTRL|KEYMOD_ALT|KEYMOD_WIN)))calculator_press(d,c);
        return;
    }
    if(desktop_top_window(d)==DESKTOP_FILES) {
        if(event->code==KEY_UP && d->selected_file>0) {--d->selected_file;return;}
        if(event->code==KEY_DOWN && d->selected_file+1<(int)initrd_count()) {++d->selected_file;return;}
        if(event->code==KEY_ENTER && d->selected_file>=0) {
            char path[64];
            if(initrd_name_at((size_t)d->selected_file,path,sizeof(path)) && d->shell.services.run)
                console_output(d,d->shell.services.run(d->shell.services.ctx,path)?
                    "Application exited successfully.":"Application not executable (or faulted).");
            return;
        }
    }
    if(desktop_top_window(d)!=DESKTOP_TERMINAL)return;
    if((mods&KEYMOD_CTRL) && lc=='l') {
        shell_execute(&d->shell,"cls");return;
    }
    if((mods&KEYMOD_CTRL) && lc=='c') {
        d->shell.used=0;d->shell.input[0]=0;return;
    }
    if(event->code==KEY_UP) {shell_history(&d->shell,-1);return;}
    if(event->code==KEY_DOWN) {shell_history(&d->shell,1);return;}
    if(event->code==KEY_HOME) {shell_home(&d->shell);return;}
    if(event->code==KEY_END) {shell_end(&d->shell);return;}
    if(event->code==KEY_LEFT) {shell_left(&d->shell);return;}
    if(event->code==KEY_RIGHT) {shell_right(&d->shell);return;}
    if(event->code==KEY_DELETE) {shell_delete(&d->shell);return;}
    if(!(mods&(KEYMOD_CTRL|KEYMOD_ALT|KEYMOD_WIN)) && c)
        desktop_key(d,c);
}
void desktop_tick(desktop_t *d,uint64_t ticks){d->clock_ticks=ticks;}
