#include "fs/vfs.h"
#include "fs/initrd.h"
#include "fs/ramfs.h"
#include "fs/app_catalog.h"
#include <stdint.h>

/* The XP-inspired namespace is metadata over the existing boot archive.
 * Native programs remain ELF64; .exe is a convenient OpenXP alias, NOT PE.
 * No kernel executable memory is copied or duplicated for these aliases. */
static const char *const dirs[] = {
    "windows", "windows/system32", "windows/system32/drivers",
    "windows/fonts", "windows/web", "windows/web/wallpaper",
    "windows/temp", "windows/resources",
    "program files", "program files/openxp", "program files/common files",
    "documents and settings", "documents and settings/user",
    "documents and settings/user/desktop", "documents and settings/user/my documents",
    "documents and settings/all users", "documents and settings/all users/start menu",
    "my documents", "my pictures", "my music", "my videos",
    "home", "bin", "docs", "temp", "recycler", "system apps"
};
typedef struct {const char *alias; const char *target;} vfs_alias_t;
#define VFS_APP_ENTRY(name, source) {name, source},
static const vfs_alias_t programs[] = {
    OPENXP_APP_CATALOG(VFS_APP_ENTRY)
};
#undef VFS_APP_ENTRY
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static bool equal(const char *a,const char *b) {
    if (!a || !b)return false;
    while (*a && *b && *a==*b){++a;++b;}return !*a && !*b;
}
static bool home_path(const char *p) {
    return p && p[0]=='h' && p[1]=='o' && p[2]=='m' && p[3]=='e' && p[4]=='/' && p[5];
}
static bool copy_name(char *dst,size_t cap,const char *src) {
    if(!dst || !cap || !src)return false;
    size_t n=0;while(src[n]){if(n+1>=cap)return false;++n;}
    for(size_t i=0;i<=n;++i)dst[i]=src[i];
    return true;
}
/* All user paths are strictly relative; never normalize '..' into a privileged path. */
static bool safe_path(const char *path){
    if(!path || !*path || *path=='/' || *path=='\\')return false;
    size_t segment=0;
    for(size_t i=0;path[i];++i){
        char c=path[i];
        if(i>=95 || c=='\\' || (unsigned char)c<32)return false;
        if(c=='/'){
            if(i==segment || (i-segment==1 && path[segment]=='.') ||
               (i-segment==2 && path[segment]=='.' && path[segment+1]=='.'))return false;
            segment=i+1;
        }
    }
    size_t n=0;while(path[n])++n;
    return n!=segment && !(n-segment==1 && path[segment]=='.') &&
           !(n-segment==2 && path[segment]=='.' && path[segment+1]=='.');
}
/* Aliased documents are backed by the small writable HOME filesystem. */
static const char *home_alias(const char *path,char *out,size_t cap){
    static const char *const roots[]={"my documents/", "documents and settings/user/my documents/"};
    if(home_path(path))return path;
    for(size_t i=0;i<COUNT(roots);++i){
        size_t j=0;while(roots[i][j] && path[j]==roots[i][j])++j;
        if(!roots[i][j] && path[j]){
            if(!copy_name(out,cap,"home/"))return NULL;
            size_t at=5;
            while(path[j]){if(at+1>=cap)return NULL;out[at++]=path[j++];}
            out[at]=0;return out;
        }
    }
    return NULL;
}
const char *vfs_executable_target(const char *path){
    if(!safe_path(path))return NULL;
    for(size_t i=0;i<COUNT(programs);++i)
        if(equal(path,programs[i].alias))return programs[i].target;
    return NULL;
}
size_t vfs_count(void){return COUNT(dirs)+COUNT(programs)+initrd_count()+ramfs_count();}
bool vfs_name_at(size_t index,char *name,size_t cap){
    if(index<COUNT(dirs))return copy_name(name,cap,dirs[index]);
    index-=COUNT(dirs);
    if(index<COUNT(programs))return copy_name(name,cap,programs[index].alias);
    index-=COUNT(programs);
    if(index<initrd_count())return initrd_name_at(index,name,cap);
    return ramfs_name_at(index-initrd_count(),name,cap);
}
vfs_kind_t vfs_kind(const char *path){
    if(!safe_path(path))return VFS_MISSING;
    for(size_t i=0;i<COUNT(dirs);++i)if(equal(path,dirs[i]))return VFS_DIRECTORY;
    char backing[48];const char *p=home_alias(path,backing,sizeof(backing));
    if(p){
        for(size_t i=0;i<ramfs_count();++i){
            char name[RAMFS_NAME_CAPACITY];
            if(ramfs_name_at(i,name,sizeof(name)) && equal(name,p))return VFS_HOME_WRITABLE;
        }
        return VFS_MISSING;
    }
    const char *source=vfs_executable_target(path);
    if(source)path=source;
    const void *data;size_t n;
    return initrd_lookup(path,&data,&n)?VFS_BOOT_READONLY:VFS_MISSING;
}
static bool child_of(const char *parent,const char *name) {
    size_t i=0;
    if(parent && *parent){
        while(parent[i] && parent[i]==name[i])++i;
        if(parent[i] || name[i]!='/')return false;
        ++i;
    }
    if(!name[i])return false;
    for(;name[i];++i)if(name[i]=='/')return false;
    return true;
}
/* Enumerate direct children, not every file from the entire archive. Paths are
 * full relative paths; callers can inspect kind via vfs_kind(). */
