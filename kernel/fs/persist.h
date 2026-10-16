#ifndef OPENXP_FS_PERSIST_H
#define OPENXP_FS_PERSIST_H
#include <stdbool.h>
#include <stdint.h>
/* Two-slot copy-on-write snapshot journal. The block backend must be a
 * dedicated, explicitly formatted 2 MiB+ disk; NEVER a host/system disk. */
#define PERSIST_BLOCK_BYTES 512u
#define PERSIST_LEGACY_SLOT_SECTORS 400u
/* v2: two large journal slots fit even a fully occupied 48-entry RAMFS
 * inside a dedicated 2 MiB IDE data image. */
#define PERSIST_SLOT_SECTORS 1700u
#define PERSIST_SLOT_A 1u
#define PERSIST_SLOT_B (PERSIST_SLOT_A+PERSIST_SLOT_SECTORS)
#define PERSIST_LEGACY_SLOT_B (PERSIST_SLOT_A+PERSIST_LEGACY_SLOT_SECTORS)
typedef struct {
    bool (*read)(void *ctx,uint32_t lba,uint8_t sector[512]);
    bool (*write)(void *ctx,uint32_t lba,const uint8_t sector[512]);
    bool (*sync)(void *ctx); /* optional cache flush, called before and after commit marker */
    void *ctx;
} persist_io_t;
/* Mounts a disk previously initialized by tools/mkstorage.py. Does not
 * initialize/format any device, and never performs implicit writes. */
bool persist_mount(const persist_io_t *io);
bool persist_is_mounted(void);
/* Recover latest valid snapshot into an EMPTY RAMFS; no disk writes. */
bool persist_restore(void);
/* Commit to the inactive slot, header LAST; old checkpoint stays readable. */
bool persist_commit(void);
uint64_t persist_generation(void);
#endif
