#ifndef OPENXP_APP_FILE_H
#define OPENXP_APP_FILE_H
#include <stdint.h>
/* Bounded in/out request: no kernel pointer is ever exposed to applications.
 * All paths are NUL-terminated and limited to 47 ASCII characters. */
#define APP_FILE_DATA 1024u
#define APP_FILE_PATH 48u
#define APP_FS_LIST 1u
#define APP_FS_READ 2u
#define APP_FS_WRITE 3u
#define APP_FS_DELETE 4u
#define APP_FS_RENAME 5u
#define APP_FS_KIND 6u
#define APP_FS_COPY 7u
typedef struct {
    char path[APP_FILE_PATH];
    char dest[APP_FILE_PATH];
    uint32_t index;
    uint32_t size;
    uint8_t data[APP_FILE_DATA];
} app_file_request_t;
#endif
