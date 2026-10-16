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
/* A directory occupies one zero-byte RAMFS record named <folder>/.oxpdir.
 * This keeps snapshots backwards-compatible without a second metadata store. */
#define DIR_MARKER "/.oxpdir"
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
/* Documents aliases are backed by the bounded writable HOME filesystem. */
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
static bool suffix_marker(const char *p){
    size_t n=0,m=0;while(p[n])++n;
    while(DIR_MARKER[m])++m;
    if(n<=m)return false;
    for(size_t i=0;i<m;++i)if(p[n-m+i]!=DIR_MARKER[i])return false;
    return true;
}
static bool marker_path(const char *p,char out[RAMFS_NAME_CAPACITY]){
    if(!safe_path(p) || !home_path(p) || suffix_marker(p))return false;
    size_t n=0,m=0;while(p[n])++n;
    while(DIR_MARKER[m])++m;
    if(n+m>=RAMFS_NAME_CAPACITY)return false;
    for(size_t i=0;i<n;++i)out[i]=p[i];
    for(size_t i=0;i<=m;++i)out[n+i]=DIR_MARKER[i];
    return true;
}
static bool user_folder(const char *p){
    char marker[RAMFS_NAME_CAPACITY];size_t len=0;
    return marker_path(p,marker) && ramfs_size(marker,&len) && len==0;
}
/* Writable folder validation applies after alias translation. */
static bool parent_exists(const char *name){
    size_t n=0,slash=0;while(name[n]){if(name[n]=='/')slash=n;++n;}
    if(slash==4 && name[0]=='h' && name[1]=='o' && name[2]=='m' && name[3]=='e')return true;
    if(!slash || slash>=RAMFS_NAME_CAPACITY)return false;
    char parent[RAMFS_NAME_CAPACITY];
    for(size_t i=0;i<slash;++i)parent[i]=name[i];
    parent[slash]=0;
    return user_folder(parent);
}
size_t vfs_count(void){return COUNT(dirs)+COUNT(programs)+initrd_count()+ramfs_count();}
bool vfs_name_at(size_t index,char *name,size_t cap){
    if(index<COUNT(dirs))return copy_name(name,cap,dirs[index]);
    index-=COUNT(dirs);
    if(index<COUNT(programs))return copy_name(name,cap,programs[index].alias);
    index-=COUNT(programs);
    if(index<initrd_count())return initrd_name_at(index,name,cap);
    if(!ramfs_name_at(index-initrd_count(),name,cap))return false;
    if(suffix_marker(name)){
        size_t n=0,m=0;while(name[n])++n;while(DIR_MARKER[m])++m;
        name[n-m]=0; /* Present metadata as a directory, never as a file. */
    }
    return true;
}
vfs_kind_t vfs_kind(const char *path){
    if(!safe_path(path))return VFS_MISSING;
    for(size_t i=0;i<COUNT(dirs);++i)if(equal(path,dirs[i]))return VFS_DIRECTORY;
    char backing[48];const char *p=home_alias(path,backing,sizeof(backing));
    if(p){
        if(suffix_marker(p))return VFS_MISSING;
        if(user_folder(p))return VFS_DIRECTORY;
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
    /* The Documents aliases are views of HOME at every depth, including
     * user-created subfolders. Translate results back to the caller's path. */
    char mapped[RAMFS_NAME_CAPACITY];
    const char *home_dir=home_alias(dir,mapped,sizeof(mapped));
    if((home_dir && !home_path(dir)) || equal(dir,"my documents") ||
       equal(dir,"documents and settings/user/my documents")){
        const char *backing=home_dir?home_dir:"home";
        size_t base_len=0;while(backing[base_len])++base_len;
        for(size_t i=0;i<ramfs_count();++i){
            char candidate[RAMFS_NAME_CAPACITY];
            if(!ramfs_name_at(i,candidate,sizeof(candidate)))continue;
            if(suffix_marker(candidate)){
                size_t n=0,m=0;while(candidate[n])++n;while(DIR_MARKER[m])++m;
                candidate[n-m]=0;
            }
            if(!child_of(backing,candidate))continue;
            char full[96];size_t at=0,j=base_len;
            while(dir[at] && at+1<sizeof(full)){full[at]=dir[at];++at;}
            if(at+1>=sizeof(full))continue;
            full[at++]='/';j++;
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
    if(p)return !suffix_marker(p) && ramfs_read(p,dst,cap,length);
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
    return p && !suffix_marker(p) && vfs_kind(path)!=VFS_DIRECTORY &&
           parent_exists(p) && ramfs_write(p,src,len);
}
bool vfs_copy(const char *from,const char *to){
    if(!safe_path(from)||!safe_path(to))return false;
    char tobuf[48],frombuf[48];
    const char *dest=home_alias(to,tobuf,sizeof(tobuf));
    if(!dest || suffix_marker(dest) || vfs_kind(to)!=VFS_MISSING ||
       !parent_exists(dest))return false;
    const char *original=home_alias(from,frombuf,sizeof(frombuf));
    if(original){
        if(suffix_marker(original) || vfs_kind(from)!=VFS_HOME_WRITABLE)return false;
        return ramfs_copy(original,dest);
    }
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
    if(!source||!dest||suffix_marker(source)||suffix_marker(dest)||
       vfs_kind(to)!=VFS_MISSING||!parent_exists(dest))return false;
    vfs_kind_t kind=vfs_kind(from);
    if(kind==VFS_HOME_WRITABLE)return ramfs_rename(source,dest);
    if(kind!=VFS_DIRECTORY)return false;
    char marker[RAMFS_NAME_CAPACITY];
    /* Only user-created folders have a backing marker. Protect HOME root
     * and all read-only virtual directories. */
    if(!marker_path(source,marker)||!user_folder(source))return false;
    return ramfs_rename_tree(source,dest);
}
bool vfs_remove(const char *path){
    if(!safe_path(path))return false;
    char b[48];const char *p=home_alias(path,b,sizeof(b));
    return p && !suffix_marker(p) && vfs_kind(path)==VFS_HOME_WRITABLE && ramfs_remove(p);
}

/* No recursive destructive operations: folders must be empty to remove. */
bool vfs_mkdir(const char *path){
    if(!safe_path(path)||vfs_kind(path)!=VFS_MISSING)return false;
    char alias[RAMFS_NAME_CAPACITY],marker[RAMFS_NAME_CAPACITY];
    const char *p=home_alias(path,alias,sizeof(alias));
    return p && marker_path(p,marker) && parent_exists(p) &&
           ramfs_write(marker,NULL,0);
}
bool vfs_rmdir(const char *path){
    if(vfs_kind(path)!=VFS_DIRECTORY)return false;
    char alias[RAMFS_NAME_CAPACITY],marker[RAMFS_NAME_CAPACITY];
    const char *p=home_alias(path,alias,sizeof(alias));
    if(!p || !marker_path(p,marker))return false;
    char first[96];
    /* Includes nested directories and files; never delete a nonempty tree. */
    if(vfs_list_dir(path,0,first,sizeof(first)))return false;
    return ramfs_remove(marker);
}
bool vfs_stat(const char *path,size_t *size,vfs_kind_t *kind){
    if(!size||!kind)return false;
    *kind=vfs_kind(path);*size=0;
    if(*kind==VFS_MISSING)return false;
    if(*kind==VFS_DIRECTORY)return true;
    char alias[RAMFS_NAME_CAPACITY];const char *p=home_alias(path,alias,sizeof(alias));
    if(p)return ramfs_size(p,size);
    const char *source=vfs_executable_target(path);if(source)path=source;
    const void *data=NULL;
    return initrd_lookup(path,&data,size);
}
