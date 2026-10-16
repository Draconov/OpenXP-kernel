#include "oxp.h"
#include "text_edit.h"
#include "edit_session.h"
#include "text_layout.h"
#include "scroll_layout.h"
#include "ui_widgets.h"
#include "advanced_input.h"
#include "file_picker.h"
/* Bounded editable document with cursor, zoom, RAM save, and horizontal moves. */
static char document[960];
static oxp_edit_history_t history;
static size_t length,cursor,anchor;
static int selecting;
static unsigned zoom=1,bold;
static unsigned scroll_top;
static int scroll_follow=1;
static char path[96]="home/wordpad.txt";
static oxp_picker_t file_picker;
static void pick_open_document(void){
    /* Per-process document path wins. Legacy mailbox remains a fallback for
     * older launchers; it is never consumed after receiving a launch arg. */
    if(oxp_launch_argument(path,sizeof path)>0)return;
    app_file_request_t r={0};
    oxp_copy(r.path,"home/open-wordpad.cfg",sizeof r.path);
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
        document[length]=0;cursor=anchor=length;selecting=0;scroll_top=0;scroll_follow=1;status="DOCUMENT LOADED";
        oxp_history_init(&history,document,length,cursor,anchor);
    }else status="NO SAVED DOCUMENT";
}
static void draw_document_line(const char *line,unsigned current,unsigned start,
                               unsigned rows,unsigned *rendered,unsigned top,
                               unsigned zoom,unsigned w,oxp_text_position_t caret,
                               oxp_text_position_t sel_lo,oxp_text_position_t sel_hi,
                               int has_selection){
    if(current<start || *rendered>=rows)return;
    unsigned y=top+6u+*rendered*8u*zoom;
    if(has_selection && current>=sel_lo.line && current<=sel_hi.line){
        unsigned line_len=(unsigned)oxp_len(line);
        unsigned a=current==sel_lo.line?sel_lo.col:0u;
        unsigned b=current==sel_hi.line?sel_hi.col:line_len;
        if(a>line_len)a=line_len;
        if(b>line_len)b=line_len;
        if(b>a && 11u+a*6u*zoom<w-22u){
            unsigned width=(b-a)*6u*zoom;
            if(width>w-22u-(11u+a*6u*zoom))width=w-22u-(11u+a*6u*zoom);
            oxp_fill(11u+a*6u*zoom,y,width,8u*zoom,0xa9caeeu);
        }
    }
    oxp_label(11,y,line,zoom,bold?0x0b214d:0x273a55);
    if(caret.line==current){
        unsigned x=11u+caret.col*6u*zoom;
        if(x<w-6u)oxp_fill(x,y+7u*zoom,5u*zoom,1u,0x3876bc);
    }
    ++*rendered;
}
static unsigned toolbar_h(void){unsigned w=oxp_canvas_width();return w<296u?44u:w>=610u?54u:30u;}
static unsigned doc_cols(unsigned w){
    return oxp_max(1u,oxp_min(118u,w>40u?(w-40u)/(6u*zoom):1u));
}
static unsigned doc_rows(unsigned h,unsigned top){
    return h>top+23u?(h-top-23u)/(8u*zoom):0u;
}
static unsigned doc_total_lines(unsigned cols){
    return oxp_text_position(document,length,length,cols).line+1u;
}
static void scroll_by(int direction){
    unsigned rows=doc_rows(oxp_canvas_height(),toolbar_h());
    unsigned cols=doc_cols(oxp_canvas_width());
    scroll_top=oxp_scroll_step(scroll_top,direction,doc_total_lines(cols),rows);
    scroll_follow=0;
}
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    unsigned compact=w<296u,top=toolbar_h(),bottom=h>16u?h-16u:0u;
    oxp_draw_cmd_t cmds[]={
        {OXP_DRAW_CLEAR,0,0,0,0,oxp_theme_toolbar(oxp_ui_theme)},
        {OXP_DRAW_FILL,6,top,w-12,h>top+18u?h-top-18u:1u,0xffffff},
        {OXP_DRAW_FILL,6,5,w-12,top-7u,oxp_theme_toolbar(oxp_ui_theme)}};
    oxp_draw(cmds,3);
    oxp_ui_toolbar((oxp_ui_rect){6u,5u,w-12u,top-7u});
    oxp_ui_text((oxp_ui_rect){11u,0u,w-18u,top},"WORDPAD",compact?8u:12u,oxp_ui_ink(),0);
    if(compact){
        oxp_ui_toolbutton((oxp_ui_rect){11u,22u,32u,21u},"SAVE",0);
        oxp_ui_toolbutton((oxp_ui_rect){50u,22u,41u,21u},"OPEN",0);
        oxp_ui_toolbutton((oxp_ui_rect){95u,22u,34u,21u},"NEW",0);
        oxp_ui_toolbutton((oxp_ui_rect){w-90u,22u,43u,21u},bold?"BOLD":"NORM",bold?2:0);
        oxp_ui_toolbutton((oxp_ui_rect){w-46u,22u,46u,21u},zoom==1?"100%":"200%",zoom==2?2:0);
    }else{
        oxp_ui_toolbutton((oxp_ui_rect){72u,5u,31u,23u},"SAVE",0);
        oxp_ui_toolbutton((oxp_ui_rect){105u,5u,35u,23u},"OPEN",0);
        oxp_ui_toolbutton((oxp_ui_rect){141u,5u,35u,23u},"NEW",0);
        oxp_ui_toolbutton((oxp_ui_rect){183u,5u,73u,23u},bold?"BOLD":"NORMAL",bold?2:0);
        oxp_ui_toolbutton((oxp_ui_rect){w-55u,5u,55u,23u},zoom==1?"100%":"200%",zoom==2?2:0);
        if(w>=610u){
            const char *names[]={"UNDO","REDO","CUT","COPY","PASTE"};
            for(unsigned i=0;i<5u;++i)
                oxp_ui_toolbutton((oxp_ui_rect){264u+57u*i,5u,50u,23u},names[i],0);
        }
    }
    unsigned cols=doc_cols(w),rows=doc_rows(h,top);
    unsigned total=doc_total_lines(cols);
    oxp_text_position_t caret=oxp_text_position(document,length,cursor,cols);
    size_t lo=anchor<cursor?anchor:cursor,hi=anchor>cursor?anchor:cursor;
    oxp_text_position_t sel_lo=oxp_text_position(document,length,lo,cols);
    oxp_text_position_t sel_hi=oxp_text_position(document,length,hi,cols);
    int has_selection=lo!=hi;
    if(scroll_follow)scroll_top=oxp_scroll_follow(scroll_top,caret.line,rows);
    scroll_top=oxp_scroll_clamp(scroll_top,total,rows);
    unsigned start=scroll_top;
    char display[120];unsigned rendered=0,n=0,current=0;
    for(size_t i=0;i<=length && rendered<rows;++i){
        char ch=i<length?document[i]:0;
        if(n==cols){
            display[n]=0;
            draw_document_line(display,current++,start,rows,&rendered,top,zoom,w,caret,sel_lo,sel_hi,has_selection);
            n=0;
        }
        if(ch=='\n'||!ch){
            display[n]=0;
            draw_document_line(display,current++,start,rows,&rendered,top,zoom,w,caret,sel_lo,sel_hi,has_selection);
            n=0;
            if(!ch)break;
        }else if(ch>=32&&ch<127 && n<cols)display[n++]=ch;
    }
    if(w>40u && bottom>top+32u){
        /* Narrow, independently scrollable document viewport with arrows.
         * Its track and thumb are derived from the same lines as hit-testing. */
        unsigned sx=w-19u,rail_y=top+20u,rail_h=bottom-top-39u;
        oxp_fill(sx,top+2u,15u,bottom-top-3u,0xd6e2f1u);
        oxp_label(sx+5u,top+5u,"^",1,0x244e86u);
        oxp_label(sx+5u,bottom-12u,"v",1,0x244e86u);
        oxp_ui_scrollbar((oxp_ui_rect){sx+2u,rail_y,11u,rail_h},start,rows,total);
    }
    if(w>=390u && w<610u)
        oxp_ui_toolbutton((oxp_ui_rect){265u,5u,70u,23u},"SAVE AS",0);
    if(w>=610u)
        oxp_ui_toolbutton((oxp_ui_rect){72u,30u,74u,21u},"SAVE AS",0);
    if(h>18u){
        oxp_ui_statusbar(w,h,status);
    }
    oxp_picker_render(&file_picker);
}
/* Click and drag share the same visual-line math used by rendering. */
static int in_document(oxp_gesture_point_t p){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height(),top=toolbar_h();
    return w>40u && h>top+23u &&
           oxp_gesture_inside(p,6u,top+2u,w-27u,h-top-19u);
}
static size_t document_at(oxp_gesture_point_t p){
    unsigned w=oxp_canvas_width(),top=toolbar_h(),cols=doc_cols(w);
    unsigned x=p.x>11u?p.x-11u:0u,y=p.y>top+6u?p.y-top-6u:0u;
    unsigned col=x/(6u*zoom),line=scroll_top+y/(8u*zoom);
    if(col>cols)col=cols;
    return oxp_text_offset_at(document,length,cols,line,col);
}
typedef struct { int drag; unsigned grab; unsigned original; } word_scroll_drag_t;
static word_scroll_drag_t scroll_drag;
static unsigned scroll_rail_y(void){return toolbar_h()+20u;}
static unsigned scroll_rail_h(void){
    unsigned h=oxp_canvas_height(),top=toolbar_h(),bottom=h>16u?h-16u:0u;
    return bottom>top+39u?bottom-top-39u:0u;
}
static int in_scrollbar(oxp_gesture_point_t p){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height(),top=toolbar_h();
    return w>40u && h>top+48u &&
           oxp_gesture_inside(p,w-19u,top+2u,15u,h-16u-top-3u);
}
static void scroll_thumb_begin(oxp_gesture_point_t p){
    unsigned rail=scroll_rail_h(),rows=doc_rows(oxp_canvas_height(),toolbar_h());
    unsigned total=doc_total_lines(doc_cols(oxp_canvas_width()));
    oxp_scroll_thumb_t th=oxp_scroll_thumb(scroll_top,total,rows,rail);
    if(!oxp_gesture_inside(p,oxp_canvas_width()-17u,scroll_rail_y(),11u,rail))return;
    unsigned yy=p.y-scroll_rail_y();
    scroll_drag.drag=1;
    scroll_drag.grab=(yy>=th.y && yy<th.y+th.h)?yy-th.y:th.h/2u;
    scroll_drag.original=scroll_top;
    scroll_top=oxp_gesture_scroll_at(p.y,scroll_rail_y(),rail,th.h,
                                 scroll_drag.grab,oxp_scroll_limit(total,rows));
    scroll_follow=0;
}
static void scroll_thumb_move(oxp_gesture_point_t p){
    if(!scroll_drag.drag||!p.valid)return;
    unsigned rail=scroll_rail_h(),rows=doc_rows(oxp_canvas_height(),toolbar_h());
    unsigned total=doc_total_lines(doc_cols(oxp_canvas_width()));
    oxp_scroll_thumb_t th=oxp_scroll_thumb(scroll_drag.original,total,rows,rail);
    scroll_top=oxp_gesture_scroll_at(p.y,scroll_rail_y(),rail,th.h,
                                scroll_drag.grab,oxp_scroll_limit(total,rows));
    scroll_follow=0;
}
static void edit_command(uint32_t e){
    int edit=oxp_edit_key(e,document,&length,&cursor,&anchor,sizeof document,
                          doc_cols(oxp_canvas_width()),&history,
                          oxp_clipboard_set,oxp_clipboard_get);
    if(edit){scroll_follow=1;
        if(edit==2)status="DOCUMENT EDITED";
        else if(e==OXP_KEY_COPY)status="COPIED SELECTION";
    }
}
static void toolbar_press(oxp_gesture_point_t p){
    unsigned w=oxp_canvas_width();
    if(w>=610u && oxp_gesture_inside(p,72u,30u,74u,21u)){
        oxp_picker_begin(&file_picker,1,path);return;
    }
    if(w>=390u && w<610u && oxp_gesture_inside(p,265u,5u,70u,23u)){
        oxp_picker_begin(&file_picker,1,path);return;
    }
    if(w>=610u){
        const uint32_t actions[]={OXP_KEY_UNDO,OXP_KEY_REDO,OXP_KEY_CUT,
                                  OXP_KEY_COPY,OXP_KEY_PASTE};
        for(unsigned i=0;i<5u;++i)
            if(oxp_gesture_inside(p,264u+57u*i,5u,50u,23u)){
                edit_command(actions[i]);return;
            }
    }
    if(w<296u){
        if(oxp_gesture_inside(p,11,22,32,21))save();
        else if(oxp_gesture_inside(p,50,22,41,21))oxp_picker_begin(&file_picker,0,path);
        else if(oxp_gesture_inside(p,95,22,34,21)){
            document[0]=0;length=cursor=anchor=scroll_top=0;scroll_follow=1;status="NEW DOCUMENT";
            oxp_history_init(&history,document,length,cursor,anchor);
        }else if(w>=90u && oxp_gesture_inside(p,w-90u,22,43,21))bold=!bold;
        else if(w>=46u && oxp_gesture_inside(p,w-46u,22,46,21))zoom=zoom==1?2:1;
    }else if(oxp_gesture_inside(p,72,5,31,23))save();
    else if(oxp_gesture_inside(p,105,5,35,23))oxp_picker_begin(&file_picker,0,path);
    else if(oxp_gesture_inside(p,141,5,35,23)){
        document[0]=0;length=cursor=anchor=scroll_top=0;scroll_follow=1;status="NEW DOCUMENT";
            oxp_history_init(&history,document,length,cursor,anchor);
    }else if(oxp_gesture_inside(p,183,5,73,23))bold=!bold;
    else if(oxp_gesture_inside(p,w-55u,5,55,23))zoom=zoom==1?2:1;
}
static void release_click(oxp_gesture_point_t start,oxp_gesture_point_t end){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height(),top=toolbar_h();
    if(in_scrollbar(start)&&in_scrollbar(end)){
        unsigned bottom=h>16u?h-16u:0u;
        if(oxp_gesture_inside(start,w-19u,top+2u,15u,18u) &&
           oxp_gesture_inside(end,w-19u,top+2u,15u,18u))scroll_by(-1);
        else if(oxp_gesture_inside(start,w-19u,bottom-17u,15u,17u) &&
                oxp_gesture_inside(end,w-19u,bottom-17u,15u,17u))scroll_by(1);
        return;
    }
    if(in_document(start))return;
    if(start.valid && end.valid){
        /* No toolbar command fires if the pointer is released elsewhere. */
        if(w<296u){
            const unsigned rx[]={11u,50u,95u,w>=90u?w-90u:0u,w>=46u?w-46u:0u};
            const unsigned rw[]={32u,41u,34u,43u,46u};
            for(unsigned i=0;i<5;++i)if(oxp_gesture_inside(start,rx[i],22u,rw[i],21u)&&
                       oxp_gesture_inside(end,rx[i],22u,rw[i],21u)){toolbar_press(end);break;}
        }else{
            const unsigned rx[]={72u,105u,141u,183u,w-55u};
            const unsigned rw[]={31u,35u,35u,73u,55u};
            for(unsigned i=0;i<5;++i)if(oxp_gesture_inside(start,rx[i],5u,rw[i],23u)&&
                       oxp_gesture_inside(end,rx[i],5u,rw[i],23u)){toolbar_press(end);break;}
            if(w>=610u &&
                oxp_gesture_inside(start,72u,30u,74u,21u)&&
                oxp_gesture_inside(end,72u,30u,74u,21u))toolbar_press(end);
            if(w>=390u&&w<610u &&
                oxp_gesture_inside(start,265u,5u,70u,23u)&&
                oxp_gesture_inside(end,265u,5u,70u,23u))toolbar_press(end);
            if(w>=610u)for(unsigned i=0;i<5u;++i)
                if(oxp_gesture_inside(start,264u+57u*i,5u,50u,23u)&&
                   oxp_gesture_inside(end,264u+57u*i,5u,50u,23u)){
                    toolbar_press(end);break;
                }
        }
    }
}
void oxp_main(void){
    if(oxp_open("WordPad - native ELF64"))oxp_exit(1);
    oxp_ui_atlas_init_widgets();pick_open_document();load();
    if(!history.count)oxp_history_init(&history,document,length,cursor,anchor);
    oxp_gesture_t gesture={0};
    for(;;){render();uint32_t e=oxp_ui_wait_gesture();
        if(file_picker.active){
            int done=oxp_picker_event(&file_picker,e,OXP_FT_TEXT);
            if(done==1){
                oxp_copy(path,file_picker.picked,sizeof path);
                if(file_picker.save)save();else load();
            }
            continue;
        }
        if(e==OXP_CLOSE||e==27)break;
        if(e==OXP_SAVE){save();continue;}
        if(e==OXP_OPEN){oxp_picker_begin(&file_picker,0,path);continue;}

        if(e==OXP_RESIZE){selecting=scroll_drag.drag=0;gesture.active=0;continue;}
        if(e&OXP_CLICK){
            oxp_gesture_point_t p=oxp_gesture_point(e);
            if(in_document(p)){cursor=anchor=document_at(p);scroll_follow=1;oxp_history_break(&history);}
            else toolbar_press(p);
            continue;
        }
        unsigned kind=oxp_gesture_feed(&gesture,e);
        oxp_gesture_point_t p=oxp_gesture_point(e);
        if(kind==OXP_GESTURE_DOWN){
            scroll_drag.drag=0;selecting=0;
            if(in_scrollbar(p))scroll_thumb_begin(p);
            else if(in_document(p)){
                cursor=anchor=document_at(p);selecting=1;scroll_follow=0;oxp_history_break(&history);
            }
        }else if(kind==OXP_GESTURE_MOVE){
            if(scroll_drag.drag)scroll_thumb_move(p);
            else if(selecting && p.valid){
                unsigned h=oxp_canvas_height(),top=toolbar_h();
                if(p.y<top+6u)scroll_by(-1);
                else if(p.y>=h-18u)scroll_by(1);
                cursor=document_at(p);scroll_follow=0;
            }
        }else if(kind==OXP_GESTURE_UP){
            if(selecting && p.valid)cursor=document_at(p);
            selecting=0;
            if(scroll_drag.drag){scroll_thumb_move(p);scroll_drag.drag=0;}
            else release_click(gesture.start,p);
        }
        edit_command(e);
    }
    oxp_exit(0);
}
