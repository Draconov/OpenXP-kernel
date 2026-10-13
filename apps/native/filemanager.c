#include "oxp.h"
/* Kernel validates paths and limits writes to HOME. No real directories yet. */
static app_file_request_t req;
static char selected[48],preview[100],status[48]="SELECT A FILE";
static char rename_buf[33];
static unsigned scroll,ren_len;
static int renaming;
static int is_home(const char *s){return s[0]=='h'&&s[1]=='o'&&s[2]=='m'&&s[3]=='e'&&s[4]=='/';}
static void choose(unsigned index){
    req=(app_file_request_t){0};req.index=index;
    if(oxp_file(APP_FS_LIST,&req))return;
    oxp_copy(selected,req.path,sizeof selected);
    req=(app_file_request_t){0};oxp_copy(req.path,selected,sizeof req.path);
    if(!oxp_file(APP_FS_READ,&req)){
        unsigned at=0;for(unsigned i=0;i<req.size && at+1<sizeof preview;++i){
            uint8_t c=req.data[i];if(c=='\n')c=' ';
            if(c>=32&&c<=126)preview[at++]=(char)c;
        }preview[at]=0;oxp_copy(status,"PREVIEW LOADED",sizeof status);
    }else{preview[0]=0;oxp_copy(status,"BINARY OR LARGE FILE",sizeof status);}
}
static void open_selected(void){
    if(selected[0]=='b'&&selected[1]=='i'&&selected[2]=='n'&&selected[3]=='/'){
        oxp_copy(status,oxp_launch(selected)==0?"LAUNCH REQUESTED":"UNKNOWN OR BUSY",sizeof status);
    }else oxp_copy(status,"TEXT PREVIEW BELOW",sizeof status);
}
static void copy_selected(void){
    if(!selected[0])return;
    req=(app_file_request_t){0};oxp_copy(req.path,selected,sizeof req.path);
    const char *leaf=selected;
    for(unsigned i=0;selected[i];++i)if(selected[i]=='/')leaf=selected+i+1;
    oxp_copy(req.dest,"home/",sizeof req.dest);
    unsigned n=5,j=0;
    while(leaf[j]&&n+6<sizeof req.dest)req.dest[n++]=leaf[j++];
    if(is_home(selected)){
        const char suffix[]=".copy";
        for(unsigned k=0;suffix[k] && n+1<sizeof req.dest;++k)req.dest[n++]=suffix[k];
    }
    req.dest[n]=0;
    oxp_copy(status,oxp_file(APP_FS_COPY,&req)==0?"COPIED TO HOME":"COPY FAILED",sizeof status);
}
static void delete_selected(void){
    if(!is_home(selected)){oxp_copy(status,"BOOT FILE IS READ ONLY",sizeof status);return;}
    req=(app_file_request_t){0};oxp_copy(req.path,selected,sizeof req.path);
    int ok=oxp_file(APP_FS_DELETE,&req)==0;
    oxp_copy(status,ok?"FILE DELETED":"DELETE FAILED",sizeof status);
    if(ok){selected[0]=0;preview[0]=0;scroll=0;}
}
static void start_rename(void){
    if(!is_home(selected)){oxp_copy(status,"RENAME: HOME FILES ONLY",sizeof status);return;}
    renaming=1;ren_len=0;rename_buf[0]=0;
    oxp_copy(status,"TYPE NEW NAME + ENTER",sizeof status);
}
static void finish_rename(void){
    if(!ren_len)return;
    req=(app_file_request_t){0};oxp_copy(req.path,selected,sizeof req.path);
    oxp_copy(req.dest,"home/",sizeof req.dest);
    unsigned i=0;while(i<ren_len && i+6<sizeof req.dest){
        req.dest[i+5]=rename_buf[i];++i;
    }req.dest[i+5]=0;
    int ok=oxp_file(APP_FS_RENAME,&req)==0;
    oxp_copy(status,ok?"RENAMED":"RENAME FAILED",sizeof status);
    if(ok){oxp_copy(selected,req.dest,sizeof selected);preview[0]=0;}
}
static void render(void){
    oxp_draw_cmd_t shapes[]={{OXP_DRAW_CLEAR,0,0,0,0,0xecf4ff},
        {OXP_DRAW_FILL,7,27,306,115,0xffffff},
        {OXP_DRAW_FILL,7,144,306,31,0xd7e6f8}};
    oxp_draw(shapes,3);oxp_label(10,8,"MY COMPUTER / BOOT + HOME",1,0x1f487b);
    for(unsigned i=0;i<9;++i){req=(app_file_request_t){0};req.index=scroll+i;
        if(oxp_file(APP_FS_LIST,&req))break;
        if(selected[0]){unsigned j=0;while(selected[j]&&selected[j]==req.path[j])++j;
            if(!selected[j]&&!req.path[j])oxp_fill(9,30+i*12,291,12,0xc4dafa);}
        oxp_label(13,32+i*12,req.path,1,0x214971);
    }
    oxp_label(274,8,"UP DN",1,0x214971);
    oxp_label(11,148,renaming?"NEW NAME:":status,1,0x194b7d);
    oxp_label(11,160,renaming?rename_buf:preview,1,0x425c7a);
    if(!renaming)oxp_label(104,148,"C COPY R REN D DEL",1,0x194b7d);
}
void oxp_main(void){
    if(oxp_open("My Computer - native ELF64"))oxp_exit(1);
    choose(0);
    for(;;){render();uint32_t e=oxp_wait(0);if(e==OXP_CLOSE||e==27)break;
        if(renaming){
            if(e==10){finish_rename();renaming=0;}
            else if(e==8){if(ren_len)rename_buf[--ren_len]=0;}
            else if(e>=32&&e<127&&ren_len+1<sizeof rename_buf){
                char c=(char)e;
                if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                   (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'){
                    if(c>='A'&&c<='Z')c+=32;
                    rename_buf[ren_len++]=c;rename_buf[ren_len]=0;
                }
            }
            continue;
        }
        if(e==OXP_UP){if(scroll)--scroll;}
        else if(e==OXP_DOWN){if(scroll<30)++scroll;}
        else if(e==10||e=='o'||e=='O')open_selected();
        else if(e=='d'||e=='D')delete_selected();
        else if(e=='c'||e=='C')copy_selected();
        else if(e=='r'||e=='R')start_rename();
        else if(e&OXP_CLICK){
            if(oxp_hit(e,269,0,51,25)){if(scroll<30)++scroll;continue;}
            for(unsigned i=0;i<9;++i)if(oxp_hit(e,9,30+i*12,291,12)){
                choose(scroll+i);break;
            }
        }
    }oxp_exit(0);
}
