#ifndef OPENXP_DRIVERS_PCI_H
#define OPENXP_DRIVERS_PCI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint32_t (*pci_cfg_read32)(void *ctx,uint8_t bus,uint8_t slot,uint8_t fn,uint8_t reg);
typedef struct {
    uint16_t vendor, device;
    uint8_t bus,slot,function,base_class,subclass,prog_if;
} pci_device_t;
typedef void (*pci_visitor_t)(void *ctx,const pci_device_t *device);
/* Read-only PCI discovery; searches bus 0 and buses behind normal PCI bridges.
 * This does not configure DMA, bus mastering, MSI or bind any driver. */
size_t pci_enumerate(pci_cfg_read32 read32,void *ctx,pci_visitor_t visit,void *visit_ctx);
uint32_t pci_config_read_io(void *ctx,uint8_t bus,uint8_t slot,uint8_t fn,uint8_t reg);
#endif
