#include "fs/persist.h"
#include "fs/ramfs.h"
#include <stddef.h>
#ifndef OPENXP_PERSIST_HOST_TEST
#include "arch/x86_64/irq_state.h"
#endif
#define MAGIC_DISK_V1 "OXPDISK1"
#define MAGIC_DISK_V2 "OXPDISK2"
#define MAGIC_SNAP "OXPSNAP1"
#define CAP ((PERSIST_SLOT_SECTORS-1u)*PERSIST_BLOCK_BYTES)
static persist_io_t io;
static bool mounted;
static unsigned slot_now;
static uint32_t slot_sectors;
static uint64_t generation;
static uint8_t payload[CAP];
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t rd64(const uint8_t *p){return (uint64_t)rd32(p)|((uint64_t)rd32(p+4)<<32);}
static void wr32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8));}
static void wr64(uint8_t *p,uint64_t v){wr32(p,(uint32_t)v);wr32(p+4,(uint32_t)(v>>32));}
static bool match8(const uint8_t *p,const char *s){for(unsigned i=0;i<8;++i)if(p[i]!=(uint8_t)s[i])return false;return true;}
static uint32_t crc(const uint8_t *p,uint32_t n){
    uint32_t result=0xffffffffu;
    for(uint32_t i=0;i<n;++i){result^=p[i];for(unsigned b=0;b<8;++b)
        result=(result>>1)^(0xedb88320u&(0u-(result&1u)));}
    return ~result;
}
static uint32_t base(unsigned slot){return PERSIST_SLOT_A+(slot?slot_sectors:0u);}
static uint32_t slot_capacity(void){return (slot_sectors-1u)*PERSIST_BLOCK_BYTES;}
static bool read_slot(unsigned slot,uint64_t *gen,uint32_t *len){
    uint8_t hdr[512];
    if(!io.read(io.ctx,base(slot),hdr)||!match8(hdr,MAGIC_SNAP))return false;
    uint32_t size=rd32(hdr+16);
    if(!size||size>slot_capacity())return false;
    for(uint32_t i=0;i<(size+511)/512;++i)
        if(!io.read(io.ctx,base(slot)+1+i,payload+i*512))return false;
    if(crc(payload,size)!=rd32(hdr+20))return false;
    *gen=rd64(hdr+8);*len=size;return true;
}
/* Validate names, bounds, record count, and collisions before mutating RAMFS. */
static bool parse(uint32_t length,bool apply){
    if(length<1 || payload[0]>RAMFS_MAX_FILES)return false;
    unsigned count=payload[0];size_t pos=1;
    char names[RAMFS_MAX_FILES][RAMFS_NAME_CAPACITY];
    for(unsigned i=0;i<count;++i){
        if(pos>=length)return false;
        unsigned n=payload[pos++];
        if(n<1||n>=RAMFS_NAME_CAPACITY||pos+n+2>length)return false;
        char name[RAMFS_NAME_CAPACITY];
        for(unsigned j=0;j<n;++j)name[j]=(char)payload[pos+j];
        name[n]=0;
        pos+=n;
        uint32_t size=(uint32_t)payload[pos]|((uint32_t)payload[pos+1]<<8);pos+=2;
        if(size>RAMFS_FILE_CAPACITY||size>length-pos)return false;
        /* Verify path via the exact RAMFS validator only when applying.
         * Structural name check here makes malformed images fail early. */
        for(unsigned j=0;j<n;++j){
            unsigned char c=(unsigned char)name[j];
            if(c<33||c>126||c==':'||c==92)return false;
        }
        if(name[0]=='/'||name[n-1]=='/')return false;
        unsigned segment=0;
        for(unsigned j=0;j<n;++j){
            if(name[j]=='/'){
                if(!segment || (segment==1 && name[j-1]=='.') ||
                    (segment==2 && name[j-2]=='.' && name[j-1]=='.'))return false;
                segment=0;
            } else ++segment;
        }
        if(!segment || (segment==1 && name[n-1]=='.') ||
            (segment==2 && name[n-2]=='.' && name[n-1]=='.'))return false;
        for(unsigned old=0;old<i;++old){
            unsigned j=0;
            while(names[old][j] && names[old][j]==name[j])++j;
            if(names[old][j]==name[j])return false;
        }
        for(unsigned j=0;j<=n;++j)names[i][j]=name[j];
        if(apply&&!ramfs_write(name,payload+pos,size))return false;
        pos+=size;
    }
    return pos==length;
}
bool persist_mount(const persist_io_t *backend){
    mounted=false;generation=0;slot_now=0;slot_sectors=0;
    if(!backend||!backend->read||!backend->write)return false;
    uint8_t header[512];
    if(!backend->read(backend->ctx,0,header))return false;
    if(match8(header,MAGIC_DISK_V1) && rd32(header+8)==1u)
        slot_sectors=PERSIST_LEGACY_SLOT_SECTORS;
    else if(match8(header,MAGIC_DISK_V2) && rd32(header+8)==2u)
        slot_sectors=PERSIST_SLOT_SECTORS;
    else return false;
    io=*backend;mounted=true;return true;
}
bool persist_is_mounted(void){return mounted;}
uint64_t persist_generation(void){return mounted?generation:0;}
bool persist_restore(void){
    if(!mounted)return false;
    uint64_t best=0;unsigned best_slot=0;bool found=false;
    for(unsigned slot=0;slot<2;++slot){
        uint64_t gen=0;uint32_t len=0;
        if(read_slot(slot,&gen,&len)&&gen && parse(len,false)&&(!found||gen>best)){
            found=true;best=gen;best_slot=slot;
        }
    }
    if(!found)return false; /* Fresh formatted disk, no snapshot yet. */
    uint64_t gen=0;uint32_t len=0;
    if(!read_slot(best_slot,&gen,&len)||gen!=best||!parse(len,false))return false;
    ramfs_reset();
    if(!parse(len,true)){ramfs_reset();return false;}
    generation=best;slot_now=best_slot;return true;
}
static bool snapshot_ramfs(size_t *out_len){
    unsigned count=(unsigned)ramfs_count();
    if(count>RAMFS_MAX_FILES)return false;
    payload[0]=(uint8_t)count;size_t pos=1;
    for(unsigned i=0;i<count;++i){
        char name[RAMFS_NAME_CAPACITY];
        if(!ramfs_name_at(i,name,sizeof(name)))return false;
        size_t n=0;while(name[n])++n;
        if(n<1||n>=RAMFS_NAME_CAPACITY||pos+n+3>slot_capacity())return false;
        size_t bytes=0;
        payload[pos++]=(uint8_t)n;
        for(size_t j=0;j<n;++j)payload[pos++]=(uint8_t)name[j];
        /* Reserve size prefix, read directly from RAMFS to the snapshot. */
        size_t prefix=pos;pos+=2;
        if(!ramfs_read(name,payload+pos,slot_capacity()-pos,&bytes)||bytes>RAMFS_FILE_CAPACITY)return false;
        payload[prefix]=(uint8_t)bytes;payload[prefix+1]=(uint8_t)(bytes>>8);
        pos+=bytes;
    }
    if(pos>slot_capacity())return false;
    *out_len=pos;return true;
}
bool persist_commit(void){
    if(!mounted || generation==UINT64_MAX)return false;
    size_t pos=0;
#ifndef OPENXP_PERSIST_HOST_TEST
    /* RAMFS is a single-CPU structure. Capture all filenames/content with
     * timer preemption disabled; enable interrupts for the slow disk I/O. */
    uint64_t irqflags=irq_save_disable();
#endif
    bool ok=snapshot_ramfs(&pos);
#ifndef OPENXP_PERSIST_HOST_TEST
    irq_restore(irqflags);
#endif
    if(!ok)return false;
    unsigned next=generation?1u-slot_now:0u;
    uint32_t start=base(next);
    /* Invalidate destination before writing the payload. A crash can never
     * invalidate the previous successfully committed slot. */
    uint8_t sector[512]={0};
    if(!io.write(io.ctx,start,sector))return false;
    for(size_t i=0;i<pos;i+=512){
        for(unsigned j=0;j<512;++j)sector[j]=(i+j<pos)?payload[i+j]:0;
        if(!io.write(io.ctx,start+1u+(uint32_t)(i/512),sector))return false;
    }
    if(io.sync && !io.sync(io.ctx))return false;
    for(unsigned i=0;i<512;++i)sector[i]=0;
    for(unsigned i=0;i<8;++i)sector[i]=(uint8_t)MAGIC_SNAP[i];
    wr64(sector+8,generation+1);wr32(sector+16,(uint32_t)pos);
    wr32(sector+20,crc(payload,(uint32_t)pos));
    if(!io.write(io.ctx,start,sector))return false;
    if(io.sync && !io.sync(io.ctx))return false;
    slot_now=next;++generation;return true;
}
