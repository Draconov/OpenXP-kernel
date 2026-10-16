/* OpenXP host test suite: filesystem.
 * Select exactly one original test per compilation using -DOPENXP_TEST_NAME.
 * Individual test binaries and sanitizers remain unchanged. */
#if defined(OPENXP_TEST_RAMFS)
#include "fs/ramfs.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
int main(void){
    ramfs_reset();assert(ramfs_count()==0);
    assert(!ramfs_write("../escape","x",1));
    assert(!ramfs_write("home/../escape","x",1));
    assert(!ramfs_write("home//escape","x",1));
    assert(!ramfs_write("home/folder/.","x",1));
    assert(!ramfs_write("home/foo\\bar","x",1));
    assert(!ramfs_write("home/no",NULL,1));
    assert(ramfs_write("home/note.txt","HELLO",5));
    char name[48],text[16];size_t n=99;
    assert(ramfs_count()==1 && ramfs_name_at(0,name,sizeof(name)));
    assert(strcmp(name,"home/note.txt")==0);
    assert(!ramfs_name_at(1,name,sizeof(name)));
    assert(!ramfs_read("home/note.txt",text,4,&n));
    assert(ramfs_read("home/note.txt",text,sizeof(text),&n));
    assert(n==5 && !memcmp(text,"HELLO",5));
    assert(!ramfs_write("home/note.txt",NULL,1));
    assert(ramfs_read("home/note.txt",text,sizeof(text),&n)&&n==5);
    assert(ramfs_write("home/note.txt","HEY",3));
    assert(ramfs_read("home/note.txt",text,sizeof(text),&n)&&n==3);
    assert(!memcmp(text,"HEY",3) && ramfs_count()==1);
    assert(ramfs_copy("home/note.txt","home/copy.txt"));
    assert(!ramfs_copy("home/note.txt","home/copy.txt"));
    assert(!ramfs_rename("home/note.txt","home/copy.txt"));
    assert(!ramfs_rename("home/note.txt","../unsafe"));
    assert(ramfs_rename("home/copy.txt","home/moved.txt"));
    assert(!ramfs_read("home/copy.txt",text,sizeof(text),&n));
    assert(ramfs_read("home/moved.txt",text,sizeof(text),&n)&&n==3);
    assert(!memcmp(text,"HEY",3));
    assert(ramfs_remove("home/moved.txt"));
    assert(ramfs_write("home/empty",NULL,0));
    assert(ramfs_read("home/empty",NULL,0,&n)&&n==0);
    uint8_t canvas[16000],copy[16000];
    for(size_t i=0;i<sizeof(canvas);++i)canvas[i]=(uint8_t)(i%8);
    assert(ramfs_write("home/painting.oxp",canvas,sizeof(canvas)));
    assert(ramfs_read("home/painting.oxp",copy,sizeof(copy),&n));
    assert(n==sizeof(canvas) && !memcmp(canvas,copy,n));
    assert(!ramfs_write("home/large",canvas,RAMFS_FILE_CAPACITY+1));
    assert(ramfs_count()==3);
    for(unsigned i=0;i<RAMFS_MAX_FILES;++i){
        char path[]="home/item-00";
        path[10]=(char)('0'+i/10u);path[11]=(char)('0'+i%10u);
        if(i<RAMFS_MAX_FILES-3u)assert(ramfs_write(path,"!",1));
        else assert(!ramfs_write(path,"!",1));
    }
    assert(ramfs_count()==RAMFS_MAX_FILES);
    assert(!ramfs_write("home/full","?",1));
    assert(ramfs_write("home/note.txt","replaced",8));
    assert(ramfs_count()==RAMFS_MAX_FILES);
    assert(ramfs_remove("home/note.txt"));
    assert(!ramfs_remove("home/note.txt"));
    assert(ramfs_write("home/freed","z",1));
    assert(ramfs_count()==RAMFS_MAX_FILES);
    ramfs_reset();assert(ramfs_count()==0);
    assert(ramfs_write("home/group/.oxpdir",NULL,0));
    assert(ramfs_write("home/group/child.txt","A",1));
    assert(ramfs_write("home/group/sub/.oxpdir",NULL,0));
    assert(ramfs_write("home/group/sub/note.txt","B",1));
    uint64_t r=ramfs_revision();
    assert(!ramfs_rename_tree("home/group","home/group/sub/nested"));
    assert(ramfs_revision()==r);
    assert(!ramfs_rename_tree("home/group","home/"));
    /* Failed subtree rename does not modify any entry or revision. */
    assert(!ramfs_rename_tree("home/group","home/supercalifragilisticexpialidocious-longname"));
    assert(ramfs_revision()==r);
    assert(ramfs_size("home/group/sub/note.txt",&n)&&n==1);
    assert(ramfs_rename_tree("home/group","home/renamed"));
    assert(ramfs_revision()==r+1u);
    assert(ramfs_size("home/renamed/sub/note.txt",&n)&&n==1);
    assert(!ramfs_size("home/group/sub/note.txt",&n));
    assert(ramfs_write("home/occupied/sub/note.txt","X",1));
    r=ramfs_revision();
    assert(!ramfs_rename_tree("home/renamed","home/occupied"));
    assert(ramfs_revision()==r && ramfs_size("home/renamed/sub/note.txt",&n));
    ramfs_reset();assert(ramfs_count()==0);
    assert(!ramfs_read("home/painting.oxp",copy,sizeof(copy),&n));
    return 0;
}
#elif defined(OPENXP_TEST_VFS)
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
#elif defined(OPENXP_TEST_PERSIST)
#include "fs/persist.h"
#include "fs/ramfs.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SECTORS 4096u
static uint8_t drive[SECTORS][512];
static unsigned writes,limit;
static bool read_sector(void *ctx,uint32_t lba,uint8_t out[512]){
    (void)ctx;if(lba>=SECTORS)return false;
    memcpy(out,drive[lba],512);return true;
}
static bool write_sector(void *ctx,uint32_t lba,const uint8_t src[512]){
    (void)ctx;if(lba>=SECTORS || writes++>=limit)return false;
    memcpy(drive[lba],src,512);return true;
}
static bool synced(void *ctx){(void)ctx;return true;}
static persist_io_t io={read_sector,write_sector,synced,0};
static void check_byte(const char *path,uint8_t expected){
    uint8_t data[2];size_t n=0;
    assert(ramfs_read(path,data,sizeof(data),&n));
    assert(n==1 && data[0]==expected);
}
static void check_file(const char *path,const char *expected){
    char buf[128];size_t n=0;
    assert(ramfs_read(path,buf,sizeof(buf),&n));
    assert(n==strlen(expected)&&!memcmp(buf,expected,n));
}
int main(void){
    ramfs_reset();limit=999999;
    assert(!persist_mount(&io)); /* refuse to use an unformatted drive */
    memcpy(drive[0],"OXPDISK2",8);drive[0][8]=2;
    assert(persist_mount(&io));
    assert(!persist_restore());
    assert(persist_generation()==0);
    assert(ramfs_write("home/test.txt","original",8));
    assert(persist_commit() && persist_generation()==1);
    ramfs_reset();assert(persist_restore());check_file("home/test.txt","original");
    assert(ramfs_write("home/test.txt","updated",7));
    assert(ramfs_write("home/other.txt","safe",4));
    assert(persist_commit() && persist_generation()==2);
    ramfs_reset();assert(persist_restore());
    check_file("home/test.txt","updated");check_file("home/other.txt","safe");
    assert(ramfs_write("home/test.txt","partially saved",15));
    /* Simulate a power cut after writing some payload sectors. */
    limit=writes+2;
    assert(!persist_commit());
    limit=999999;
    ramfs_reset();assert(persist_mount(&io));assert(persist_restore());
    check_file("home/test.txt","updated");check_file("home/other.txt","safe");
    /* A malformed newer checkpoint must not hide an intact old one. */
    assert(ramfs_write("home/test.txt","new checkpoint",14));
    assert(persist_commit() && persist_generation()==3);
    drive[PERSIST_SLOT_A][20]^=0xffu; /* break older slot CRC, if selected */
    drive[PERSIST_SLOT_B][20]^=0xffu; /* break other slot CRC */
    ramfs_reset();assert(persist_mount(&io));assert(!persist_restore());
    /* Test refusal of a disk with a valid-looking but wrong revision. */
    drive[0][8]=9;
    assert(!persist_mount(&io));
    /* Legacy OXPDISK1: recognize and restore smaller snapshots without
     * interpreting the v1 journal layout as v2. */
    memset(drive,0,sizeof(drive));
    memcpy(drive[0],"OXPDISK1",8);drive[0][8]=1;
    ramfs_reset();assert(persist_mount(&io));
    assert(ramfs_write("home/legacy.cfg","old",3));
    assert(persist_commit());
    assert(!memcmp(drive[PERSIST_SLOT_A],"OXPSNAP1",8));
    assert(ramfs_write("home/legacy.cfg","new",3));
    assert(persist_commit());
    assert(!memcmp(drive[PERSIST_LEGACY_SLOT_B],"OXPSNAP1",8));
    ramfs_reset();assert(persist_mount(&io));assert(persist_restore());
    check_file("home/legacy.cfg","new");
    /* Desktop appearance and a nested directory survive a v2 checkpoint. */
    memset(drive,0,sizeof(drive));
    memcpy(drive[0],"OXPDISK2",8);drive[0][8]=2;
    ramfs_reset();assert(persist_mount(&io));
    {const uint8_t value=0;assert(ramfs_write("home/desktop.cfg",&value,1));}
    {const uint8_t value=2;assert(ramfs_write("home/appearance.cfg",&value,1));}
    {const uint8_t value=2;assert(ramfs_write("home/wallpaper.cfg",&value,1));}
    assert(ramfs_write("home/work/.oxpdir",NULL,0));
    assert(ramfs_write("home/work/note.txt","saved",5));
    assert(persist_commit());
    ramfs_reset();assert(persist_mount(&io));assert(persist_restore());
    check_byte("home/desktop.cfg",0);
    check_byte("home/appearance.cfg",2);
    check_byte("home/wallpaper.cfg",2);
    check_file("home/work/note.txt","saved");
    /* A legacy v1 slot cannot contain 13 completely full files. A failed
     * save MUST leave the previous good v1 checkpoint intact. */
    memset(drive,0,sizeof(drive));
    memcpy(drive[0],"OXPDISK1",8);drive[0][8]=1;
    ramfs_reset();assert(persist_mount(&io));
    assert(ramfs_write("home/safe.cfg","keep",4));
    assert(persist_commit());
    static uint8_t filled[RAMFS_FILE_CAPACITY];
    for(unsigned i=0;i<13u;++i){
        char path[]="home/full-00";
        path[10]=(char)('0'+i/10u);path[11]=(char)('0'+i%10u);
        assert(ramfs_write(path,filled,sizeof(filled)));
    }
    assert(!persist_commit());
    ramfs_reset();assert(persist_mount(&io));assert(persist_restore());
    check_file("home/safe.cfg","keep");
    /* Version 2 stores a fully populated 48-file RAMFS (16 KiB each). */
    memset(drive,0,sizeof(drive));
    memcpy(drive[0],"OXPDISK2",8);drive[0][8]=2;
    ramfs_reset();assert(persist_mount(&io));
    static uint8_t large[RAMFS_FILE_CAPACITY];
    for(size_t i=0;i<sizeof(large);++i)large[i]=(uint8_t)(i*29u+1u);
    for(unsigned i=0;i<RAMFS_MAX_FILES;++i){
        char path[]="home/file-00.txt";
        path[10]=(char)('0'+i/10u);path[11]=(char)('0'+i%10u);
        assert(ramfs_write(path,large,sizeof(large)));
    }
    assert(ramfs_count()==RAMFS_MAX_FILES);
    assert(persist_commit());
    assert(!memcmp(drive[PERSIST_SLOT_A],"OXPSNAP1",8));
    ramfs_reset();assert(persist_mount(&io));assert(persist_restore());
    assert(ramfs_count()==RAMFS_MAX_FILES);
    for(unsigned i=0;i<RAMFS_MAX_FILES;++i){
        char path[]="home/file-00.txt";
        path[10]=(char)('0'+i/10u);path[11]=(char)('0'+i%10u);
        static uint8_t verify[RAMFS_FILE_CAPACITY];size_t n=0;
        assert(ramfs_read(path,verify,sizeof(verify),&n));
        assert(n==sizeof(large)&&memcmp(verify,large,n)==0);
    }
    /* Interrupted next checkpoint leaves the previously committed slot. */
    assert(ramfs_remove("home/file-00.txt"));
    limit=writes+2;assert(!persist_commit());limit=999999;
    ramfs_reset();assert(persist_mount(&io));assert(persist_restore());
    assert(ramfs_count()==RAMFS_MAX_FILES);
    /* Refuse an incompatible version even if the magic matches. */
    drive[0][8]=99;assert(!persist_mount(&io));
    puts("PASS: formatted-disk gating, two-slot commit, recovery after power loss, CRC and version rejection");
    return 0;
}
#elif defined(OPENXP_TEST_INITRD)
#include "fs/initrd.h"
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static uint8_t buf[256];
static void put32(size_t n, uint32_t x) {
    for (unsigned i = 0; i < 4; ++i) buf[n+i] = (uint8_t)(x >> (8*i));
}
static uint32_t crc32(const void *data, size_t n) {
    const uint8_t *p = data;
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ ((uint32_t)-(int)(crc & 1u) & 0xedb88320u);
    }
    return ~crc;
}
static void make_archive(void) {
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "OXRD", 4);
    put32(4, 1); put32(8, 2); put32(12, 147);
    memcpy(buf+16, "bin/hello", 9);
    memcpy(buf+80, "bin/fault", 9);
    memcpy(buf+144, "HI!", 3);
    memcpy(buf+147, "UD2", 3);
    /* two records 16+2*64 -> data at byte 144, total 150 */
    put32(12, 150);
    put32(64, 144); put32(68, 3); put32(72, crc32(buf+144, 3)); put32(76, 1);
    put32(128, 147); put32(132, 3); put32(136, crc32(buf+147, 3)); put32(140, 1);
}
int main(void) {
    const void *out = NULL;
    size_t length = 0;
    assert(!initrd_lookup("bin/hello", &out, &length));
    make_archive();
    assert(initrd_mount(buf, 150) && initrd_count() == 2);
    assert(initrd_lookup("bin/hello", &out, &length) && length == 3 &&
           !memcmp(out, "HI!", 3));
    assert(initrd_lookup("bin/fault", &out, &length) && length == 3 &&
           !memcmp(out, "UD2", 3));
    assert(!initrd_lookup("bin/nope", &out, &length));
    assert(!initrd_lookup("bin/hello-extra", &out, &length));
    assert(!initrd_lookup("", &out, &length));
    assert(!initrd_lookup("bin/hello", NULL, &length));
    assert(!initrd_lookup("bin/hello", &out, NULL));
    assert(!initrd_mount(buf, 149) && !initrd_count());
    make_archive(); buf[146] ^= 1; /* corrupted file */
    assert(!initrd_mount(buf, 150));
    make_archive(); memcpy(buf+16, "bin/../hi", 9);
    assert(!initrd_mount(buf, 150));
    make_archive(); memcpy(buf+80, "bin/hello", 9);
    assert(!initrd_mount(buf, 150));
    make_archive(); put32(128, 146); /* overlap */
    assert(!initrd_mount(buf, 150));
    make_archive(); put32(132, 4); /* beyond end */
    assert(!initrd_mount(buf, 150));
    make_archive(); put32(8, INITRD_MAX_FILES+1);
    assert(!initrd_mount(buf, 150));
    make_archive(); put32(4, 42);
    assert(!initrd_mount(buf, 150));
    make_archive(); put32(140, 0);
    assert(!initrd_mount(buf, 150));
    make_archive();
    assert(initrd_mount(buf, 150));
    initrd_unmount(); assert(initrd_count() == 0);
    puts("PASS: initrd mount/lookup, CRC, names, overlap, truncated and corrupt archives");
    return 0;
}
#elif defined(OPENXP_TEST_INITRD_IMAGE)
#include "fs/initrd.h"
#include "user/process_layout.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t le64(const uint8_t *p) {
    uint64_t v = 0;
    for (unsigned i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (i * 8);
    return v;
}
/* Regression for the Ring 3 desktop launch failure: native windowd's
 * large RW mapping used to overlap the stack at the old 0x110000 offset.
 * Verify the *actual packaged ELF images* against the kernel stack layout.
 * A change to ELF size or stack placement must fail host-side CI, before the
 * QEMU shell smoke test reaches its 15-second timeout. */
