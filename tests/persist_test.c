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
