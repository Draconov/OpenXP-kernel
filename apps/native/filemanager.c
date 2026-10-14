#include "oxp.h"
#include "filetypes.h"
#include "wm_protocol.h"
/* XP-inspired Explorer for the bounded OpenXP VFS. System files are read-only.
 * Context actions do not bypass the kernel executable allowlist or RAMFS ACL. */
static app_file_request_t req;
static char cwd[96],previous[96],selected[96],preview[78],status[74]="LOCAL DISK (C:)";
static char rename_buf[33];
static unsigned scroll,ren_len;
static int renaming,context_menu;
static int home(const char *p){return p[0]=='h'&&p[1]=='o'&&p[2]=='m'&&p[3]=='e'&&p[4]=='/';}
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
    app_file_request_t r={0};oxp_copy(r.path,"home/open-document.cfg",sizeof r.path);
    r.size=0;while(path[r.size]&&r.size<95u){r.data[r.size]=(uint8_t)path[r.size];++r.size;}
    if(oxp_file(APP_FS_WRITE,&r)!=0){oxp_copy(status,"CANNOT PREPARE DOCUMENT",sizeof status);return;}
    oxp_filetype_t type=oxp_filetype(path,0);
    const char *program=oxp_filetype_app(type);
    oxp_copy(status,program && oxp_launch(program)==0?"OPENING DOCUMENT":"APPLICATION UNAVAILABLE",sizeof status);
    if(!program){(void)oxp_file(APP_FS_DELETE,&r);}
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
static void open_selected(void){
    context_menu=0;if(!selected[0])return;
    if(folder(selected)){enter_folder(selected);return;}
    oxp_filetype_t type=oxp_filetype(selected,0);
    if(type==OXP_FT_EXE || (selected[0]=='b'&&selected[1]=='i'&&selected[2]=='n'&&selected[3]=='/')){
        oxp_copy(status,oxp_launch(selected)==0?"APPLICATION LAUNCH QUEUED":"NOT AN OPENXP EXECUTABLE",sizeof status);
    }else if(type==OXP_FT_BITMAP){set_wallpaper(selected);}
    else if(oxp_filetype_app(type))handoff(selected);
    else oxp_copy(status,"NO ASSOCIATED APPLICATION",sizeof status);
}
static void copy_selected(void){
    context_menu=0;if(!selected[0]||folder(selected))return;
    req=(app_file_request_t){0};oxp_copy(req.path,selected,sizeof req.path);
    const char *part=leaf(selected);oxp_copy(req.dest,"home/",sizeof req.dest);
    unsigned n=5,j=0;while(part[j]&&n+7<sizeof req.dest)req.dest[n++]=part[j++];
    if(home(selected)){const char suffix[]=".copy";
        for(unsigned k=0;suffix[k]&&n+1<sizeof req.dest;++k)req.dest[n++]=suffix[k];}
    req.dest[n]=0;
    oxp_copy(status,oxp_file(APP_FS_COPY,&req)==0?"FILE COPIED TO HOME":"COPY FAILED / FILE TOO LARGE",sizeof status);
}
static void delete_selected(void){
    context_menu=0;if(!home(selected)){oxp_copy(status,"SYSTEM FILE IS READ ONLY",sizeof status);return;}
    req=(app_file_request_t){0};oxp_copy(req.path,selected,sizeof req.path);
    int success=oxp_file(APP_FS_DELETE,&req)==0;
    oxp_copy(status,success?"FILE DELETED":"DELETE FAILED",sizeof status);
    if(success){selected[0]=0;preview[0]=0;scroll=0;}
}
static void start_rename(void){
    context_menu=0;
    if(!home(selected)){oxp_copy(status,"RENAME: HOME FILES ONLY",sizeof status);return;}
    renaming=1;ren_len=0;rename_buf[0]=0;
    oxp_copy(status,"TYPE NEW NAME + ENTER",sizeof status);
}
static void finish_rename(void){
    if(!ren_len)return;
    req=(app_file_request_t){0};oxp_copy(req.path,selected,sizeof req.path);
    oxp_copy(req.dest,"home/",sizeof req.dest);
    unsigned i=0;while(i<ren_len && i+6<sizeof req.dest){req.dest[i+5]=rename_buf[i];++i;}
    req.dest[i+5]=0;
    int ok=oxp_file(APP_FS_RENAME,&req)==0;
    oxp_copy(status,ok?"FILE RENAMED":"RENAME FAILED",sizeof status);
    if(ok){oxp_copy(selected,req.dest,sizeof selected);preview[0]=0;}
}
static void draw_icon(unsigned x,unsigned y,oxp_filetype_t t){
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
    "WALLPAPERS","PROGRAM FILES","SYSTEM32"};
