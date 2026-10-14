#include "user/app_launch.h"
#include "arch/x86_64/irq_state.h"
#include "fs/app_catalog.h"
static char request[48];
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
void app_launch_reset(void){uint64_t f=irq_save_disable();pending=false;request[0]=0;irq_restore(f);}
bool app_launch_submit(const char *path){
    path=target(path);
    if(!path)return false;
    uint64_t flags=irq_save_disable();
    if(pending){irq_restore(flags);return false;}
    size_t i=0;do{request[i]=path[i];}while(path[i++]&&i<sizeof request);
    pending=true;irq_restore(flags);return true;
}
bool app_launch_take(char *dest,size_t cap){
    if(!dest||cap<sizeof request)return false;
    uint64_t flags=irq_save_disable();
    if(!pending){irq_restore(flags);return false;}
    size_t i=0;do{dest[i]=request[i];}while(request[i++]&&i<sizeof request);
    pending=false;request[0]=0;irq_restore(flags);return true;
}
