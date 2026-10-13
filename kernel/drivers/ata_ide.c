#include "drivers/ata_ide.h"
#ifdef OPENXP_ENABLE_EXPERIMENTAL_IDE_STORAGE
#include <stdint.h>
#include <stddef.h>
#define IO 0x170u
static inline void outb(uint16_t p,uint8_t v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline uint8_t inb(uint16_t p){uint8_t v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline uint16_t inw(uint16_t p){uint16_t v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void outw(uint16_t p,uint16_t v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static uint32_t capacity;
static bool ready(bool drq){
    for(unsigned attempts=0;attempts<100000u;++attempts){
        uint8_t s=inb(IO+7);
        if(!s || s==0xff || (s&0x21u))return false;
        if(!(s&0x80u) && (!drq || (s&0x08u)))return true;
    }
    return false;
}
static void select_sector(uint32_t lba,uint8_t command){
    outb(IO+6,(uint8_t)(0xe0u|((lba>>24)&0x0fu)));
    for(unsigned i=0;i<4;++i)(void)inb(0x376u);
    outb(IO+2,1);
    outb(IO+3,(uint8_t)lba);
    outb(IO+4,(uint8_t)(lba>>8));
    outb(IO+5,(uint8_t)(lba>>16));
    outb(IO+7,command);
}
static bool read_sector(void *ctx,uint32_t lba,uint8_t data[512]){
    (void)ctx;
    if(!data||lba>=capacity||!ready(false))return false;
    select_sector(lba,0x20u);
    if(!ready(true))return false;
    for(unsigned i=0;i<256;++i){uint16_t w=inw(IO);data[i*2]=(uint8_t)w;data[i*2+1]=(uint8_t)(w>>8);}
    return ready(false);
}
static bool write_sector(void *ctx,uint32_t lba,const uint8_t data[512]){
    (void)ctx;
    if(!data||lba>=capacity||!ready(false))return false;
    select_sector(lba,0x30u);
    if(!ready(true))return false;
    for(unsigned i=0;i<256;++i)outw(IO,(uint16_t)data[i*2]|((uint16_t)data[i*2+1]<<8));
    return ready(false);
}
static bool flush(void *ctx){
    (void)ctx;
    if(!ready(false))return false;
    outb(IO+7,0xe7u); /* FLUSH CACHE (28-bit command). */
    return ready(false);
}
bool ata_secondary_open(persist_io_t *io){
    if(!io)return false;
    outb(IO+6,0xa0u);for(unsigned i=0;i<4;++i)(void)inb(0x376u);
    outb(IO+2,0);outb(IO+3,0);outb(IO+4,0);outb(IO+5,0);
    outb(IO+7,0xecu);
    if(!ready(true))return false;
    uint16_t id[256];for(unsigned i=0;i<256;++i)id[i]=inw(IO);
    /* Allow ONLY deliberately tiny virtual disks (2-4 MiB). */
    uint32_t sectors=(uint32_t)id[60]|((uint32_t)id[61]<<16);
    if(sectors<4096u||sectors>8192u)return false;
    capacity=sectors;
    *io=(persist_io_t){.read=read_sector,.write=write_sector,.sync=flush,.ctx=0};
    return true;
}
#else
bool ata_secondary_open(persist_io_t *io){(void)io;return false;}
#endif
