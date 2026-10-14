#ifndef OPENXP_INITRD_H
#define OPENXP_INITRD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Minimal read-only OXRD v1 archive. Every file is checksummed at mount.
 * All names are relative ASCII paths without traversal segments. */
#define INITRD_MAX_FILES 64u
#define INITRD_MAX_SIZE (4u * 1024u * 1024u)
bool initrd_mount(const void *data, size_t length);
bool initrd_lookup(const char *path, const void **data, size_t *length);
size_t initrd_count(void);
bool initrd_name_at(size_t index, char *dst, size_t capacity);
void initrd_unmount(void);
#endif