static void verify_stack_disjoint(const uint8_t *data, size_t size) {
    assert(size >= 64 && le16(data + 54) == 56);
    uint16_t ph_count = le16(data + 56);
    uint64_t ph_offset = le64(data + 32);
    assert(ph_count && ph_count <= 16 && ph_offset <= size &&
           (uint64_t)ph_count * 56 <= size - ph_offset);
    const uint64_t stack_first = PROCESS_USER_STACK_GUARD_OFFSET;
    const uint64_t stack_last = stack_first +
        (PROCESS_USER_STACK_PAGES + 2u) * PROCESS_USER_PAGE_BYTES;
    assert(stack_last <= PROCESS_USER_SLOT_BYTES);
    for (unsigned i = 0; i < ph_count; ++i) {
        const uint8_t *ph = data + ph_offset + i * 56u;
        assert(le32(ph) == 1u); /* Only PT_LOAD programs are supported. */
        uint64_t first = le64(ph + 16), bytes = le64(ph + 40);
        /* The loader permits up to 2 MiB in one segment, and at most 768
         * mapped pages across executable text and writable data. */
        assert(bytes <= 2u * 1024u * 1024u);
        assert(bytes && first < PROCESS_USER_SLOT_BYTES &&
               bytes <= PROCESS_USER_SLOT_BYTES - first);
        uint64_t last = first + bytes;
        first &= ~(PROCESS_USER_PAGE_BYTES - 1u);
        last = (last + PROCESS_USER_PAGE_BYTES - 1u) &
               ~(PROCESS_USER_PAGE_BYTES - 1u);
        assert(last <= stack_first || first >= stack_last);
    }
}
static void verify_file(const char *path) {
    const uint8_t *data = NULL;
    size_t size = 0;
    assert(initrd_lookup(path, (const void **)&data, &size));
    assert(size >= 120 && !memcmp(data, "\177ELF", 4));
    assert(data[4] == 2 && data[5] == 1 && le16(data + 16) == 3);
    assert(le16(data + 18) == 62);
    assert(le64(data + 24) == 0x100000 && le64(data + 32) == 64);
    size_t first_offset = le16(data + 56) == 2 ? 176u : 120u;
    assert(le64(data + 72) == first_offset && le64(data + 80) == 0x100000);
    assert(le64(data + 96) == size - first_offset);
    if(first_offset == 176u){
        /* The native app's writable zero-initialized pages are isolated from
         * executable text. Two segments: RX, RW. */
        assert((data[68] & 7u) == 5u);
        assert((data[124] & 7u) == 6u);
    }
}
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    long n = ftell(file);
    assert(n > 0 && n <= INITRD_MAX_SIZE);
    rewind(file);
    uint8_t *data = malloc((size_t)n);
    assert(data);
    assert(fread(data, 1, (size_t)n, file) == (size_t)n);
    assert(fclose(file) == 0);
    assert(initrd_mount(data, (size_t)n) && initrd_count() >= 8);
    verify_file("bin/hello");
    verify_file("bin/fault");
    char name[64];
    /* Check every bundled executable, including programs added in future. */
    for(size_t i=0;i<initrd_count();++i){
        assert(initrd_name_at(i,name,sizeof(name)));
        if(name[0]=='b' && name[1]=='i' && name[2]=='n' && name[3]=='/'){
            const uint8_t *elf=NULL;
            size_t elf_size=0;
            assert(initrd_lookup(name, (const void **)&elf, &elf_size));
            assert(elf_size>=64 && !memcmp(elf,"\177ELF",4));
            verify_stack_disjoint(elf,elf_size);
        }
    }
    assert(!initrd_name_at(initrd_count(),name,sizeof(name)));
    verify_file("bin/calculator");
    verify_file("bin/clock");
    verify_file("bin/notepad");
    verify_file("bin/probe");
    verify_file("bin/canvas");
    verify_file("bin/windowd");
    verify_file("bin/winclient");
    verify_file("bin/waitprobe");
    assert(initrd_count()>=22);
    verify_file("bin/calendar");
    verify_file("bin/snake");
    verify_file("bin/mines");
    verify_file("bin/monitor");
    verify_file("bin/about");
    verify_file("bin/paint");
    verify_file("bin/wordpad");
    verify_file("bin/terminal");
    verify_file("bin/filemanager");
    verify_file("bin/run");
    verify_file("bin/settings");
    const char *wallpapers[]={"windows/web/wallpaper/azure.bmp",
        "windows/web/wallpaper/meadow.bmp","windows/web/wallpaper/twilight.bmp"};
    for(size_t i=0;i<3;++i){
        const void *bmp=NULL;size_t bmp_len=0;
        assert(initrd_lookup(wallpapers[i],&bmp,&bmp_len));
        assert(bmp_len>=54 && ((const unsigned char*)bmp)[0]=='B' &&
               ((const unsigned char*)bmp)[1]=='M');
    }
    const void *text=NULL;size_t length=0;
    assert(initrd_lookup("docs/readme.txt",&text,&length) && length>20 &&
           !memcmp(text,"OpenXP - Exp",12));
    initrd_unmount();
    free(data);
    puts("PASS: built initrd ELF64 apps, stack layout and desktop README");
    return 0;
}
#else
#error "Compile this suite with an OPENXP_TEST_* selector"
#endif
