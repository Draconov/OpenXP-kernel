#include "drivers/pci.h"
#include <assert.h>
#include <stdio.h>
static uint32_t mock(void *ctx,uint8_t bus,uint8_t slot,uint8_t fn,uint8_t reg){
    (void)ctx;
    if(bus==0 && slot==1 && fn==0){
        if(reg==0)return 0x12348086u;
        if(reg==8)return 0x06040000u;
        if(reg==0x18)return 0x00010100u; /* bridge -> bus 1 */
        if(reg==0x0c)return 0;
    }
    if(bus==1 && slot==2 && fn==0){
        if(reg==0)return 0x100e8086u;
        if(reg==8)return 0x02000000u; /* ethernet */
        if(reg==0x0c)return 0x00800000u; /* multifunction */
    }
    if(bus==1 && slot==2 && fn==1){
        if(reg==0)return 0x29348086u;
        if(reg==8)return 0x0c030000u; /* USB */
    }
    return 0xffffffffu;
}
static unsigned classes[256],visits;
static void collect(void *ctx,const pci_device_t *d){
    (void)ctx;assert(d->vendor==0x8086);
    ++visits;++classes[d->base_class];
}
int main(void){
    assert(pci_enumerate(0,0,collect,0)==0);
    assert(pci_enumerate(mock,0,collect,0)==3);
    assert(visits==3 && classes[6]==1 && classes[2]==1 && classes[12]==1);
    puts("PASS: read-only PCI bridge traversal and USB/network classification");
}
