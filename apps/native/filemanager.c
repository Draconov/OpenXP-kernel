#include "oxp.h"
#include "filetypes.h"
#include "file_workflow.h"
#include "file_drop.h"
#include "wm_protocol.h"
#include "scroll_layout.h"
#include "ui_widgets.h"
#include "advanced_input.h"
/* XP-inspired Explorer for the bounded OpenXP VFS. System files are read-only.
 * Context actions do not bypass the kernel executable allowlist or RAMFS ACL. */
static app_file_request_t req;
static char cwd[96],previous[96],selected[96],preview[78],status[74]="LOCAL DISK (C:)";
static char rename_buf[33];
static unsigned scroll,ren_len;
static int dragging_file,drop_highlight,drag_offered;
static char drop_folder[96];
static char dragging_path[96];
static oxp_double_click_t double_click;
static int renaming,creating,context_menu,properties_open;
static uint32_t properties_size,properties_kind;
static char properties_path[96];
static void decimal(char out[16],uint32_t value){
    char rev[16];unsigned n=0,i=0;
    do{rev[n++]=(char)('0'+value%10u);value/=10u;}while(value&&n<15u);
    while(n)out[i++]=rev[--n];out[i]=0;
}
static int equal(const char *a,const char *b);
static int folder(const char *path);
static int home(const char *p){
    const char *a="home/",*b="my documents/",*c="documents and settings/user/my documents/";
    unsigned i=0;while(a[i]&&p[i]==a[i])++i;if(!a[i])return 1;
    i=0;while(b[i]&&p[i]==b[i])++i;if(!b[i])return 1;
    i=0;while(c[i]&&p[i]==c[i])++i;return !c[i];
}
static int writable_dir(const char *p){
    return equal(p,"home")||equal(p,"my documents")||
        equal(p,"documents and settings/user/my documents") ||
        (home(p) && folder(p));
}
static int equal(const char *a,const char *b){while(*a&&*a==*b){++a;++b;}return !*a&&!*b;}
static const char *leaf(const char *path){
    const char *p=path;while(*path){if(*path=='/')p=path+1;++path;}return p;
}
static int entry(unsigned index,char *name){
    req=(app_file_request_t){0};req.index=index;req.dest[0]='D';
    oxp_copy(req.path,cwd,sizeof req.path);
    if(oxp_file(APP_FS_LIST,&req))return 0;
    oxp_copy(name,req.path,96);return 1;
}
static int folder(const char *path){
    req=(app_file_request_t){0};oxp_copy(req.path,path,sizeof req.path);
    return oxp_file(APP_FS_KIND,&req)==3;
}
static void choose(unsigned index){
    char path[96];if(!entry(index,path)){selected[0]=0;preview[0]=0;return;}
    oxp_copy(selected,path,sizeof selected);preview[0]=0;
    oxp_filetype_t type=oxp_filetype(path,folder(path));
    oxp_copy(status,oxp_filetype_label(type),sizeof status);
    if(type==OXP_FT_TEXT||type==OXP_FT_CONFIG){
        req=(app_file_request_t){0};oxp_copy(req.path,path,sizeof req.path);
        if(oxp_file(APP_FS_READ,&req)==0){
            unsigned at=0;for(unsigned i=0;i<req.size && at+1<sizeof preview;++i){
                unsigned char c=req.data[i];if(c=='\n'||c=='\r')c=' ';
                if(c>=32&&c<=126)preview[at++]=(char)c;
            }
            preview[at]=0;
        }
    }
}
static void enter_folder(const char *path){
    if(!equal(cwd,path)){oxp_copy(previous,cwd,sizeof previous);oxp_copy(cwd,path,sizeof cwd);}
    double_click.armed=0;
    scroll=0;selected[0]=0;preview[0]=0;context_menu=0;
    oxp_copy(status,"FOLDER OPENED",sizeof status);
}
static void parent(void){
    if(!cwd[0])return;
    char next[96];oxp_copy(next,cwd,sizeof next);
    unsigned n=0;while(next[n])++n;
    while(n&&next[n-1]!='/')--n;
    if(n)next[n-1]=0;else next[0]=0;
    enter_folder(next);
}
static void back(void){char path[96];oxp_copy(path,previous,sizeof path);enter_folder(path);}
static void handoff(const char *path){
    const char *program=oxp_doc_program(path);
    if(!program){oxp_copy(status,"NO ASSOCIATED APPLICATION",sizeof status);return;}
    /* Aurora 15: atomic launch packet, bound to the new process by PID.
     * Never use the old global editor mailboxes for new document launches. */
    oxp_copy(status,oxp_launch_document(program,path)==0?
        "OPENING DOCUMENT":"APPLICATION BUSY / UNAVAILABLE",sizeof status);
}
static void set_wallpaper(const char *path){
    unsigned found=0;
    for(unsigned i=0;i<32;++i){
        app_file_request_t r={0};r.index=i;r.dest[0]='D';
        oxp_copy(r.path,"windows/web/wallpaper",sizeof r.path);
        if(oxp_file(APP_FS_LIST,&r))break;
        unsigned n=0;while(r.path[n])++n;
        if(n<4||!oxp_iequal(r.path+n-4,".bmp"))continue;
        if(equal(r.path,path)){
            int64_t h=oxp_connect();if(h<=0)break;
            wm_packet_t m={.magic=WM_MAGIC,.version=WM_VERSION,
                .operation=WM_WALLPAPER,.x=found},reply={0};
            int64_t sent=oxp_send((uint64_t)h,&m,sizeof m);
            int64_t got=sent==(int64_t)sizeof m?oxp_recv((uint64_t)h,&reply,sizeof reply):-1;
            if(got==-11 && oxp_ipc_wait((uint64_t)h,60)>=0)
                got=oxp_recv((uint64_t)h,&reply,sizeof reply);
            (void)oxp_close((uint64_t)h);
            if(got==(int64_t)sizeof reply && reply.status==WM_OK &&
               reply.operation==(WM_WALLPAPER|WM_REPLY_FLAG)){
                app_file_request_t pref={0};
                oxp_copy(pref.path,"home/wallpaper.cfg",sizeof pref.path);
                pref.size=1;pref.data[0]=(uint8_t)found;
                (void)oxp_file(APP_FS_WRITE,&pref);
                oxp_copy(status,"DESKTOP BACKGROUND APPLIED",sizeof status);
                return;
            }
            break;
        }
        ++found;
    }
    oxp_copy(status,"BACKGROUND NOT AVAILABLE",sizeof status);
}
static void show_properties(void){
    if(!selected[0]){oxp_copy(status,"SELECT A FILE OR FOLDER",sizeof status);return;}
    app_file_request_t info={0};
    oxp_copy(info.path,selected,sizeof info.path);
    if(oxp_file(APP_FS_STAT,&info)){
        oxp_copy(status,"FILE INFORMATION UNAVAILABLE",sizeof status);return;
    }
    properties_kind=info.index;properties_size=info.size;
    oxp_copy(properties_path,selected,sizeof properties_path);
    context_menu=0;properties_open=1;
}
static void start_folder(void){
    context_menu=0;
    if(!writable_dir(cwd)){
        oxp_copy(status,"NEW FOLDER: OPEN DOCUMENTS",sizeof status);return;
    }
    creating=1;renaming=1;ren_len=0;rename_buf[0]=0;
    oxp_copy(status,"NEW FOLDER NAME + ENTER",sizeof status);
}
static void open_selected(void){
    context_menu=0;if(!selected[0])return;
    if(folder(selected)){enter_folder(selected);return;}
    oxp_filetype_t type=oxp_filetype(selected,0);
    if(type==OXP_FT_EXE || (selected[0]=='b'&&selected[1]=='i'&&selected[2]=='n'&&selected[3]=='/')){
        oxp_copy(status,oxp_launch(selected)==0?"APPLICATION LAUNCH QUEUED":"NOT AN OPENXP EXECUTABLE",sizeof status);
    }else if(type==OXP_FT_BITMAP){set_wallpaper(selected);}
    else if(oxp_doc_program(selected))handoff(selected);
    else oxp_copy(status,"NO ASSOCIATED APPLICATION",sizeof status);
}
/* Tagged file clipboard shared with editors' system clipboard. System files
 * can be copied into writable HOME, never moved or deleted. */
