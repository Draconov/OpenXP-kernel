#ifndef OPENXP_APP_FILE_H
#define OPENXP_APP_FILE_H
#include <stdint.h>
/* Bounded in/out request: no kernel pointer is ever exposed to applications.
 * All paths are NUL-terminated and limited to 95 ASCII characters. */
#define APP_FILE_DATA 1024u
#define APP_FILE_PATH 96u
#define APP_FS_LIST 1u
#define APP_FS_READ 2u
#define APP_FS_WRITE 3u
#define APP_FS_DELETE 4u
#define APP_FS_RENAME 5u
#define APP_FS_KIND 6u
#define APP_FS_COPY 7u
#define APP_FS_READ_SLICE 8u /* index=byte offset, size=requested bytes */
#define APP_FS_MKDIR 9u      /* create empty writable HOME directory */
#define APP_FS_RMDIR 10u     /* remove only an empty writable directory */
#define APP_FS_STAT 11u      /* return req.index=kind, req.size=byte length */
typedef struct {
    char path[APP_FILE_PATH];
    char dest[APP_FILE_PATH];
    uint32_t index;
    uint32_t size;
    uint8_t data[APP_FILE_DATA];
} app_file_request_t;
#endif
