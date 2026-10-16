#include "oxp.h"
#include "ui_widgets.h"
#include "text_edit.h"
#include "edit_session.h"
#include "text_layout.h"
#include "scroll_layout.h"
#include "advanced_input.h"
#include "file_picker.h"
/* Bounded RAM-backed editor. Display positions use the same wrap math as
 * pointer hit-testing, so selection/caret stay aligned after resizing. */
static char note[960];
static oxp_edit_history_t history;
static size_t length,cursor,anchor;
static unsigned scroll_top;
static char note_path[96];
static oxp_picker_t file_picker;
static int selecting,thumb_drag,scroll_follow=1;
static unsigned thumb_grab,thumb_original;
static const char *status="READY - HOME BUFFER";
static void pick_open_document(void){
    /* Per-process document path wins. Legacy mailbox remains a fallback for
     * older launchers; it is never consumed after receiving a launch arg. */
    if(oxp_launch_argument(note_path,sizeof note_path)>0)return;
    app_file_request_t r={0};
    oxp_copy(r.path,"home/open-notepad.cfg",sizeof r.path);
    if(oxp_file(APP_FS_READ,&r)==0 && r.size && r.size<sizeof note_path){
        unsigned n=0;while(n<r.size&&r.data[n]&&n+1u<sizeof note_path){
            note_path[n]=(char)r.data[n];++n;
        }
        note_path[n]=0;(void)oxp_file(APP_FS_DELETE,&r);
    }
}
static void load(void){
    size_t n=0;
    if(note_path[0]){
        app_file_request_t r={0};oxp_copy(r.path,note_path,sizeof r.path);
        if(oxp_file(APP_FS_READ,&r)!=0||r.size>=sizeof note){status="OPEN FAILED";return;}
        n=r.size;
        for(unsigned i=0;i<n;++i)note[i]=(char)r.data[i];
    }else{
        uint64_t size=oxp_read(note,sizeof note-1u);
        if(size>=sizeof note){status="OPEN FAILED";return;}n=(size_t)size;
    }
    length=n;note[n]=0;cursor=anchor=n;
    scroll_top=0;scroll_follow=1;status="DOCUMENT OPENED";
    oxp_history_init(&history,note,length,cursor,anchor);
}
static void save(void){
    if(!note_path[0]){
        status=oxp_write(note,length)==length?"SAVED TO HOME BUFFER":"SAVE FAILED";return;
    }
    if(!oxp_doc_home(note_path)){
        char copy[96]="home/";oxp_copy(copy+5u,oxp_doc_leaf(note_path),sizeof copy-5u);
        oxp_copy(note_path,copy,sizeof note_path);
    }
    app_file_request_t r={0};oxp_copy(r.path,note_path,sizeof r.path);
    r.size=(uint32_t)length;
    for(unsigned i=0;i<length;++i)r.data[i]=(uint8_t)note[i];
    status=oxp_file(APP_FS_WRITE,&r)==0?"DOCUMENT SAVED":"SAVE FAILED";
}
static oxp_ui_rect save_button(unsigned w){return (oxp_ui_rect){w-121u,2u,55u,21u};}
static oxp_ui_rect open_button(unsigned w){return (oxp_ui_rect){w-63u,2u,58u,21u};}
static unsigned columns(unsigned w){return oxp_max(1u,oxp_min(100u,w>37u?(w-37u)/6u:1u));}
static unsigned rows_visible(unsigned h){return h>55u?(h-55u)/8u:0u;}
static unsigned total_lines(unsigned cols){return oxp_text_position(note,length,length,cols).line+1u;}
static int in_document(oxp_gesture_point_t p){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    return w>37u&&h>55u&&oxp_gesture_inside(p,6u,30u,w-29u,h-49u);
}
static size_t at_document(oxp_gesture_point_t p){
    unsigned cols=columns(oxp_canvas_width());
    unsigned x=p.x>10u?p.x-10u:0u,y=p.y>36u?p.y-36u:0u;
    unsigned col=x/6u,line=scroll_top+y/8u;
    return oxp_text_offset_at(note,length,cols,line,col>cols?cols:col);
}
static void scroll_by(int direction){
    unsigned h=oxp_canvas_height(),w=oxp_canvas_width();
    scroll_top=oxp_scroll_step(scroll_top,direction,total_lines(columns(w)),rows_visible(h));
    scroll_follow=0;
}
static unsigned rail_height(unsigned h){return h>82u?h-82u:0u;}
static unsigned rail_top(void){return 47u;}
static int over_scroll(oxp_gesture_point_t p){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    return w>37u&&h>89u&&oxp_gesture_inside(p,w-18u,30u,14u,h-49u);
}
static void thumb_start(oxp_gesture_point_t p){
    unsigned h=oxp_canvas_height(),w=oxp_canvas_width();
    unsigned rail=rail_height(h),count=total_lines(columns(w)),rows=rows_visible(h);
    oxp_scroll_thumb_t th=oxp_scroll_thumb(scroll_top,count,rows,rail);
    if(!oxp_gesture_inside(p,w-16u,rail_top(),10u,rail))return;
    unsigned pos=p.y-rail_top();
    thumb_drag=1;
    thumb_original=scroll_top;
    thumb_grab=(pos>=th.y&&pos<th.y+th.h)?pos-th.y:th.h/2u;
    scroll_top=oxp_gesture_scroll_at(p.y,rail_top(),rail,th.h,thumb_grab,
                                      oxp_scroll_limit(count,rows));
    scroll_follow=0;
}
static void thumb_move(oxp_gesture_point_t p){
    if(!thumb_drag||!p.valid)return;
    unsigned h=oxp_canvas_height(),w=oxp_canvas_width();
    unsigned rail=rail_height(h),count=total_lines(columns(w)),rows=rows_visible(h);
    oxp_scroll_thumb_t th=oxp_scroll_thumb(thumb_original,count,rows,rail);
    scroll_top=oxp_gesture_scroll_at(p.y,rail_top(),rail,th.h,thumb_grab,
                                      oxp_scroll_limit(count,rows));
    scroll_follow=0;
}
static void draw_line(unsigned line,const char *buffer,unsigned size,unsigned w,unsigned current_row,
                      oxp_text_position_t caret,oxp_text_position_t sel_a,
                      oxp_text_position_t sel_b,int has_select){
    unsigned y=36u+current_row*8u;
    if(has_select&&line>=sel_a.line&&line<=sel_b.line){
        unsigned a=line==sel_a.line?sel_a.col:0u;
        unsigned b=line==sel_b.line?sel_b.col:size;
        if(a>size)a=size;if(b>size)b=size;
        if(b>a&&10u+a*6u<w-22u){
            unsigned n=(b-a)*6u;
            if(n>w-22u-(10u+a*6u))n=w-22u-(10u+a*6u);
            oxp_fill(10u+a*6u,y,n,8u,0xa9caeeu);
        }
    }
    oxp_label(10u,y,buffer,1u,oxp_ui_edit_ink());
    if(caret.line==line&&10u+caret.col*6u<w-22u)
        oxp_fill(10u+caret.col*6u,y+7u,5u,1u,0x3876bcu);
}
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    oxp_ui_clear();oxp_ui_appbar(w,"NOTEPAD");
    if(w>=270u)oxp_ui_toolbutton(
        (oxp_ui_rect){78u,2u,69u,21u},"SAVE AS",0);
    if(w>=180u){
        oxp_ui_toolbutton(save_button(w),"SAVE",0);
        oxp_ui_toolbutton(open_button(w),"OPEN",0);
    }
    if(w>=560u){
        const char *names[]={"UNDO","REDO","CUT","COPY","PASTE"};
        for(unsigned i=0;i<5u;++i)
            oxp_ui_toolbutton((oxp_ui_rect){150u+56u*i,2u,52u,21u},names[i],0);
    }
    unsigned bottom=h>19u?h-19u:h;
    if(w<=37u||bottom<=35u)return;
    oxp_ui_sheet((oxp_ui_rect){5u,30u,w-10u,bottom-34u});
    unsigned cols=columns(w),rows=rows_visible(h);
    if(!cols||!rows){oxp_ui_statusbar(w,h,status);return;}
    oxp_text_position_t caret=oxp_text_position(note,length,cursor,cols);
    if(scroll_follow)scroll_top=oxp_scroll_follow(scroll_top,caret.line,rows);
    scroll_top=oxp_scroll_clamp(scroll_top,total_lines(cols),rows);
    size_t lo=anchor<cursor?anchor:cursor,hi=anchor>cursor?anchor:cursor;
    oxp_text_position_t sel_a=oxp_text_position(note,length,lo,cols);
    oxp_text_position_t sel_b=oxp_text_position(note,length,hi,cols);
    unsigned line=0,col=0,visible=0;char buffer[102];
    for(size_t i=0;i<=length&&visible<rows;++i){
        char ch=i<length?note[i]:0;
        if(col==cols){
            buffer[col]=0;
            if(line>=scroll_top){draw_line(line,buffer,col,w,visible,caret,sel_a,sel_b,lo!=hi);++visible;}
            ++line;col=0;
        }
        if(ch=='\n'||!ch){
            buffer[col]=0;
            if(line>=scroll_top&&visible<rows){
                draw_line(line,buffer,col,w,visible,caret,sel_a,sel_b,lo!=hi);++visible;
            }
            ++line;col=0;if(!ch)break;
        }else if(ch>=32&&ch<=126&&col<cols)buffer[col++]=ch;
    }
    if(h>89u){
        unsigned sx=w-18u,rail=rail_height(h);
        oxp_fill(sx,30u,14u,h-49u,0xd7e5f5u);
        oxp_label(sx+4u,34u,"^",1u,0x244e86u);
        oxp_label(sx+4u,h-31u,"v",1u,0x244e86u);
        oxp_ui_scrollbar((oxp_ui_rect){sx+2u,rail_top(),10u,rail},
                         scroll_top,rows,total_lines(cols));
    }
    oxp_ui_statusbar(w,h,status);
    oxp_picker_render(&file_picker);
}
static void edit_command(uint32_t e){
    int edit=oxp_edit_key(e,note,&length,&cursor,&anchor,sizeof note,
                          columns(oxp_canvas_width()),&history,
                          oxp_clipboard_set,oxp_clipboard_get);
    if(edit){scroll_follow=1;
        if(edit==2)status="DOCUMENT EDITED";
        else if(e==OXP_KEY_COPY)status="COPIED SELECTION";
    }
}
static void buttons(oxp_gesture_point_t p){
    unsigned w=oxp_canvas_width();
    if(w>=560u){
        const uint32_t actions[]={OXP_KEY_UNDO,OXP_KEY_REDO,OXP_KEY_CUT,
                                  OXP_KEY_COPY,OXP_KEY_PASTE};
        for(unsigned i=0;i<5u;++i)
            if(oxp_gesture_inside(p,150u+56u*i,2u,52u,21u)){
                edit_command(actions[i]);return;
            }
    }
    if(w>=270u&&w<560u&&oxp_gesture_inside(p,78u,2u,69u,21u)){
        oxp_picker_begin(&file_picker,1,note_path[0]?note_path:"home/notepad.txt");return;
    }
    if(w<180u)return;
    oxp_ui_rect save_rect=save_button(w),open=open_button(w);
    if(oxp_gesture_inside(p,save_rect.x,save_rect.y,save_rect.w,save_rect.h))
        save();
    else if(oxp_gesture_inside(p,open.x,open.y,open.w,open.h))oxp_picker_begin(&file_picker,0,note_path);
}
void oxp_main(void){
    if(oxp_open("Notepad - native ELF64"))oxp_exit(1);
    oxp_ui_atlas_init_widgets();pick_open_document();load();
    if(!history.count)oxp_history_init(&history,note,length,cursor,anchor);
    oxp_gesture_t gesture={0};
    for(;;){
        render();uint32_t e=oxp_ui_wait_gesture();
        if(file_picker.active){
            int done=oxp_picker_event(&file_picker,e,OXP_FT_TEXT);
            if(done==1){
                oxp_copy(note_path,file_picker.picked,sizeof note_path);
                if(file_picker.save)save();else load();
            }
            continue;
        }
        if(e==OXP_CLOSE||e==27)break;
        if(e==OXP_SAVE){save();continue;}
        if(e==OXP_OPEN){oxp_picker_begin(&file_picker,0,note_path);continue;}

        if(e==OXP_RESIZE){selecting=thumb_drag=0;gesture.active=0;continue;}
        if(e&OXP_CLICK){
            oxp_gesture_point_t p=oxp_gesture_point(e);
            if(in_document(p)){cursor=anchor=at_document(p);scroll_follow=1;oxp_history_break(&history);}
            else buttons(p);
            continue;
        }
        unsigned k=oxp_gesture_feed(&gesture,e);oxp_gesture_point_t p=oxp_gesture_point(e);
        if(k==OXP_GESTURE_DOWN){
            selecting=thumb_drag=0;
            if(over_scroll(p))thumb_start(p);
            else if(in_document(p)){cursor=anchor=at_document(p);selecting=1;scroll_follow=0;oxp_history_break(&history);}
        }else if(k==OXP_GESTURE_MOVE){
            if(thumb_drag)thumb_move(p);
            else if(selecting&&p.valid){
                if(p.y<36u)scroll_by(-1);
                else if(p.y>=oxp_canvas_height()-20u)scroll_by(1);
                cursor=at_document(p);scroll_follow=0;
            }
        }else if(k==OXP_GESTURE_UP){
            if(selecting&&p.valid)cursor=at_document(p);
            selecting=0;
            if(thumb_drag){thumb_move(p);thumb_drag=0;}
            else if(gesture.start.valid&&p.valid){
                unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
                if(over_scroll(gesture.start)&&over_scroll(p)){
                    if(oxp_gesture_inside(gesture.start,w-18u,30u,14u,17u)&&
                       oxp_gesture_inside(p,w-18u,30u,14u,17u))scroll_by(-1);
                    else if(oxp_gesture_inside(gesture.start,w-18u,h-36u,14u,17u)&&
                            oxp_gesture_inside(p,w-18u,h-36u,14u,17u))scroll_by(1);
                }else if(w>=180u){
                    if(w>=560u){
                        unsigned matched=0;
                        for(unsigned i=0;i<5u;++i)
                            if(oxp_gesture_click(&gesture,p,150u+56u*i,2u,52u,21u)){
                                buttons(p);matched=1;break;
                            }
                        if(matched)continue;
                    }
                    if(w>=270u&&w<560u&&
                        oxp_gesture_inside(gesture.start,78u,2u,69u,21u)&&
                        oxp_gesture_inside(p,78u,2u,69u,21u))buttons(p);
                    oxp_ui_rect a=save_button(w),b=open_button(w);
                    if(oxp_gesture_inside(gesture.start,a.x,a.y,a.w,a.h)&&
                       oxp_gesture_inside(p,a.x,a.y,a.w,a.h))buttons(p);
                    else if(oxp_gesture_inside(gesture.start,b.x,b.y,b.w,b.h)&&
                            oxp_gesture_inside(p,b.x,b.y,b.w,b.h))buttons(p);
                }
            }
        }
        edit_command(e);
    }
    oxp_exit(0);
}