static void clipboard_selected(int move){
    context_menu=0;
    if(!selected[0]){oxp_copy(status,"SELECT A FILE OR FOLDER",sizeof status);return;}
    if(folder(selected) && !move){oxp_copy(status,"FOLDER COPY NOT SUPPORTED; USE CUT",sizeof status);return;}
    if(move&&!home(selected)){oxp_copy(status,"SYSTEM FILE IS READ ONLY",sizeof status);return;}
    oxp_file_clip_t payload={0};oxp_clip_file_pack(&payload,selected,move);
    oxp_copy(status,oxp_clipboard_set(&payload,sizeof payload)==(int64_t)sizeof payload?
        (move?(folder(selected)?"FOLDER CUT - PASTE TO MOVE":"FILE CUT - PASTE TO MOVE"):"FILE COPIED - PASTE TO COPY"):
        "CLIPBOARD FAILED",sizeof status);
}
static void paste_payload(const oxp_file_clip_t *p){
    oxp_file_clip_t payload=*p;
    /* Writable HOME and My Documents are views of the same store. Other
     * VFS directories are virtual or read-only and cannot be paste targets. */
    if(!writable_dir(cwd)){
        oxp_copy(status,"PASTE: OPEN DOCUMENTS FIRST",sizeof status);return;
    }
    if(payload.move && equal(cwd,"home") && home(payload.path) &&
       !folder(payload.path) && equal(payload.path,selected)){
        /* HOME and My Documents are aliases of one directory: moving a
         * file within that same store must not silently rename it. */
        oxp_copy(status,"FILE ALREADY IN DOCUMENTS",sizeof status);
        (void)oxp_clipboard_set("",0);return;
    }
    char destination[96];int found=0;
    for(unsigned n=0;n<30u;++n){
        if(!oxp_clip_destination_in(destination,payload.path,cwd,n))break;
        app_file_request_t probe={0};oxp_copy(probe.path,destination,sizeof probe.path);
        if(oxp_file(APP_FS_KIND,&probe)==0){found=1;break;}
    }
    if(!found){oxp_copy(status,"NO AVAILABLE FILE NAME",sizeof status);return;}
    req=(app_file_request_t){0};
    oxp_copy(req.path,payload.path,sizeof req.path);
    oxp_copy(req.dest,destination,sizeof req.dest);
    if(payload.move&&!home(payload.path)){
        oxp_copy(status,"SYSTEM FILE CANNOT MOVE",sizeof status);return;
    }
    int moving_folder=payload.move&&folder(payload.path);
    int ok=oxp_file(payload.move?APP_FS_RENAME:APP_FS_COPY,&req)==0;
    if(ok){
        oxp_copy(status,payload.move?(moving_folder?"FOLDER MOVED":"FILE MOVED TO DOCUMENTS"):"FILE PASTED TO DOCUMENTS",sizeof status);
        oxp_copy(selected,destination,sizeof selected);scroll=0;
        if(payload.move)(void)oxp_clipboard_set("",0);
    }else oxp_copy(status,"PASTE FAILED / STORAGE FULL",sizeof status);
}
static void paste_file(void){
    context_menu=0;
    oxp_file_clip_t payload={0};
    int64_t got=oxp_clipboard_get(&payload,sizeof payload);
    if(got<0 || !oxp_clip_file_valid(&payload,(size_t)got)){
        oxp_copy(status,"NO FILE IN CLIPBOARD",sizeof status);return;
    }
    paste_payload(&payload);
}
static void copy_selected(void){clipboard_selected(0);}
static void delete_selected(void){
    context_menu=0;if(!home(selected)){oxp_copy(status,"SYSTEM FILE IS READ ONLY",sizeof status);return;}
    req=(app_file_request_t){0};oxp_copy(req.path,selected,sizeof req.path);
    int success=oxp_file(folder(selected)?APP_FS_RMDIR:APP_FS_DELETE,&req)==0;
    oxp_copy(status,success?"FILE DELETED":"DELETE FAILED",sizeof status);
    if(success){selected[0]=0;preview[0]=0;scroll=0;double_click.armed=0;}
}
static void start_rename(void){
    context_menu=0;
    if(!home(selected)){oxp_copy(status,"RENAME: HOME ITEMS ONLY",sizeof status);return;}
    creating=0;
    renaming=1;ren_len=0;rename_buf[0]=0;
    oxp_copy(status,"TYPE NEW NAME + ENTER",sizeof status);
}
static void finish_rename(void){
    if(!ren_len)return;
    req=(app_file_request_t){0};
    if(creating){
        oxp_copy(req.path,cwd,sizeof req.path);
    }else{
        oxp_copy(req.path,selected,sizeof req.path);
        unsigned end=0;while(selected[end])++end;
        while(end&&selected[end-1]!='/')--end;
        if(!end || end>=sizeof req.dest)return;
        for(unsigned i=0;i<end;++i)req.dest[i]=selected[i];
        req.dest[end]=0;
    }
    unsigned n=0;while(req.dest[n])++n;
    if(creating){if(n+1>=sizeof req.dest)return;req.dest[n++]='/';}
    if(n+ren_len>=48u){oxp_copy(status,"FOLDER NAME TOO LONG",sizeof status);return;}
    for(unsigned i=0;i<ren_len;++i)req.dest[n+i]=rename_buf[i];
    req.dest[n+ren_len]=0;
    int ok;
    if(creating){
        oxp_copy(req.path,req.dest,sizeof req.path);
        ok=oxp_file(APP_FS_MKDIR,&req)==0;
    }else ok=oxp_file(APP_FS_RENAME,&req)==0;
    oxp_copy(status,ok?(creating?"FOLDER CREATED":"ITEM RENAMED"):
                (creating?"CREATE FOLDER FAILED":"RENAME FAILED"),sizeof status);
    if(ok){oxp_copy(selected,req.dest,sizeof selected);preview[0]=0;scroll=0;double_click.armed=0;}
}
static void draw_icon(unsigned x,unsigned y,oxp_filetype_t t){
    unsigned tile=t==OXP_FT_DIR?0u:t==OXP_FT_EXE?1u:
                  (t==OXP_FT_TEXT||t==OXP_FT_CONFIG)?2u:
                  (t==OXP_FT_BITMAP||t==OXP_FT_DRAWING)?3u:4u;
    if(oxp_ui_icon(tile,x,y,13u))return;

    unsigned col=0x88a8c7;
    if(t==OXP_FT_DIR)col=0xe4ba55;
    else if(t==OXP_FT_EXE)col=0x4586cc;
    else if(t==OXP_FT_TEXT)col=0xf6f9ff;
    else if(t==OXP_FT_BITMAP)col=0x61b583;
    else if(t==OXP_FT_DRAWING)col=0xc48dcb;
    else if(t==OXP_FT_CONFIG)col=0x9daec1;
    oxp_fill(x,y+2,12,10,col);
    if(t==OXP_FT_DIR)oxp_fill(x+1,y,6,3,0xefcf86);
    else if(t==OXP_FT_EXE)oxp_fill(x+2,y+4,8,4,0xe7f4ff);
    else if(t==OXP_FT_TEXT)oxp_fill(x+2,y+5,8,1,0x56799b);
    else if(t==OXP_FT_BITMAP)oxp_fill(x+2,y+5,6,5,0x397caa);
    else if(t==OXP_FT_DRAWING)oxp_fill(x+3,y+5,5,5,0xf4d37a);
}
static const char *locations[]={"","my documents","system apps",
    "windows/web/wallpaper","program files","windows/system32"};
