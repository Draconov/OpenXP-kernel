#include "oxp.h"
#include "text_edit.h"
#include "text_layout.h"
/* Bounded editable document with cursor, zoom, RAM save, and horizontal moves. */
static char document[960];
static size_t length,cursor;
static unsigned zoom=1,bold;
static char path[96]="home/wordpad.txt";
static void pick_open_document(void){
    app_file_request_t r={0};
    oxp_copy(r.path,"home/open-document.cfg",sizeof r.path);
    if(oxp_file(APP_FS_READ,&r)==0 && r.size>0 && r.size<sizeof path){
        unsigned i=0;for(;i<r.size && r.data[i] && i+1<sizeof path;++i)path[i]=(char)r.data[i];
        path[i]=0;
        (void)oxp_file(APP_FS_DELETE,&r);
    }
}

static const char *status="EDIT DOCUMENT - RAM ONLY";
static void save(void){
    app_file_request_t r={0};oxp_copy(r.path,path,sizeof r.path);
    /* Editing a bundled read-only document saves a writable HOME copy. */
    if(!(path[0]=='h'&&path[1]=='o'&&path[2]=='m'&&path[3]=='e'&&path[4]=='/')){
        const char *leaf=path;for(const char *scan=path;*scan;++scan)if(*scan=='/')leaf=scan+1;
        char target[96]="home/";unsigned n=5;
        while(*leaf && n+1<sizeof target)target[n++]=*leaf++;
        target[n]=0;oxp_copy(path,target,sizeof path);
        oxp_copy(r.path,path,sizeof r.path);
    }
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
static void draw_document_line(const char *line,unsigned current,unsigned start,
                               unsigned rows,unsigned *rendered,unsigned top,
                               unsigned zoom,unsigned w,oxp_text_position_t caret){
    if(current<start || *rendered>=rows)return;
    unsigned y=top+6u+*rendered*8u*zoom;
    oxp_label(11,y,line,zoom,bold?0x0b214d:0x273a55);
    if(caret.line==current){
        unsigned x=11u+caret.col*6u*zoom;
        if(x<w-6u)oxp_fill(x,y+7u*zoom,5u*zoom,1u,0x3876bc);
    }
    ++*rendered;
}
static unsigned toolbar_h(void){return oxp_canvas_width()<296u?44u:30u;}
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    unsigned compact=w<296u,top=toolbar_h(),bottom=h>16u?h-16u:0u;
    oxp_draw_cmd_t cmds[]={
        {OXP_DRAW_CLEAR,0,0,0,0,0xe5effc},
        {OXP_DRAW_FILL,6,top,w-12,h>top+18u?h-top-18u:1u,0xffffff},
        {OXP_DRAW_FILL,6,5,w-12,top-7u,0xcbdff8}};
    oxp_draw(cmds,3);
    oxp_label(11,compact?8u:12u,"WORDPAD",1,0x174579);
    if(compact){
        oxp_label(11,28,"SAVE",1,0x174579);oxp_label(52,28,"OPEN",1,0x174579);
        oxp_label(99,28,"NEW",1,0x174579);
        oxp_label(w-87u,28,bold?"BOLD":"NORM",1,0x174579);
        oxp_label(w-41u,28,zoom==1?"100%":"200%",1,0x174579);
    }else{
        oxp_label(72,12,"SAVE OPEN NEW",1,0x174579);
        oxp_label(185,12,bold?"BOLD":"NORMAL",1,0x174579);
        oxp_label(w-53u,12,zoom==1?"100%":"200%",1,0x174579);
    }
    unsigned cols=oxp_min(49u,(w-22u)/(6u*zoom));
    unsigned rows=h>top+23u?(h-top-23u)/(8u*zoom):0u;
    if(!cols)cols=1;
    oxp_text_position_t caret=oxp_text_position(document,length,cursor,cols);
    unsigned start=rows&&caret.line>=rows?caret.line-rows+1u:0u;
    char display[50];unsigned rendered=0,n=0,current=0;
    for(size_t i=0;i<=length && rendered<rows;++i){
        char ch=i<length?document[i]:0;
        if(n==cols){
            display[n]=0;
            draw_document_line(display,current++,start,rows,&rendered,top,zoom,w,caret);
            n=0;
        }
        if(ch=='\n'||!ch){
            display[n]=0;
            draw_document_line(display,current++,start,rows,&rendered,top,zoom,w,caret);
            n=0;
            if(!ch)break;
        }else if(ch>=32&&ch<127 && n<cols)display[n++]=ch;
    }
    if(h>18u){oxp_fill(5,bottom,w-10,16,0xe5effc);
        oxp_label(11,bottom+4,status,1,0x40648d);}
}
void oxp_main(void){
    if(oxp_open("WordPad - native ELF64"))oxp_exit(1);
    pick_open_document();
    load();
    for(;;){render();uint32_t e=oxp_wait(0);
        if(e==OXP_CLOSE||e==27)break;
        if(e==OXP_SAVE){save();continue;}
        if(e==OXP_OPEN){load();continue;}
        if(e==OXP_LEFT){if(cursor)--cursor;continue;}
        if(e==OXP_RIGHT){if(cursor<length)++cursor;continue;}
        if(e&OXP_CLICK){
            if(oxp_canvas_width()<296u){
                if(oxp_hit(e,11,22,32,21))save();
                else if(oxp_hit(e,50,22,41,21))load();
                else if(oxp_hit(e,95,22,34,21)){document[0]=0;length=cursor=0;status="NEW DOCUMENT";}
                else if(oxp_hit(e,oxp_canvas_width()-90u,22,43,21))bold=!bold;
                else if(oxp_hit(e,oxp_canvas_width()-46u,22,46,21))zoom=zoom==1?2:1;
            }else if(oxp_hit(e,72,5,31,23))save();
            else if(oxp_hit(e,105,5,35,23))load();
            else if(oxp_hit(e,141,5,35,23)){
                document[0]=0;length=cursor=0;status="NEW DOCUMENT";
            }else if(oxp_hit(e,183,5,73,23))bold=!bold;
            else if(oxp_hit(e,oxp_canvas_width()-55u,5,55,23))zoom=zoom==1?2:1;
            if(oxp_click_x(e)>=27u && oxp_click_y(e)>=43u+toolbar_h()+6u &&
               oxp_click_x(e)<16u+oxp_canvas_width()-10u &&
               oxp_click_y(e)<43u+oxp_canvas_height()-18u){
                unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
                unsigned top=toolbar_h(),cols=oxp_min(49u,(w-22u)/(6u*zoom));
                unsigned rows=h>top+23u?(h-top-23u)/(8u*zoom):0u;
                if(!cols)cols=1;
                oxp_text_position_t caret=oxp_text_position(document,length,cursor,cols);
                unsigned start=rows&&caret.line>=rows?caret.line-rows+1u:0u;
                unsigned line=start+(oxp_click_y(e)-43u-top-6u)/(8u*zoom);
                unsigned col=(oxp_click_x(e)-27u)/(6u*zoom);
                cursor=oxp_text_offset_at(document,length,cols,line,col);
            }
            continue;
        }
        if(e==8){(void)oxp_backspace(document,&length,&cursor);continue;}
        if((e==10 || (e>=32&&e<127))){
            (void)oxp_insert(document,&length,&cursor,sizeof document,(char)e);
        }
    }oxp_exit(0);
}
