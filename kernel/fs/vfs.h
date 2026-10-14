#ifndef OPENXP_FS_VFS_H
#define OPENXP_FS_VFS_H
#include <stdbool.h>
#include <stddef.h>
/* First VFS seam: validated OXRD files are read-only, HOME files are writable
 * through RAMFS. The persistent IDE snapshot is a RAMFS *backing store*,
 * not a general block filesystem. No handles or real directories yet. */
typedef enum { VFS_MISSING=0, VFS_BOOT_READONLY=1, VFS_HOME_WRITABLE=2, VFS_DIRECTORY=3 } vfs_kind_t;
size_t vfs_count(void);
/* XP-style virtual directories and executable aliases backed by OXRD. */
bool vfs_list_dir(const char *dir, size_t index, char *name, size_t capacity);
const char *vfs_executable_target(const char *path);
bool vfs_name_at(size_t index, char *name, size_t capacity);
vfs_kind_t vfs_kind(const char *path);
bool vfs_read(const char *path, void *dst, size_t capacity, size_t *length);
/* Bounded read of a read-only boot file. Offset may equal file size (EOF). */
bool vfs_read_slice(const char *path, size_t offset, void *dst, size_t cap, size_t *length);
bool vfs_write(const char *path, const void *src, size_t length);
bool vfs_copy(const char *from, const char *to);
bool vfs_rename(const char *from, const char *to);
bool vfs_remove(const char *path);
#endif
