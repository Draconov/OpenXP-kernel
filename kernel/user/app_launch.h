#ifndef OPENXP_APP_LAUNCH_H
#define OPENXP_APP_LAUNCH_H
#include <stdbool.h>
#include <stddef.h>
/* At most one pending launch request, consumed by the boot thread. */
bool app_launch_allowed(const char *path);
bool app_launch_submit(const char *path);
bool app_launch_take(char *dest, size_t cap);
void app_launch_reset(void);
#endif