static const char *location_names[]={"C: DRIVE","DOCUMENTS","SYSTEM APPS",
    "WALLPAPERS","PROGRAMS","SYSTEM32"};
static unsigned sidebar_width(unsigned w){return w>=292u?91u:78u;}
static unsigned visible_rows(unsigned h){
    unsigned rows=h>70u?(h-69u)/15u:0u;
    return rows>32u?32u:rows;
}
static unsigned entry_count(void){
    char path[96];unsigned count=0;
    while(count<64u && entry(count,path))++count;
    return count;
}
static void scroll_by(int direction){
    scroll=oxp_scroll_step(scroll,direction,entry_count(),visible_rows(oxp_canvas_height()));
    choose(scroll);
}
static void draw_name(unsigned x,unsigned y,const char *name,unsigned right){
    /* The shared font engine measures proportional glyphs and draws an
     * ellipsis within this row instead of guessing six pixels per letter. */
    if(right<=x)return;
    oxp_ui_text((oxp_ui_rect){x,y,right-x,14u},name,y,0x254c78u,0);
}
static unsigned menu_x(unsigned w){return w>154u?w-153u:0u;}
static unsigned menu_y(unsigned h){return h>136u?h-136u:0u;}
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    unsigned side=sidebar_width(w),bottom=h>19u?h-19u:0u;
    oxp_draw_cmd_t b[]={{OXP_DRAW_CLEAR,0,0,0,0,oxp_theme_panel(oxp_ui_theme)},
        {OXP_DRAW_FILL,0,0,w,21,oxp_theme_accent(oxp_ui_theme)},
        {OXP_DRAW_FILL,0,21,w,27,oxp_theme_toolbar(oxp_ui_theme)},
        {OXP_DRAW_FILL,0,48,side-1,h>67u?h-67u:1u,oxp_theme_sidebar(oxp_ui_theme)},
        {OXP_DRAW_FILL,side,48,w-side,h>67u?h-67u:1u,0xffffff},
        {OXP_DRAW_FILL,0,bottom,w,h-bottom,oxp_theme_sidebar(oxp_ui_theme)}};
    oxp_draw(b,6);
    oxp_ui_toolbar((oxp_ui_rect){0,21u,w,27u});
    oxp_ui_text((oxp_ui_rect){9u,0u,w-18u,21u},"MY COMPUTER",7u,0xffffff,0);
    oxp_ui_toolbutton((oxp_ui_rect){5,24u,61u,21u},"< BACK",0);
    oxp_ui_toolbutton((oxp_ui_rect){68u,24u,46u,21u},"^ UP",0);
    if(w>=330u)oxp_ui_toolbutton((oxp_ui_rect){w-100u,24u,94u,21u},"+ NEW FOLDER",0);
    oxp_ui_text((oxp_ui_rect){117u,25u,w>120u?w-120u:0u,17u},
                cwd[0]?cwd:"C:\\ LOCAL DISK",28u,oxp_ui_ink(),0);
    for(unsigned i=0;i<6u;++i){
        unsigned y=50u+i*17u;
        if(y+16u>bottom)break;
        if(equal(cwd,locations[i]))oxp_fill(1,y,side-2u,16,oxp_theme_selection(oxp_ui_theme));
        unsigned tile=(i==0u?1u:i==1u?11u:i==2u?5u:
                       i==3u?3u:i==4u?0u:4u);
        if(!oxp_ui_icon(tile,5u,y+1u,14u))
            oxp_fill(5u,y+3u,10u,10u,0x78a5d4);
        draw_name(23u,y+3u,location_names[i],side-2u);
    }
    unsigned rows=visible_rows(h),total=entry_count();
    scroll=oxp_scroll_clamp(scroll,total,rows);
    for(unsigned i=0;i<rows;++i){
        char path[96];if(!entry(scroll+i,path))break;
        int dir=folder(path);
        oxp_filetype_t t=oxp_filetype(path,dir);
        unsigned y=49u+i*15u;
        if(equal(selected,path)||equal(drop_folder,path))
            oxp_fill(side+3u,y,w-side-25u,15,oxp_theme_selection(oxp_ui_theme));
        draw_icon(side+7u,y+1u,t);
        draw_name(side+23u,y+4u,leaf(path),w-21u);
    }
    if(h>89u && w>side+26u){
        unsigned sx=w-18u,rail_y=65u,rail_h=bottom>82u?bottom-82u:1u;
        oxp_fill(sx,48,16u,bottom-48u,0xd7e5f5u);
        oxp_label(sx+5u,52u,"^",1,0x244e86u);
        oxp_label(sx+5u,bottom-12u,"v",1,0x244e86u);
        oxp_ui_scrollbar((oxp_ui_rect){sx+2u,rail_y,12u,rail_h},scroll,rows,total);
    }
    if(h>=27u){
        if(renaming){oxp_label(5,bottom+4u,"NEW NAME:",1,0x225583);
            oxp_label(65,bottom+4u,rename_buf,1,0x225583);}
        else oxp_ui_text((oxp_ui_rect){5u,bottom,w>12u?w-12u:0u,17u},status,bottom+5u,oxp_ui_soft_ink(),0);
    }
    if(drop_highlight && h>=103u){
        oxp_fill(1u,67u,side-2u,16u,oxp_theme_selection(oxp_ui_theme));
        oxp_ui_text((oxp_ui_rect){23u,69u,side>26u?side-26u:0u,14u},
                    "DOCUMENTS",72u,oxp_ui_ink(),0);
    }
    if(properties_open){
        unsigned pw=w>232u?230u:w-12u,ph=115u;
        unsigned px=(w-pw)/2u,py=h>ph?(h-ph)/2u:0u;
        oxp_ui_panel((oxp_ui_rect){px,py,pw,ph});
        oxp_fill(px,py,pw,19u,oxp_theme_accent(oxp_ui_theme));
        oxp_label(px+6u,py+5u,"PROPERTIES",1u,0xffffff);
        oxp_ui_text((oxp_ui_rect){px+6u,py+24u,pw-12u,14u},leaf(properties_path),py+26u,oxp_ui_ink(),0);
        oxp_label(px+6u,py+44u,properties_kind==3u?"FOLDER":
                  properties_kind==2u?"USER DOCUMENT":"SYSTEM FILE",1u,oxp_ui_ink());
        char bytes[16];decimal(bytes,properties_size);
        oxp_label(px+6u,py+62u,"SIZE:",1u,oxp_ui_ink());
        oxp_label(px+47u,py+62u,bytes,1u,oxp_ui_ink());
        oxp_label(px+6u,py+79u,properties_kind==2u?"WRITABLE":"READ ONLY / FOLDER",1u,oxp_ui_ink());
        oxp_ui_button((oxp_ui_rect){px+pw-53u,py+91u,47u,19u},"OK",OXP_UI_NORMAL,0);
    }
    if(context_menu && h>=142u){
        unsigned x=menu_x(w),y=menu_y(h);
        oxp_ui_menu((oxp_ui_rect){x,y,144,132});
        const char *actions[]={"OPEN","COPY","CUT","PASTE","RENAME","DELETE","PROPERTIES"};
        for(unsigned i=0;i<7u;++i)oxp_ui_menu_entry(
            (oxp_ui_rect){x+6u,y+5u+i*17u,132u,16u},actions[i]);
    }
}
/* Pointer-captured scrollbar: move thumb independently of file selection. */
static oxp_gesture_t file_pointer;
static unsigned thumb_grab,thumb_original;
static int dragging_thumb;
static int scrollbar_hit(oxp_gesture_point_t p){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height(),bottom=h>19u?h-19u:0u;
    return w>sidebar_width(w)+26u && bottom>82u &&
           oxp_gesture_inside(p,w-18u,48u,16u,bottom-48u);
}
/* Publish an opt-in drag ticket to the trusted shell. No clipboard side
 * effects; drop onto Notepad/WordPad/Paint opens a new document process. */
