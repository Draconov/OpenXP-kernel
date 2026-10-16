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
    long end=ftell(input);assert(end>0&&end<(long)INITRD_MAX_SIZE);
    rewind(input);
    void *image=malloc((size_t)end);assert(image);
    assert(fread(image,1,(size_t)end,input)==(size_t)end);
    fclose(input);
    ramfs_reset();initrd_unmount();
    assert(initrd_mount(image,(size_t)end));
    /* Directories and executable aliases are real VFS entries, not fake
     * Windows binaries or duplicate executable archive payloads. */
    assert(vfs_kind("windows")==VFS_DIRECTORY);
    assert(vfs_kind("windows/system32")==VFS_DIRECTORY);
    assert(vfs_kind("windows/web/wallpaper")==VFS_DIRECTORY);
    assert(vfs_kind("system apps")==VFS_DIRECTORY);
    assert(vfs_kind("program files/openxp")==VFS_DIRECTORY);
    assert(vfs_kind("documents and settings/user/my documents")==VFS_DIRECTORY);
    assert(vfs_kind("system apps/notepad.exe")==VFS_BOOT_READONLY);
    assert(vfs_kind("windows/system32/notepad.exe")==VFS_BOOT_READONLY);
    assert(vfs_kind("windows/system32/evil.exe")==VFS_MISSING);
    assert(vfs_kind("windows/../bin/notepad")==VFS_MISSING);
    assert(!strcmp(vfs_executable_target("system apps/notepad.exe"),"bin/notepad"));
    assert(vfs_executable_target("windows/system32/evil.exe")==NULL);
    bool saw_windows=false, saw_apps=false, saw_wallpaper=false;
    for(size_t i=0;i<128;++i){
        char entry[96];
        if(!vfs_list_dir("",i,entry,sizeof(entry)))break;
        if(!strcmp(entry,"windows"))saw_windows=true;
        if(!strcmp(entry,"system apps"))saw_apps=true;
    }
    assert(saw_windows&&saw_apps);
    for(size_t i=0;i<128;++i){
        char entry[96];
        if(!vfs_list_dir("windows/web/wallpaper",i,entry,sizeof(entry)))break;
        if(!strcmp(entry,"windows/web/wallpaper/azure.bmp"))saw_wallpaper=true;
    }
    assert(saw_wallpaper);
    {char bad[96];assert(!vfs_list_dir("windows/../",0,bad,sizeof(bad)));}
    assert(!vfs_write("system apps/notepad.exe","x",1));
    assert(!vfs_remove("windows/system32/notepad.exe"));
    assert(vfs_kind("bin/hello")==VFS_BOOT_READONLY);
    assert(vfs_kind("docs/readme.txt")==VFS_BOOT_READONLY);
    assert(vfs_kind("home/note.txt")==VFS_MISSING);
    assert(vfs_count()>initrd_count());
    char name[RAMFS_NAME_CAPACITY];
    assert(vfs_name_at(0,name,sizeof(name)));
    size_t file_len=0;
    char docs[8192];
    char native1[32768],native2[32768];size_t n1=0,n2=0;
    assert(vfs_read("bin/notepad",native1,sizeof(native1),&n1));
    assert(vfs_read("system apps/notepad.exe",native2,sizeof(native2),&n2));
    assert(n1==n2 && !memcmp(native1,native2,n1));
    assert(n1>=4 && !memcmp(native2,"\177ELF",4));
    assert(vfs_read("docs/readme.txt",docs,sizeof(docs),&file_len));
    assert(file_len>0);
    {char part[8]={0};size_t amount=0;
        assert(vfs_read_slice("docs/readme.txt",0,part,5,&amount));
        assert(amount==5 && !memcmp(part,docs,5));
        assert(vfs_read_slice("docs/readme.txt",file_len-3,part,7,&amount));
        assert(amount==3 && !memcmp(part,docs+file_len-3,3));
        assert(vfs_read_slice("docs/readme.txt",file_len,part,7,&amount)&&amount==0);
        assert(!vfs_read_slice("docs/readme.txt",file_len+1,part,7,&amount));
        assert(!vfs_read_slice("home/../traverse",0,part,4,&amount));
    }
    assert(!vfs_read("docs/readme.txt",docs,1,&file_len));
    assert(!vfs_write("bin/hello","BAD",3));
    assert(!vfs_write("docs/readme.txt","BAD",3));
    assert(!vfs_write("home/../bad","BAD",3));
    assert(vfs_write("home/note.txt","Hello",5));
    assert(vfs_write("my documents/note2.txt","Doc",3));
    assert(vfs_kind("documents and settings/user/my documents/note2.txt")==VFS_HOME_WRITABLE);
    assert(vfs_kind("home/note2.txt")==VFS_HOME_WRITABLE);
    assert(vfs_read("my documents/note2.txt",docs,sizeof(docs),&file_len));
    assert(file_len==3&&!memcmp(docs,"Doc",3));
    assert(vfs_remove("documents and settings/user/my documents/note2.txt"));
    assert(vfs_kind("home/note.txt")==VFS_HOME_WRITABLE);
    assert(vfs_count()>initrd_count()+1);
    char out[16]={0};
    assert(vfs_read("home/note.txt",out,sizeof(out),&file_len));
    assert(file_len==5&&!memcmp(out,"Hello",5));
    assert(vfs_copy("docs/readme.txt","home/readme-copy.txt"));
    assert(vfs_kind("home/readme-copy.txt")==VFS_HOME_WRITABLE);
    assert(vfs_read("home/readme-copy.txt",docs,sizeof(docs),&file_len));
    assert(file_len>0);
    assert(!vfs_copy("docs/readme.txt","home/readme-copy.txt"));
    assert(!vfs_copy("docs/readme.txt","bin/overwrite"));
    assert(!vfs_copy("bin/unknown","home/nope"));
    assert(vfs_copy("home/note.txt","home/copy.txt"));
    assert(!vfs_copy("home/note.txt","home/copy.txt"));
    assert(vfs_rename("home/copy.txt","home/new.txt"));
    assert(!vfs_rename("bin/hello","home/executable"));
    assert(!vfs_remove("bin/hello"));
    assert(vfs_remove("home/note.txt"));
    assert(vfs_kind("home/note.txt")==VFS_MISSING);
    assert(vfs_kind("home/new.txt")==VFS_HOME_WRITABLE);
    /* Aurora 16: nested writable directories, alias views, metadata and ACLs. */
    assert(!vfs_mkdir("windows/hacked"));
    assert(!vfs_mkdir("home/../hacked"));
    assert(!vfs_mkdir("home/missing/child"));
    assert(vfs_mkdir("home/projects"));
    assert(vfs_kind("home/projects")==VFS_DIRECTORY);
    assert(vfs_kind("my documents/projects")==VFS_DIRECTORY);
    assert(!vfs_mkdir("home/projects"));
    assert(!vfs_write("home/projects/.oxpdir","x",1));
    assert(!vfs_remove("home/projects/.oxpdir"));
    assert(!vfs_read("home/projects/.oxpdir",out,sizeof(out),&file_len));
    assert(vfs_mkdir("my documents/projects/archive"));
    assert(vfs_write("home/projects/readme.txt","123",3));
    assert(!vfs_rmdir("home/projects"));
    assert(!vfs_rmdir("home/projects")); /* nested folder exists */
    vfs_kind_t kind=VFS_MISSING;size_t bytes=999u;
    assert(vfs_stat("my documents/projects/readme.txt",&bytes,&kind));
    assert(kind==VFS_HOME_WRITABLE&&bytes==3);
    assert(vfs_stat("docs/readme.txt",&bytes,&kind)&&kind==VFS_BOOT_READONLY&&bytes>0);
    assert(vfs_stat("home/projects/archive",&bytes,&kind)&&kind==VFS_DIRECTORY&&bytes==0);
    assert(!vfs_stat("home/ghost",&bytes,&kind));
    char listed[96];
    assert(vfs_list_dir("my documents/projects",0,listed,sizeof(listed)));
    assert(!strcmp(listed,"my documents/projects/archive") ||
           !strcmp(listed,"my documents/projects/readme.txt"));
    assert(vfs_list_dir("home/projects",0,listed,sizeof(listed)));
    assert(strstr(listed,"/.oxpdir")==NULL);
    assert(!vfs_write("home/projects/bad/thing","x",1));
    assert(!vfs_remove("home/projects"));
    /* Aurora 17: atomic directory-tree rename across aliases, no self-nesting. */
    assert(!vfs_rename("home/projects","home/projects/archive/deeper"));
    assert(vfs_rename("my documents/projects","home/renamed"));
    assert(vfs_kind("home/projects")==VFS_MISSING);
    assert(vfs_kind("home/renamed")==VFS_DIRECTORY);
    assert(vfs_kind("home/renamed/archive")==VFS_DIRECTORY);
    assert(vfs_kind("home/renamed/readme.txt")==VFS_HOME_WRITABLE);
    assert(!vfs_rename("home","home/nope"));
    assert(!vfs_rename("home/renamed","windows/renamed"));
    assert(!vfs_rename("home/renamed","home/renamed/archive"));
    assert(vfs_remove("home/renamed/readme.txt"));
    assert(vfs_rmdir("home/renamed/archive"));
    assert(vfs_rmdir("home/renamed"));
    assert(vfs_kind("home/projects")==VFS_MISSING);
    assert(vfs_read("docs/readme.txt",docs,sizeof(docs),&file_len));
    initrd_unmount();free(image);
    puts("PASS: OXRD read-only namespace, HOME writes, path validation, copy, rename and delete");
}
