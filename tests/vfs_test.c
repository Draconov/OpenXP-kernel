#include "fs/vfs.h"
#include "fs/ramfs.h"
#include "fs/initrd.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc==2);
    FILE *input=fopen(argv[1],"rb");assert(input);
    assert(!fseek(input,0,SEEK_END));
    long end=ftell(input);assert(end>0&&end<1048576);
    rewind(input);
    void *image=malloc((size_t)end);assert(image);
    assert(fread(image,1,(size_t)end,input)==(size_t)end);
    fclose(input);
    ramfs_reset();initrd_unmount();
    assert(initrd_mount(image,(size_t)end));
    assert(vfs_kind("bin/hello")==VFS_BOOT_READONLY);
    assert(vfs_kind("docs/readme.txt")==VFS_BOOT_READONLY);
    assert(vfs_kind("home/note.txt")==VFS_MISSING);
    assert(vfs_count()==initrd_count());
    char name[RAMFS_NAME_CAPACITY];
    assert(vfs_name_at(0,name,sizeof(name)));
    size_t file_len=0;
    char docs[8192];
    assert(vfs_read("docs/readme.txt",docs,sizeof(docs),&file_len));
    assert(file_len>0);
    assert(!vfs_read("docs/readme.txt",docs,1,&file_len));
    assert(!vfs_write("bin/hello","BAD",3));
    assert(!vfs_write("docs/readme.txt","BAD",3));
    assert(!vfs_write("home/../bad","BAD",3));
    assert(vfs_write("home/note.txt","Hello",5));
    assert(vfs_kind("home/note.txt")==VFS_HOME_WRITABLE);
    assert(vfs_count()==initrd_count()+1);
    char out[16]={0};
    assert(vfs_read("home/note.txt",out,sizeof(out),&file_len));
    assert(file_len==5&&!memcmp(out,"Hello",5));
    assert(vfs_copy("home/note.txt","home/copy.txt"));
    assert(!vfs_copy("home/note.txt","home/copy.txt"));
    assert(vfs_rename("home/copy.txt","home/new.txt"));
    assert(!vfs_rename("bin/hello","home/executable"));
    assert(!vfs_remove("bin/hello"));
    assert(vfs_remove("home/note.txt"));
    assert(vfs_kind("home/note.txt")==VFS_MISSING);
    assert(vfs_kind("home/new.txt")==VFS_HOME_WRITABLE);
    assert(vfs_read("docs/readme.txt",docs,sizeof(docs),&file_len));
    initrd_unmount();free(image);
    puts("PASS: OXRD read-only namespace, HOME writes, path validation, copy, rename and delete");
}