static void offer_cross_window_drag(const char *path){
    wm_drag_packet_t drag={.magic=WM_MAGIC,.version=WM_VERSION,.operation=WM_DRAG_BEGIN};
    unsigned n=0;
    while(path[n]&&n+1u<sizeof drag.path){drag.path[n]=path[n];++n;}
    if(path[n])return; /* bounded IPC cannot represent an oversized path */
    int64_t channel=oxp_connect();if(channel<=0)return;
    if(oxp_send((uint64_t)channel,&drag,sizeof drag)==(int64_t)sizeof drag){
        wm_packet_t ignored={0};
        int64_t got=oxp_recv((uint64_t)channel,&ignored,sizeof ignored);
        if(got==-11 && oxp_ipc_wait((uint64_t)channel,20)>=0)
            (void)oxp_recv((uint64_t)channel,&ignored,sizeof ignored);
    }
    (void)oxp_close((uint64_t)channel);
}
static void scrollbar_begin(oxp_gesture_point_t p){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height(),bottom=h-19u;
    unsigned rail=bottom-82u,rows=visible_rows(h),total=entry_count();
    oxp_scroll_thumb_t thumb=oxp_scroll_thumb(scroll,total,rows,rail);
    if(!oxp_gesture_inside(p,w-16u,65u,12u,rail))return;
    unsigned y=p.y-65u;
    dragging_thumb=1;
    thumb_original=scroll;
    thumb_grab=(y>=thumb.y&&y<thumb.y+thumb.h)?y-thumb.y:thumb.h/2u;
    scroll=oxp_gesture_scroll_at(p.y,65u,rail,thumb.h,thumb_grab,
                                  oxp_scroll_limit(total,rows));
}
static void scrollbar_move(oxp_gesture_point_t p){
    if(!dragging_thumb||!p.valid)return;
    unsigned h=oxp_canvas_height(),bottom=h>19u?h-19u:0u;
    unsigned rail=bottom>82u?bottom-82u:0u;
    unsigned rows=visible_rows(h),total=entry_count();
    oxp_scroll_thumb_t thumb=oxp_scroll_thumb(thumb_original,total,rows,rail);
    scroll=oxp_gesture_scroll_at(p.y,65u,rail,thumb.h,thumb_grab,
                                  oxp_scroll_limit(total,rows));
}
/* Convert completed small pointer gestures back to the existing click
 * handlers. Long drags are not double-clicks and must not open files. */
