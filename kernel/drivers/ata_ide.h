#ifndef OPENXP_ATA_IDE_H
#define OPENXP_ATA_IDE_H
#include <stdbool.h>
#include "fs/persist.h"
/* Optional, unsafe-experimental x86 ATA PIO backend, secondary master ONLY.
 * Build with -DOPENXP_ENABLE_EXPERIMENTAL_IDE_STORAGE. Never touches primary. */
bool ata_secondary_open(persist_io_t *io);
#endif