bool vfs_list_dir(const char *dir,size_t index,char *name,size_t cap){
    if(!dir || (*dir && vfs_kind(dir)!=VFS_DIRECTORY))return false;
    size_t seen=0;
    for(size_t i=0;i<vfs_count();++i){
        char candidate[96];
        if(!vfs_name_at(i,candidate,sizeof(candidate)))continue;
        if(!child_of(dir,candidate))continue;
        if(seen++==index)return copy_name(name,cap,candidate);
    }
    /* Virtual documents view of HOME entries, without duplicating storage. */
    if(equal(dir,"my documents") || equal(dir,"documents and settings/user/my documents")){
        for(size_t i=0;i<ramfs_count();++i){
            char candidate[96];
            if(!ramfs_name_at(i,candidate,sizeof(candidate)) || !home_path(candidate))continue;
            char full[96];size_t at=0;
            while(dir[at] && at+1<sizeof(full)){full[at]=dir[at];++at;}
            if(at+1>=sizeof(full))continue;
            full[at++]='/';
            size_t j=5;
            while(candidate[j] && at+1<sizeof(full))full[at++]=candidate[j++];
            if(candidate[j])continue;
            full[at]=0;
            if(seen++==index)return copy_name(name,cap,full);
        }
    }
    return false;
}
bool vfs_read(const char *path,void *dst,size_t cap,size_t *length){
    if(!safe_path(path)||!length||(!dst&&cap))return false;
    char backing[48];const char *p=home_alias(path,backing,sizeof(backing));
    if(p)return ramfs_read(p,dst,cap,length);
    const char *alias=vfs_executable_target(path);if(alias)path=alias;
    const void *source=NULL;size_t size=0;
    if(!initrd_lookup(path,&source,&size)||size>cap||(!dst&&size))return false;
    for(size_t i=0;i<size;++i)((uint8_t*)dst)[i]=((const uint8_t*)source)[i];
    *length=size;return true;
}
bool vfs_read_slice(const char *path,size_t offset,void *dst,size_t cap,size_t *length){
    if(!safe_path(path)||!length||(!dst&&cap))return false;
    const char *alias=vfs_executable_target(path);if(alias)path=alias;
    const void *data=NULL;size_t n=0;
    if(!initrd_lookup(path,&data,&n)||offset>n)return false;
    if(cap>n-offset)cap=n-offset;
    for(size_t i=0;i<cap;++i)((uint8_t *)dst)[i]=((const uint8_t *)data)[offset+i];
    *length=cap;return true;
}
bool vfs_write(const char *path,const void *src,size_t len){
    if(!safe_path(path))return false;
    char buf[48];const char *p=home_alias(path,buf,sizeof(buf));
    return p && ramfs_write(p,src,len);
}
bool vfs_copy(const char *from,const char *to){
    if(!safe_path(from)||!safe_path(to))return false;
    char tobuf[48],frombuf[48];
    const char *dest=home_alias(to,tobuf,sizeof(tobuf));
    if(!dest)return false;
    const char *original=home_alias(from,frombuf,sizeof(frombuf));
    if(original)return ramfs_copy(original,dest);
    const char *alias=vfs_executable_target(from);if(alias)from=alias;
    const void *data=NULL;size_t size=0;
    if(!initrd_lookup(from,&data,&size)||size>RAMFS_FILE_CAPACITY||vfs_kind(to)!=VFS_MISSING)
        return false;
    return ramfs_write(dest,data,size);
}
bool vfs_rename(const char *from,const char *to){
    if(!safe_path(from)||!safe_path(to))return false;
    char a[48],b[48];const char *source=home_alias(from,a,sizeof(a));
    const char *dest=home_alias(to,b,sizeof(b));
    return source&&dest&&ramfs_rename(source,dest);
}
bool vfs_remove(const char *path){
    if(!safe_path(path))return false;
    char b[48];const char *p=home_alias(path,b,sizeof(b));
    return p && ramfs_remove(p);
}
