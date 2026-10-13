#include "fs/vfs.h"
#include "fs/initrd.h"
#include "fs/ramfs.h"
#include <stdint.h>
/* Keep the writable namespace unambiguous and prevent callers from forging
 * implicit parent paths. RAMFS itself validates each complete filename. */
static bool home_path(const char *p) {
    if (!p || p[0]!='h'||p[1]!='o'||p[2]!='m'||p[3]!='e'||p[4]!='/')
        return false;
    return p[5]!=0;
}
size_t vfs_count(void) { return initrd_count()+ramfs_count(); }
bool vfs_name_at(size_t index,char *name,size_t cap) {
    size_t boot=initrd_count();
    return index<boot?initrd_name_at(index,name,cap):
                      ramfs_name_at(index-boot,name,cap);
}
vfs_kind_t vfs_kind(const char *path) {
    if (!path) return VFS_MISSING;
    const void *data; size_t length;
    if (home_path(path)) {
        /* Enumerate bounded file names without allocating a large buffer. */
        for(size_t i=0;i<ramfs_count();++i){
            char name[RAMFS_NAME_CAPACITY];
            if(!ramfs_name_at(i,name,sizeof(name)))continue;
            size_t j=0;
            while(name[j] && name[j]==path[j])++j;
            if(!name[j]&&!path[j])return VFS_HOME_WRITABLE;
        }
        return VFS_MISSING;
    }
    return initrd_lookup(path,&data,&length)?VFS_BOOT_READONLY:VFS_MISSING;
}
bool vfs_read(const char *path,void *dst,size_t cap,size_t *length) {
    if(!path||!length||(!dst&&cap))return false;
    if(home_path(path))return ramfs_read(path,dst,cap,length);
    const void *source=0;size_t size=0;
    if(!initrd_lookup(path,&source,&size)||size>cap||(!dst&&size))return false;
    for(size_t i=0;i<size;++i)((uint8_t *)dst)[i]=((const uint8_t *)source)[i];
    *length=size;return true;
}
bool vfs_write(const char *path,const void *src,size_t length) {
    return home_path(path)&&ramfs_write(path,src,length);
}
bool vfs_copy(const char *from,const char *to) {
    return home_path(from)&&home_path(to)&&ramfs_copy(from,to);
}
bool vfs_rename(const char *from,const char *to) {
    return home_path(from)&&home_path(to)&&ramfs_rename(from,to);
}
bool vfs_remove(const char *path) {
    return home_path(path)&&ramfs_remove(path);
}
