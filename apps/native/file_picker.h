#ifndef OPENXP_FILE_PICKER_H
#define OPENXP_FILE_PICKER_H
#include "oxp.h"
#include "file_workflow.h"
#include "ui_widgets.h"
#include "advanced_input.h"
/* Modal, bounded Open/Save-As picker shared by Ring 3 editors. Paths remain
 * inside the existing VFS. SAVE AS can only target the writable HOME store. */
typedef struct {
    int active,save,confirm,docs,filename_focus;
    unsigned selection,scroll;
    char filename[48],picked[96],error[50];
    oxp_gesture_t gesture;
    oxp_double_click_t double_click;
} oxp_picker_t;
static inline void oxp_picker_begin(oxp_picker_t *p,int save,const char *path){
    *p=(oxp_picker_t){0};p->active=1;p->save=!!save;
    p->filename_focus=save!=0;
    const char *name=oxp_doc_leaf(path);
    oxp_copy(p->filename,name,sizeof p->filename);
    if(save && (!p->filename[0] || oxp_len(p->filename)>40u))
        oxp_copy(p->filename,"untitled.txt",sizeof p->filename);
    oxp_copy(p->error,"HOME FILES",sizeof p->error);
}
static inline const char *oxp_picker_dir(const oxp_picker_t *p){
    return p->docs?"docs":"home";
}
static inline int oxp_picker_entry(const oxp_picker_t *p,unsigned index,char out[96]){
    app_file_request_t req={0};
    req.index=index;req.dest[0]='D';
    oxp_copy(req.path,oxp_picker_dir(p),sizeof req.path);
    if(oxp_file(APP_FS_LIST,&req))return 0;
    oxp_copy(out,req.path,96);return 1;
}
static inline unsigned oxp_picker_count(const oxp_picker_t *p){
    char name[96];unsigned n=0;
    while(n<64u&&oxp_picker_entry(p,n,name))++n;
    return n;
}
static inline void oxp_picker_layout(unsigned *x,unsigned *y,unsigned *w,unsigned *h){
    unsigned cw=oxp_canvas_width(),ch=oxp_canvas_height();
    *w=cw>312u?300u:(cw>20u?cw-12u:cw);
    *h=ch>178u?172u:(ch>20u?ch-10u:ch);
    *x=(cw-*w)/2u;*y=(ch-*h)/2u;
}
static inline unsigned oxp_picker_rows(unsigned height){
    return height>85u?(height-85u)/15u:0u;
}
static inline void oxp_picker_render(oxp_picker_t *p){
    if(!p->active)return;
    unsigned x,y,w,h;oxp_picker_layout(&x,&y,&w,&h);
    oxp_ui_menu((oxp_ui_rect){x,y,w,h});
    oxp_fill(x+3u,y+3u,w>6u?w-6u:0u,18u,oxp_theme_accent(oxp_ui_theme));
    oxp_ui_text((oxp_ui_rect){x+8u,y+2u,w>14u?w-14u:0u,17u},
                p->save?"SAVE DOCUMENT AS":"OPEN DOCUMENT",y+7u,0xffffff,0);
    if(h<92u||w<136u)return;
    oxp_ui_toolbutton((oxp_ui_rect){x+7u,y+23u,57u,18u},"HOME",p->docs==0);
    oxp_ui_toolbutton((oxp_ui_rect){x+67u,y+23u,57u,18u},"DOCS",p->docs!=0);
    unsigned rows=oxp_picker_rows(h),count=oxp_picker_count(p);
    if(rows>8u)rows=8u;
    if(p->scroll>count)p->scroll=count;
    for(unsigned i=0;i<rows;++i){
        char path[96];if(!oxp_picker_entry(p,p->scroll+i,path))break;
        unsigned ly=y+43u+i*15u;
        if(p->scroll+i==p->selection)oxp_fill(x+6u,ly,w-12u,14u,
                                                   oxp_theme_selection(oxp_ui_theme));
        oxp_ui_text((oxp_ui_rect){x+10u,ly,w-20u,14u},
                    oxp_doc_leaf(path),ly+3u,oxp_ui_ink(),0);
    }
    unsigned by=y+h-41u;
    oxp_fill(x+6u,by,w-12u,17u,p->filename_focus?0xfffaf0u:0xffffffu);
    oxp_ui_text((oxp_ui_rect){x+10u,by+1u,w-20u,14u},
                p->filename,by+5u,oxp_ui_ink(),0);
    oxp_ui_toolbutton((oxp_ui_rect){x+w-115u,y+h-21u,54u,18u},
                      p->confirm?"REPLACE":p->save?"SAVE":"OPEN",0);
    oxp_ui_toolbutton((oxp_ui_rect){x+w-57u,y+h-21u,53u,18u},"CANCEL",0);
    oxp_ui_text((oxp_ui_rect){x+7u,y+h-21u,w>125u?w-125u:0u,17u},
                p->error,y+h-16u,oxp_ui_soft_ink(),0);
}
static inline int oxp_picker_valid_name(const char *s){
    unsigned n=0;
    while(s[n]){
        unsigned char c=(unsigned char)s[n++];
        if(n>40u || c<33||c>126||c=='/'||c=='\\'||c==':'||c=='*'||c=='?')return 0;
    }
    return n>0u && !(n==1u&&s[0]=='.') &&
           !(n==2u&&s[0]=='.'&&s[1]=='.');
}
static inline int oxp_picker_accept(oxp_picker_t *p,oxp_filetype_t expected){
    if(!oxp_picker_valid_name(p->filename)){
        oxp_copy(p->error,"INVALID NAME",sizeof p->error);return 0;
    }
    char path[96];const char *dir=p->save?"home/":p->docs?"docs/":"home/";
    unsigned n=0;while(dir[n]){path[n]=dir[n];++n;}
    for(unsigned j=0;p->filename[j]&&n+1u<sizeof path;++j)
        path[n++]=p->filename[j];
    path[n]=0;
    oxp_filetype_t actual=oxp_filetype(path,0);
    if(actual!=expected && !(expected==OXP_FT_TEXT&&actual==OXP_FT_CONFIG)){
        oxp_copy(p->error,"WRONG FILE TYPE",sizeof p->error);return 0;
    }
    app_file_request_t req={0};oxp_copy(req.path,path,sizeof req.path);
    int64_t kind=oxp_file(APP_FS_KIND,&req);
    if(p->save){
        if(kind!=0 && !p->confirm){p->confirm=1;
            oxp_copy(p->error,"CONFIRM REPLACE",sizeof p->error);return 0;}
    }else if(kind!=1 && kind!=2){
        oxp_copy(p->error,"FILE NOT FOUND",sizeof p->error);return 0;
    }
    oxp_copy(p->picked,path,sizeof p->picked);p->active=0;return 1;
}
/* Returns +1 on accepted path, -1 on cancelled, 0 while modal. */
static inline int oxp_picker_event(oxp_picker_t *p,uint32_t e,oxp_filetype_t type){
    if(!p->active)return 0;
    if(e==OXP_CLOSE||e==27){p->active=0;return -1;}
    if(e==OXP_RESIZE){p->gesture.active=0;return 0;}
    if(e==OXP_UP){if(p->selection)p->selection--;if(p->selection<p->scroll)p->scroll=p->selection;return 0;}
    if(e==OXP_DOWN){unsigned count=oxp_picker_count(p);
        if(p->selection+1u<count)++p->selection;
        unsigned x,y,w,h;oxp_picker_layout(&x,&y,&w,&h);
        unsigned rows=oxp_picker_rows(h);if(rows>8u)rows=8u;
        if(rows && p->selection>=p->scroll+rows)p->scroll=p->selection-rows+1u;
        return 0;
    }
    if(e==9 || e==OXP_SHIFT_TAB){p->filename_focus=!p->filename_focus;return 0;}
    if(p->filename_focus&&e==OXP_KEY_SELECT_ALL){p->filename[0]=0;p->confirm=0;return 0;}
    if(e==8){unsigned n=(unsigned)oxp_len(p->filename);
        if(p->filename_focus&&n){p->filename[n-1u]=0;p->confirm=0;}return 0;}
    if(e==10)return oxp_picker_accept(p,type);
    if(p->filename_focus && e>=33u&&e<=126u){
        unsigned n=(unsigned)oxp_len(p->filename);
        char c=(char)e;
        if(n+1u<sizeof p->filename && c!='/'&&c!='\\'&&c!=':'&&c!='*'&&c!='?'){
            p->filename[n]=c;p->filename[n+1u]=0;p->confirm=0;
        }return 0;
    }
    /* The host may send legacy CLICK or the modern capture/release pair. */
    if(!(e&(OXP_CLICK|OXP_POINTER_MASK)))return 0;
    unsigned x,y,w,h;oxp_picker_layout(&x,&y,&w,&h);
    if(h<92u||w<136u)return 0;
    unsigned rows=oxp_picker_rows(h);if(rows>8u)rows=8u;
    if(!(e&OXP_CLICK)){
        unsigned k=oxp_gesture_feed(&p->gesture,e);
        if(k!=OXP_GESTURE_UP)return 0;
        oxp_gesture_point_t q=oxp_gesture_point(e);
        if(!q.valid||!p->gesture.start.valid)return 0;
        unsigned dx=q.x>p->gesture.start.x?q.x-p->gesture.start.x:p->gesture.start.x-q.x;
        unsigned dy=q.y>p->gesture.start.y?q.y-p->gesture.start.y:p->gesture.start.y-q.y;
        if(dx>5u||dy>5u)return 0;
    }
    oxp_gesture_point_t q=oxp_gesture_point(e);
    if(!q.valid)return 0;
    if(oxp_gesture_inside(q,x+7u,y+23u,57u,18u)){
        p->docs=0;p->selection=p->scroll=0;p->confirm=0;return 0;
    }
    if(oxp_gesture_inside(q,x+67u,y+23u,57u,18u)){
        if(!p->save){p->docs=1;p->selection=p->scroll=0;p->confirm=0;}
        return 0;
    }
    if(q.x>=x+6u&&q.x<x+w-6u&&q.y>=y+43u&&q.y<y+43u+rows*15u){
        unsigned idx=p->scroll+(q.y-y-43u)/15u;
        char path[96];
        if(oxp_picker_entry(p,idx,path)){
            p->selection=idx;oxp_copy(p->filename,oxp_doc_leaf(path),sizeof p->filename);
            p->filename_focus=0;p->confirm=0;
            if(oxp_double_click(&p->double_click,path,oxp_ticks()))
                return oxp_picker_accept(p,type);
        }return 0;
    }
    if(oxp_gesture_inside(q,x+6u,y+h-41u,w-12u,17u)){
        p->filename_focus=1;p->confirm=0;return 0;
    }
    if(oxp_gesture_inside(q,x+w-115u,y+h-21u,54u,18u))return oxp_picker_accept(p,type);
    if(oxp_gesture_inside(q,x+w-57u,y+h-21u,53u,18u)){p->active=0;return -1;}
    return 0;
}
#endif
