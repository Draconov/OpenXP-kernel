#include "oxp.h"
#include "text_edit.h"
/* Bounded editable document with cursor, zoom, RAM save, and horizontal moves. */
static char document[960];
static size_t length,cursor;
static unsigned zoom=1,bold;
static const char path[]="home/wordpad.txt";
static const char *status="EDIT DOCUMENT - RAM ONLY";
static void save(void){
    app_file_request_t r={0};oxp_copy(r.path,path,sizeof r.path);
    r.size=(uint32_t)length;
    for(size_t i=0;i<length;++i)r.data[i]=(uint8_t)document[i];
    status=oxp_file(APP_FS_WRITE,&r)==0?"DOCUMENT SAVED":"SAVE FAILED";
}
static void load(void){
    app_file_request_t r={0};oxp_copy(r.path,path,sizeof r.path);
    if(!oxp_file(APP_FS_READ,&r) && r.size<sizeof document){
        length=r.size;
        for(size_t i=0;i<length;++i)document[i]=(char)r.data[i];
        document[length]=0;cursor=length;status="DOCUMENT LOADED";
    }else status="NO SAVED DOCUMENT";
}
static void render(void){
    oxp_draw_cmd_t cmds[]={
        {OXP_DRAW_CLEAR,0,0,0,0,0xe5effc},
        {OXP_DRAW_FILL,6,31,308,132,0xffffff},
        {OXP_DRAW_FILL,6,5,308,23,0xcbdff8}};
    oxp_draw(cmds,3);
    oxp_label(11,12,"WORDPAD",1,0x174579);
    oxp_label(72,12,"SAVE OPEN NEW",1,0x174579);
    oxp_label(185,12,bold?"BOLD":"NORMAL",1,0x174579);
    oxp_label(267,12,zoom==1?"100%":"200%",1,0x174579);
    unsigned cols=zoom==1?49u:25u,rows=zoom==1?15u:7u;
    /* Layout lines on demand so cursor remains visible without a huge buffer. */
    unsigned line_number=0,col=0,cursor_line=0,cursor_col=0;
    for(size_t i=0;i<=length;++i){
        if(i==cursor){cursor_line=line_number;cursor_col=col;}
        if(i==length)break;
        if(document[i]=='\n'){++line_number;col=0;}
        else if(++col>=cols){++line_number;col=0;}
    }
    unsigned start=cursor_line>=rows?cursor_line-rows+1u:0u;
    char display[50];unsigned rendered=0,n=0,current=0;
    for(size_t i=0;i<=length && rendered<rows;++i){
        char ch=i<length?document[i]:0;
        if(ch=='\n'||!ch||n==cols){
            display[n]=0;
            if(current>=start){
                unsigned y=37u+rendered*8u*zoom;
                oxp_label(11,y,display,zoom,bold?0x0b214d:0x273a55);
                if(cursor_line==current){
                    unsigned x=11u+cursor_col*6u*zoom;
                    if(x<309u)oxp_fill(x,y+7u*zoom,5u*zoom,1u,0x3876bc);
                }
                ++rendered;
            }
            ++current;n=0;
            if(!ch)break;
            if(ch=='\n')continue;
        }
        if(ch>=32&&ch<127 && n<cols)display[n++]=ch;
    }
    oxp_fill(5,165,310,15,0xe5effc);
    oxp_label(11,169,status,1,0x40648d);
}
void oxp_main(void){
    if(oxp_open("WordPad - native ELF64"))oxp_exit(1);
    load();
    for(;;){render();uint32_t e=oxp_wait(0);
        if(e==OXP_CLOSE||e==27)break;
        if(e==OXP_SAVE){save();continue;}
        if(e==OXP_OPEN){load();continue;}
        if(e==OXP_LEFT){if(cursor)--cursor;continue;}
        if(e==OXP_RIGHT){if(cursor<length)++cursor;continue;}
        if(e&OXP_CLICK){
            if(oxp_hit(e,72,5,31,23))save();
            else if(oxp_hit(e,105,5,35,23))load();
            else if(oxp_hit(e,141,5,35,23)){
                document[0]=0;length=cursor=0;status="NEW DOCUMENT";
            }else if(oxp_hit(e,183,5,73,23))bold=!bold;
            else if(oxp_hit(e,264,5,55,23))zoom=zoom==1?2:1;
            continue;
        }
        if(e==8){(void)oxp_backspace(document,&length,&cursor);continue;}
        if((e==10 || (e>=32&&e<127))){
            (void)oxp_insert(document,&length,&cursor,sizeof document,(char)e);
        }
    }oxp_exit(0);
}
