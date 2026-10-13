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
static void check_file(const char *path,const char *expected){
    char buf[128];size_t n=0;
    assert(ramfs_read(path,buf,sizeof(buf),&n));
    assert(n==strlen(expected)&&!memcmp(buf,expected,n));
}
int main(void){
    ramfs_reset();limit=999999;
    assert(!persist_mount(&io)); /* refuse to use an unformatted drive */
    memcpy(drive[0],"OXPDISK1",8);drive[0][8]=1;
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
    puts("PASS: formatted-disk gating, two-slot commit, recovery after power loss, CRC and version rejection");
    return 0;
}
