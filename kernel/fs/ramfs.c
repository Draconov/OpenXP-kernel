#include "fs/ramfs.h"
#include <stdint.h>

/* Fixed bounds keep allocation and failure behavior deterministic during
 * early development. Every write is atomic from the caller's perspective
 * on the current single-CPU, kernel-hosted desktop. Not SMP-safe. */
typedef struct {
    char name[RAMFS_NAME_CAPACITY];
    uint8_t data[RAMFS_FILE_CAPACITY];
    size_t size;
    bool used;
} ramfs_file_t;
static ramfs_file_t files[RAMFS_MAX_FILES];
static uint64_t revision;
uint64_t ramfs_revision(void){return revision;}

static bool valid_name(const char *name) {
    if(!name || !*name)return false;
    size_t n=0,segment=0;
    while(name[n]) {
        unsigned char c=(unsigned char)name[n];
        if(n+1>=RAMFS_NAME_CAPACITY || c<33 || c>126 || c=='\\' || c==':')return false;
        if(c=='/') {
            if(segment==0 || (segment==1 && name[n-1]=='.') ||
               (segment==2 && name[n-2]=='.' && name[n-1]=='.'))return false;
            segment=0;
        } else ++segment;
        ++n;
    }
    if(segment==0 || (segment==1 && name[n-1]=='.') ||
       (segment==2 && name[n-2]=='.' && name[n-1]=='.'))return false;
    return true;
}
static bool equal(const char *a,const char *b){
    while(*a && *a==*b){++a;++b;}
    return *a==*b;
}
static int find(const char *name){
    for(unsigned i=0;i<RAMFS_MAX_FILES;++i)
        if(files[i].used && equal(name,files[i].name))return (int)i;
    return -1;
}
void ramfs_reset(void){
    ++revision;
    /* Clear file metadata, not all 192 KiB of backing storage. */
    for(unsigned i=0;i<RAMFS_MAX_FILES;++i){
        files[i].used=false;files[i].size=0;files[i].name[0]=0;
    }
}
bool ramfs_write(const char *name,const void *data,size_t size){
    if(!valid_name(name) || size>RAMFS_FILE_CAPACITY || (size && !data))return false;
    int slot=find(name);
    if(slot<0){
        for(unsigned i=0;i<RAMFS_MAX_FILES;++i)
            if(!files[i].used){slot=(int)i;break;}
    }
    if(slot<0)return false; /* existing contents are preserved on failure */
    ramfs_file_t *f=&files[slot];
    /* Permit an input buffer inside this file by copying forwards/backwards. */
    const uint8_t *src=(const uint8_t *)data;
    uint8_t *dst=f->data;
    if(size && (uintptr_t)src<(uintptr_t)dst &&
       (uintptr_t)dst-(uintptr_t)src<size){
        for(size_t i=size;i>0;--i)dst[i-1]=src[i-1];
    } else {
        for(size_t i=0;i<size;++i)dst[i]=src[i];
    }
    size_t j=0;while(name[j]){f->name[j]=name[j];++j;}f->name[j]=0;
    f->size=size;f->used=true;
    ++revision;return true;
}
bool ramfs_size(const char *name,size_t *size){
    int slot=name?find(name):-1;
    if(slot<0 || !size)return false;
    *size=files[slot].size;return true;
}
bool ramfs_read(const char *name,void *data,size_t capacity,size_t *size){
    if(!name || !size)return false;
    int slot=find(name);
    if(slot<0 || files[slot].size>capacity || (files[slot].size && !data))return false;
    const ramfs_file_t *f=&files[slot];
    for(size_t i=0;i<f->size;++i)((uint8_t *)data)[i]=f->data[i];
    *size=f->size;
    return true;
}
bool ramfs_remove(const char *name){
    if(!name)return false;
    int slot=find(name);
    if(slot<0)return false;
    files[slot].used=false;files[slot].size=0;files[slot].name[0]=0;
    ++revision;return true;
}
bool ramfs_rename(const char *old_name,const char *new_name){
    if(!valid_name(new_name)||!old_name)return false;
    int src=find(old_name);
    if(src<0||find(new_name)>=0)return false;
    size_t n=0;while(new_name[n]){files[src].name[n]=new_name[n];++n;}
    files[src].name[n]=0;
    ++revision;return true;
}
bool ramfs_copy(const char *from,const char *to){
    if(!valid_name(to)||!from)return false;
    int src=find(from);
    if(src<0||find(to)>=0)return false;
    int dst=-1;
    for(unsigned i=0;i<RAMFS_MAX_FILES;++i)if(!files[i].used){dst=(int)i;break;}
    if(dst<0)return false;
    ramfs_file_t *target=&files[dst];
    for(size_t i=0;i<files[src].size;++i)target->data[i]=files[src].data[i];
    size_t n=0;while(to[n]){target->name[n]=to[n];++n;}target->name[n]=0;
    target->size=files[src].size;target->used=true;
    ++revision;return true;
}
/* Rename a directory plus every child with a single metadata transaction.
 * Preflight the entire rename before touching even one entry. */
bool ramfs_rename_tree(const char *from,const char *to){
    if(!valid_name(from)||!valid_name(to))return false;
    size_t a=0,b=0;
    while(from[a])++a;
    while(to[b])++b;
    if(b>=a && !equal(from,to)){
        bool nested=true;
        for(size_t j=0;j<a;++j)if(from[j]!=to[j]){nested=false;break;}
        if(nested && to[a]=='/')return false;
    }
    if(equal(from,to))return false;
    char next[RAMFS_MAX_FILES][RAMFS_NAME_CAPACITY];
    bool affected[RAMFS_MAX_FILES]={0};
    unsigned moved=0;
    for(unsigned i=0;i<RAMFS_MAX_FILES;++i){
        if(!files[i].used)continue;
        const char *name=files[i].name;
        size_t k=0;while(k<a && name[k] && name[k]==from[k])++k;
        if(k!=a || (name[k]!='/' && name[k]!=0))continue;
        size_t tail=0;while(name[k+tail])++tail;
        if(b+tail>=RAMFS_NAME_CAPACITY)return false;
        for(size_t j=0;j<b;++j)next[i][j]=to[j];
        for(size_t j=0;j<=tail;++j)next[i][b+j]=name[k+j];
        if(!valid_name(next[i]))return false;
        affected[i]=true;++moved;
    }
    if(!moved)return false;
    for(unsigned i=0;i<RAMFS_MAX_FILES;++i){
        if(!affected[i])continue;
        for(unsigned j=0;j<RAMFS_MAX_FILES;++j){
            if(!files[j].used || i==j)continue;
            if(affected[j]){
                if(equal(next[i],next[j]))return false;
            }else if(equal(next[i],files[j].name))return false;
        }
    }
    for(unsigned i=0;i<RAMFS_MAX_FILES;++i){
        if(!affected[i])continue;
        size_t j=0;while(next[i][j]){files[i].name[j]=next[i][j];++j;}
        files[i].name[j]=0;
    }
    ++revision;return true;
}
size_t ramfs_count(void){
    size_t n=0;
    for(unsigned i=0;i<RAMFS_MAX_FILES;++i)if(files[i].used)++n;
    return n;
}
bool ramfs_name_at(size_t index,char *name,size_t capacity){
    if(!name||!capacity)return false;
    for(unsigned i=0;i<RAMFS_MAX_FILES;++i)if(files[i].used){
        if(index-- !=0)continue;
        size_t n=0;while(files[i].name[n])++n;
        if(n>=capacity)return false;
        for(size_t j=0;j<=n;++j)name[j]=files[i].name[j];
        return true;
    }
    return false;
}