static unsigned sidebar_width(unsigned w){return w>=292u?91u:78u;}
static unsigned visible_rows(unsigned h){
    unsigned rows=h>70u?(h-69u)/15u:0u;
    return rows>7u?7u:rows;
}
static unsigned menu_x(unsigned w){return w>154u?w-153u:0u;}
static unsigned menu_y(unsigned h){return h>100u?h-100u:0u;}
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    unsigned side=sidebar_width(w),bottom=h>19u?h-19u:0u;
    oxp_draw_cmd_t b[]={{OXP_DRAW_CLEAR,0,0,0,0,0xf3f7fd},
        {OXP_DRAW_FILL,0,0,w,21,0x2e6caf},
        {OXP_DRAW_FILL,0,21,w,27,0xe5edf8},
        {OXP_DRAW_FILL,0,48,side-1,h>67u?h-67u:1u,0xdceafa},
        {OXP_DRAW_FILL,side,48,w-side,h>67u?h-67u:1u,0xffffff},
        {OXP_DRAW_FILL,0,bottom,w,h-bottom,0xd8e8f8}};
    oxp_draw(b,6);
    oxp_label(9,7,"MY COMPUTER",1,0xffffff);
    oxp_label(8,27,"< BACK",1,0x1f4b83);
    oxp_label(70,27,"^ UP",1,0x1f4b83);
    oxp_label(117,27,"C:\\",1,0x1f4b83);
    oxp_label(137,27,cwd[0]?cwd:"LOCAL DISK",1,0x1f4b83);
    for(unsigned i=0;i<6u;++i){
        unsigned y=50u+i*17u;
        if(y+16u>bottom)break;
        if(equal(cwd,locations[i]))oxp_fill(1,y,side-2u,16,0xbad3f1);
        oxp_label(7,y+3u,location_names[i],1,0x245585);
    }
    unsigned rows=visible_rows(h);
    for(unsigned i=0;i<rows;++i){
        char path[96];if(!entry(scroll+i,path))break;
        int dir=folder(path);
        oxp_filetype_t t=oxp_filetype(path,dir);
        unsigned y=49u+i*15u;
        if(equal(selected,path))oxp_fill(side+3u,y,w-side-7u,15,0xc5ddfa);
        draw_icon(side+7u,y+1u,t);
        oxp_label(side+23u,y+4u,leaf(path),1,0x254c78);
    }
    if(h>=27u){
        if(renaming){oxp_label(5,bottom+4u,"NEW NAME:",1,0x225583);
            oxp_label(65,bottom+4u,rename_buf,1,0x225583);}
        else oxp_label(5,bottom+4u,status,1,0x285883);
    }
    if(context_menu && h>=105u){
        unsigned x=menu_x(w),y=menu_y(h);
        oxp_fill(x+3u,y+3u,144,94,0xd0d9e8);oxp_fill(x,y,144,93,0xffffff);
        const char *actions[]={"OPEN","COPY TO HOME","RENAME","DELETE","PROPERTIES"};
        for(unsigned i=0;i<5u;++i)oxp_label(x+10u,y+5u+i*17u,actions[i],1,0x225582);
    }
}
void oxp_main(void){
    if(oxp_open("My Computer - OpenXP Explorer"))oxp_exit(1);
    choose(0);
    for(;;){
        render();uint32_t e=oxp_wait(0);if(e==OXP_CLOSE||e==27)break;
        if(renaming){
            if(e==10){finish_rename();renaming=0;}
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
        if(e==OXP_UP){if(scroll)--scroll;choose(scroll);}
        else if(e==OXP_DOWN){char path[96];if(entry(scroll+1,path)){++scroll;choose(scroll);}}
        else if(e==8)parent();
        else if(e==10||e=='o'||e=='O')open_selected();
        else if(e=='d'||e=='D')delete_selected();
        else if(e=='c'||e=='C')copy_selected();
        else if(e=='r'||e=='R')start_rename();
        else if(e==27)context_menu=0;
        else if(e&(OXP_CLICK|OXP_RIGHT_CLICK)){
            if(context_menu){
                unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
                if(h>=105u && oxp_hit(e,menu_x(w),menu_y(h),144,93)){
                    unsigned item=(oxp_click_y(e)-43u-menu_y(h)-5u)/17u;
                    context_menu=0;
                    if(item==0)open_selected();else if(item==1)copy_selected();
                    else if(item==2)start_rename();else if(item==3)delete_selected();
                    else if(item==4)oxp_copy(status,oxp_filetype_label(oxp_filetype(selected,folder(selected))),sizeof status);
                }else context_menu=0;
                continue;
            }
            if(oxp_hit(e,8,23,58,21)){back();continue;}
            if(oxp_hit(e,69,23,44,21)){parent();continue;}
            int was_sidebar=0;
            unsigned side=sidebar_width(oxp_canvas_width()),h=oxp_canvas_height();
            for(unsigned i=0;i<6u;++i)if(50u+i*17u+16u<=h-19u &&
                oxp_hit(e,1,50+i*17,side-2u,16)){
                enter_folder(locations[i]);was_sidebar=1;break;
            }
            if(was_sidebar)continue;
            for(unsigned i=0;i<visible_rows(h);++i)if(oxp_hit(e,side+3u,49+i*15,
                oxp_canvas_width()-side-7u,15)){
                char path[96];if(!entry(scroll+i,path))break;
                int old=equal(path,selected);
                choose(scroll+i);
                if(e&OXP_RIGHT_CLICK)context_menu=1;
                else if(old)open_selected();
                break;
            }
        }
    }
    oxp_exit(0);
}
