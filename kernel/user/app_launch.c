#include "user/app_launch.h"
#include "arch/x86_64/irq_state.h"
#include "fs/app_catalog.h"
static app_launch_request_t request;
static bool pending;
static bool equal(const char *a,const char *b){
    size_t i=0;while(a[i]&&b[i]&&i<47){if(a[i]!=b[i])return false;++i;}
    return i<47 && !a[i]&&!b[i];
}
/* User-visible .exe entries refer to the exact allowlisted OpenXP ELF files.
 * A foreign PE executable is never accepted merely because it ends in .exe. */
static const char *target(const char *path){
    static const struct {const char *display; const char *native;} names[]={
#define APP_LAUNCH_ENTRY(display, native) {display,native},
        OPENXP_APP_CATALOG(APP_LAUNCH_ENTRY)
#undef APP_LAUNCH_ENTRY
    };
    if(!path)return NULL;
    for(size_t i=0;i<sizeof names/sizeof names[0];++i){
        if(equal(path,names[i].display)||equal(path,names[i].native))return names[i].native;
    }
    return NULL;
}
bool app_launch_allowed(const char *p){return target(p)!=NULL;}
void app_launch_reset(void){uint64_t f=irq_save_disable();pending=false;request=(app_launch_request_t){0};irq_restore(f);}
/* Validate data path syntax independently from the executable allowlist.
 * No traversal, embedded controls, or malformed paths enter the queue. */
static bool valid_argument(const char *s){
    if(!s)return true;
    size_t n=0,part=0;
    for(;s[n];++n){
        unsigned char ch=(unsigned char)s[n];
        if(n+1>=APP_LAUNCH_ARG_MAX || ch<33u || ch>126u ||
           ch=='\\' || ch==':')return false;
        if(ch=='/'){
            if(part==0 || (part==1 && s[n-1]=='.') ||
               (part==2 && s[n-2]=='.' && s[n-1]=='.'))return false;
            part=0;
        }else ++part;
    }
    return n>0 && part>0 &&
           !(part==1 && s[n-1]=='.') &&
           !(part==2 && s[n-2]=='.' && s[n-1]=='.');
}
bool app_launch_submit_with_arg(const char *path,const char *argument){
    path=target(path);
    if(!path || (argument && !valid_argument(argument)))return false;
    uint64_t flags=irq_save_disable();
    if(pending){irq_restore(flags);return false;}
    request=(app_launch_request_t){0};
    size_t i=0;do{request.executable[i]=path[i];}while(path[i++]&&i<sizeof request.executable);
    if(argument){i=0;do{request.argument[i]=argument[i];}while(argument[i++]&&i<sizeof request.argument);}
    pending=true;irq_restore(flags);return true;
}
bool app_launch_submit(const char *path){return app_launch_submit_with_arg(path,NULL);}
bool app_launch_take_request(app_launch_request_t *dest){
    if(!dest)return false;
    uint64_t flags=irq_save_disable();
    if(!pending){irq_restore(flags);return false;}
    *dest=request;pending=false;request=(app_launch_request_t){0};
    irq_restore(flags);return true;
}
bool app_launch_take(char *dest,size_t cap){
    if(!dest||cap<APP_LAUNCH_PATH_MAX)return false;
    app_launch_request_t r;
    if(!app_launch_take_request(&r))return false;
    size_t i=0;do{dest[i]=r.executable[i];}while(r.executable[i++]&&i<APP_LAUNCH_PATH_MAX);
    return true;
}
