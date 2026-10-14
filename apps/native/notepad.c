#include "oxp.h"
static char note[960];
static size_t length;
static void load(void){
    uint64_t n=oxp_read(note,sizeof(note)-1);
    if(n<sizeof(note)){length=(size_t)n;note[length]=0;}
}
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    oxp_draw_cmd_t background[]={
        {OXP_DRAW_CLEAR,0,0,0,0,0xf5f8ff},
        {OXP_DRAW_FILL,0,0,w,27,0xd6e7fb},
        {OXP_DRAW_FILL,5,32,w-10,h>40?h-38:1,0xffffff}
    };
    oxp_draw(background,3);
    oxp_label(10,9,"NOTEPAD",1,0x173c72);
    if(w>=165u){oxp_label(w-119u,9,"S SAVE",1,0x174b8a);oxp_label(w-63u,9,"O OPEN",1,0x174b8a);}
    unsigned cols=oxp_min(47u,(w-20u)/6u);
    unsigned rows=h>39u?oxp_min(16u,(h-39u)/8u):0u;
    if(!cols || !rows)return;
    size_t start=length>350?length-350:0;
    unsigned row=0,col=0;
    char line[48];
    for(size_t i=start;i<=length && row<rows;++i){
        char ch=i<length?note[i]:'\0';
        if(ch=='\n'||ch=='\0'||col==cols){
            line[col]=0;
            oxp_label(10,37+row*8,line,1,0x263951);
            ++row;col=0;
            if(ch=='\0')break;
            if(ch=='\n')continue;
        }
        if(ch>=32&&ch<=126 && col<cols)line[col++]=ch;
    }
}
void oxp_main(void){
    if(oxp_open("Notepad - native ELF64"))oxp_exit(1);
    load();
    for(;;){
        render();
        uint32_t key=oxp_wait(0);
        if(!key)continue;
        if(key==OXP_CLOSE||key==27)break;
        if(key==OXP_SAVE){oxp_write(note,length);continue;}
        if(key==OXP_OPEN){load();continue;}
        if(key&OXP_CLICK)continue;
        if(key==8){if(length)note[--length]=0;continue;}
        if((key==10||(key>=32&&key<=126))&&length+1<sizeof(note)){
            note[length++]=(char)key;note[length]=0;
        }
    }
    oxp_exit(0);
}
