#ifndef OPENXP_RAMFS_H
#define OPENXP_RAMFS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Temporary writable in-memory files, lost on reboot.
 * Never exposes a block device or writes to an attached virtual disk. */
#define RAMFS_MAX_FILES 12u
#define RAMFS_FILE_CAPACITY 16384u
#define RAMFS_NAME_CAPACITY 48u

void ramfs_reset(void);
uint64_t ramfs_revision(void);
bool ramfs_write(const char *name, const void *data, size_t size);
bool ramfs_read(const char *name, void *data, size_t capacity, size_t *size);
bool ramfs_remove(const char *name);
/* Atomic metadata-only rename and in-memory copy. Never overwrite a target.
 * Both operations reject invalid paths and preserve source on failure. */
bool ramfs_rename(const char *old_name,const char *new_name);
bool ramfs_copy(const char *from,const char *to);
size_t ramfs_count(void);
bool ramfs_name_at(size_t index, char *name, size_t capacity);
#endif