static uint32_t file_gesture_event(uint32_t e){
    unsigned k=oxp_gesture_feed(&file_pointer,e);
    if(!k)return e;
    oxp_gesture_point_t p=oxp_gesture_point(e);
    if(k==OXP_GESTURE_DOWN){
        dragging_thumb=dragging_file=drop_highlight=drag_offered=0;
        dragging_path[0]=drop_folder[0]=0;
        if(scrollbar_hit(p)){scrollbar_begin(p);return 0;}
        unsigned side=sidebar_width(oxp_canvas_width());
        int index=oxp_drop_source_row(p.x,p.y,side,oxp_canvas_width(),
                                      oxp_canvas_height(),scroll);
        if(index>=0){
            char source[96];
            if(entry((unsigned)index,source) && !folder(source)){
                dragging_file=1;
                oxp_copy(dragging_path,source,sizeof dragging_path);
            }
        }
        return 0;
    }
    if(k==OXP_GESTURE_MOVE){
        scrollbar_move(p);
        if(dragging_file && p.valid){
            unsigned dx=p.x>file_pointer.start.x?p.x-file_pointer.start.x:
                        file_pointer.start.x-p.x;
            unsigned dy=p.y>file_pointer.start.y?p.y-file_pointer.start.y:
                        file_pointer.start.y-p.y;
            int moved=(dx>5u||dy>5u);
            if(moved && !drag_offered && oxp_doc_program(dragging_path)){
                drag_offered=1;
                offer_cross_window_drag(dragging_path);
            }
            drop_highlight=moved &&
                oxp_drop_documents_target(p.x,p.y,sidebar_width(oxp_canvas_width()),
                                          oxp_canvas_height());
            drop_folder[0]=0;
            if(moved && !drop_highlight){
                int idx=oxp_drop_source_row(p.x,p.y,sidebar_width(oxp_canvas_width()),
                                            oxp_canvas_width(),oxp_canvas_height(),scroll);
                if(idx>=0){
                    char target[96];
                    if(entry((unsigned)idx,target) && folder(target) &&
                       writable_dir(target))
                        oxp_copy(drop_folder,target,sizeof drop_folder);
                }
            }
        }
        return 0;
    }
    if(k==OXP_GESTURE_UP){
        drop_highlight=0;
        if(dragging_file && drop_folder[0]){
            char old[96];oxp_copy(old,cwd,sizeof old);
            oxp_copy(cwd,drop_folder,sizeof cwd);drop_folder[0]=0;
            oxp_file_clip_t payload={0};oxp_clip_file_pack(&payload,dragging_path,0);
            paste_payload(&payload);oxp_copy(cwd,old,sizeof cwd);
            dragging_file=0;return 0;
        }
        if(dragging_file && p.valid){
            unsigned dx=p.x>file_pointer.start.x?p.x-file_pointer.start.x:
                        file_pointer.start.x-p.x;
            unsigned dy=p.y>file_pointer.start.y?p.y-file_pointer.start.y:
                        file_pointer.start.y-p.y;
            if((dx>5u||dy>5u) &&
               oxp_drop_documents_target(p.x,p.y,sidebar_width(oxp_canvas_width()),
                                         oxp_canvas_height())){
                dragging_file=0;
                if(!oxp_drop_should_copy(dragging_path)){
                    oxp_copy(status,"FILE ALREADY IN DOCUMENTS",sizeof status);
                    return 0;
                }
                oxp_file_clip_t dropped={0};
                oxp_clip_file_pack(&dropped,dragging_path,0);
                /* Dragging should not overwrite the user's text clipboard. */
                char old_dir[96];oxp_copy(old_dir,cwd,sizeof old_dir);
                oxp_copy(cwd,"my documents",sizeof cwd);
                paste_payload(&dropped);
                oxp_copy(cwd,old_dir,sizeof cwd);
                return 0;
            }
        }
        dragging_file=0;drop_folder[0]=0;
        if(dragging_thumb){scrollbar_move(p);dragging_thumb=0;return 0;}
        if(scrollbar_hit(file_pointer.start)&&scrollbar_hit(p)){
            unsigned h=oxp_canvas_height(),w=oxp_canvas_width();
            unsigned bottom=h-19u;
            if(oxp_gesture_click(&file_pointer,p,w-18u,48u,16u,17u))scroll_by(-1);
            else if(oxp_gesture_click(&file_pointer,p,w-18u,bottom-17u,16u,17u))scroll_by(1);
            return 0;
        }
        if(!p.valid||!file_pointer.start.valid)return 0;
        unsigned dx=p.x>file_pointer.start.x?p.x-file_pointer.start.x:file_pointer.start.x-p.x;
        unsigned dy=p.y>file_pointer.start.y?p.y-file_pointer.start.y:file_pointer.start.y-p.y;
        int at_start=oxp_pointer_at(&oxp_ui_pointer,file_pointer.start.x+16u,
                                   file_pointer.start.y+43u);
        int at_end=oxp_pointer_at(&oxp_ui_pointer,p.x+16u,p.y+43u);
        if(at_start!=at_end || (at_start<0 && (dx>5u||dy>5u)))return 0;
        return OXP_CLICK|(e&0xfffffu);
    }
    return 0;
}
void oxp_main(void){
    if(oxp_open("My Computer - OpenXP Explorer"))oxp_exit(1);
    oxp_ui_atlas_init_widgets();
    oxp_ui_atlas_init_icons();
    choose(0);
    for(;;){
        render();uint32_t e=oxp_ui_wait_gesture();
        if((e&OXP_POINTER_MASK)){
            e=file_gesture_event(e);
            if(!e)continue;
        }
        if(e==OXP_RESIZE){dragging_thumb=dragging_file=drop_highlight=0;file_pointer.active=0;continue;}
        if(e==OXP_CLOSE||e==27)break;
        if(properties_open){
            if(e==10||e==27||(e&OXP_CLICK))properties_open=0;
            continue;
        }
        if(renaming){
            if(e==10){finish_rename();renaming=0;creating=0;}
            else if(e==8){if(ren_len)rename_buf[--ren_len]=0;}
            else if(e>=32&&e<127&&ren_len+1<sizeof rename_buf){
                char c=(char)e;if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                    (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'){
                    if(c>='A'&&c<='Z')c=(char)(c+32);
                    rename_buf[ren_len++]=c;rename_buf[ren_len]=0;
                }
            }
            continue;
        }
        if(e==OXP_UP){scroll_by(-1);}
        else if(e==OXP_DOWN){scroll_by(1);}
        else if(e==8)parent();
        else if(e==10||e=='o'||e=='O')open_selected();
        else if(e=='d'||e=='D'||e==OXP_KEY_DELETE)delete_selected();
        else if(e==OXP_KEY_COPY || e=='c'||e=='C')copy_selected();
        else if(e==OXP_KEY_CUT)clipboard_selected(1);
        else if(e==OXP_KEY_PASTE)paste_file();
        else if(e=='f'||e=='F'){
            uint64_t state=oxp_storage_flush();
            oxp_copy(status,!(state&OXP_STORAGE_PRESENT)?"RAM ONLY - NO DATA DISK":
                (state&OXP_STORAGE_QUEUED)?"DISK SAVE REQUESTED":"DISK UP TO DATE",sizeof status);
        }
        else if(e=='r'||e=='R')start_rename();
        else if(e=='n'||e=='N')start_folder();
        else if(e=='p'||e=='P')show_properties();
        else if(e==27)context_menu=0;
        else if(e&(OXP_CLICK|OXP_RIGHT_CLICK)){
            if(context_menu){
                unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
                if(h>=142u && oxp_hit(e,menu_x(w),menu_y(h),144,132)){
                    unsigned item=(oxp_click_y(e)-43u-menu_y(h)-5u)/17u;
                    context_menu=0;
                    if(item==0)open_selected();else if(item==1)copy_selected();
                    else if(item==2)clipboard_selected(1);else if(item==3)paste_file();
                    else if(item==4)start_rename();else if(item==5)delete_selected();
                    else if(item==6)show_properties();
                }else context_menu=0;
                continue;
            }
            unsigned fw=oxp_canvas_width(),fh=oxp_canvas_height();
            unsigned fb=fh>19u?fh-19u:0u;
            if(fh>89u && oxp_hit(e,fw-18u,48,16u,fb-48u)){
                if(oxp_click_y(e)-43u<65u)scroll_by(-1);
                else if(oxp_click_y(e)-43u>=fb-17u)scroll_by(1);
                else{
                    unsigned rows=visible_rows(fh),count=entry_count();
                    unsigned max=oxp_scroll_limit(count,rows);
                    unsigned rail=fb>82u?fb-82u:1u;
                    unsigned pos=oxp_click_y(e)-43u-65u;
                    if(pos>rail)pos=rail;
                    scroll=(unsigned)((unsigned long long)pos*max/rail);
                    choose(scroll);
                }
                continue;
            }
            if(oxp_hit(e,8,23,58,21)){back();continue;}
            if(oxp_hit(e,69,23,44,21)){parent();continue;}
            if(fw>=330u && oxp_hit(e,fw-100u,24,94u,21u)){
                start_folder();continue;
            }
            int was_sidebar=0;
            unsigned side=sidebar_width(oxp_canvas_width()),h=oxp_canvas_height();
            for(unsigned i=0;i<6u;++i)if(50u+i*17u+16u<=h-19u &&
                oxp_hit(e,1,50+i*17,side-2u,16)){
                enter_folder(locations[i]);was_sidebar=1;break;
            }
            if(was_sidebar)continue;
            for(unsigned i=0;i<visible_rows(h);++i)if(oxp_hit(e,side+3u,49+i*15,
                oxp_canvas_width()-side-25u,15)){
                char path[96];if(!entry(scroll+i,path))break;
                choose(scroll+i);
                if(e&OXP_RIGHT_CLICK){context_menu=1;double_click.armed=0;}
                else if(oxp_double_click(&double_click,path,oxp_ticks()))open_selected();
                break;
            }
        }
    }
    oxp_exit(0);
}
