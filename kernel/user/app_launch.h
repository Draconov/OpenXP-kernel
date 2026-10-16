#ifndef OPENXP_APP_LAUNCH_H
#define OPENXP_APP_LAUNCH_H
#include <stdbool.h>
#include <stddef.h>
#define APP_LAUNCH_PATH_MAX 48u
#define APP_LAUNCH_ARG_MAX 96u
typedef struct {
    char executable[APP_LAUNCH_PATH_MAX];
    char argument[APP_LAUNCH_ARG_MAX];
} app_launch_request_t;
/* One bounded launch request, atomically consumed by the boot controller.
 * Allowlisted executables only; optional document path is data, never code. */
bool app_launch_allowed(const char *path);
bool app_launch_submit(const char *path);
bool app_launch_submit_with_arg(const char *path,const char *argument);
bool app_launch_take(char *dest,size_t cap); /* Legacy path-only client */
bool app_launch_take_request(app_launch_request_t *dest);
void app_launch_reset(void);
#endif
