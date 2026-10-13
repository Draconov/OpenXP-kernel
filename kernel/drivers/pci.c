#include "drivers/pci.h"
uint32_t pci_config_read_io(void *ctx,uint8_t bus,uint8_t slot,uint8_t fn,uint8_t reg){
    (void)ctx;
#ifndef OPENXP_PCI_HOST_TEST
    uint32_t addr=0x80000000u|((uint32_t)bus<<16)|((uint32_t)slot<<11)|
                  ((uint32_t)fn<<8)|(reg&0xfcu);
    __asm__ volatile("outl %0,%1"::"a"(addr),"Nd"((uint16_t)0xcf8));
    uint32_t value;
    __asm__ volatile("inl %1,%0":"=a"(value):"Nd"((uint16_t)0xcfc));
    return value;
#else
    (void)bus;(void)slot;(void)fn;(void)reg;return 0xffffffffu;
#endif
}
size_t pci_enumerate(pci_cfg_read32 read32,void *ctx,pci_visitor_t visit,void *visit_ctx){
    if(!read32)return 0;
    uint8_t seen[256]={0}, queue[256]={0};
    unsigned head=0,tail=1;seen[0]=1;size_t count=0;
    while(head<tail){
        unsigned bus=queue[head++];
        for(unsigned slot=0;slot<32;++slot){
            uint32_t id=read32(ctx,(uint8_t)bus,(uint8_t)slot,0,0);
            if((id&0xffffu)==0xffffu)continue;
            uint32_t header=read32(ctx,(uint8_t)bus,(uint8_t)slot,0,0x0c);
            unsigned functions=(header&0x00800000u)?8:1;
            for(unsigned fn=0;fn<functions;++fn){
                id=read32(ctx,(uint8_t)bus,(uint8_t)slot,(uint8_t)fn,0);
                if((id&0xffffu)==0xffffu)continue;
                uint32_t classes=read32(ctx,(uint8_t)bus,(uint8_t)slot,(uint8_t)fn,8);
                pci_device_t d={
                    .vendor=(uint16_t)id,.device=(uint16_t)(id>>16),
                    .bus=(uint8_t)bus,.slot=(uint8_t)slot,.function=(uint8_t)fn,
                    .base_class=(uint8_t)(classes>>24),
                    .subclass=(uint8_t)(classes>>16),.prog_if=(uint8_t)(classes>>8)
                };
                ++count;if(visit)visit(visit_ctx,&d);
                if(d.base_class==6&&d.subclass==4){
                    uint32_t buses=read32(ctx,(uint8_t)bus,(uint8_t)slot,(uint8_t)fn,0x18);
                    unsigned secondary=(buses>>8)&255u;
                    if(secondary && !seen[secondary] && tail<256){
                        seen[secondary]=1;queue[tail++]=(uint8_t)secondary;
                    }
                }
            }
        }
    }
    return count;
}
