#include "user/app_launch.h"
#include "arch/x86_64/irq_state.h"
static char request[48];
static bool pending;
static bool equal(const char *a,const char *b){
    size_t i=0;while(a[i]&&b[i]&&i<47){if(a[i]!=b[i])return false;++i;}
    return i<47 && !a[i]&&!b[i];
}
bool app_launch_allowed(const char *p){
    static const char *const names[]={"bin/notepad","bin/calculator","bin/clock",
       "bin/canvas","bin/calendar","bin/snake","bin/mines","bin/monitor",
       "bin/about","bin/paint","bin/wordpad","bin/terminal",
       "bin/filemanager","bin/run","bin/settings"};
    if(!p)return false;
    for(size_t i=0;i<sizeof names/sizeof names[0];++i)
        if(equal(p,names[i]))return true;
    return false;
}
void app_launch_reset(void){uint64_t f=irq_save_disable();pending=false;request[0]=0;irq_restore(f);}
bool app_launch_submit(const char *path){
    if(!app_launch_allowed(path))return false;
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
