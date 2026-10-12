#ifndef OPENXP_SHELL_H
#define OPENXP_SHELL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*shell_output_fn)(void *ctx, const char *line);
typedef struct {
    uint64_t (*ticks)(void *ctx);
    uint64_t (*free_pages)(void *ctx);
    size_t (*file_count)(void *ctx);
    bool (*file_name)(void *ctx, size_t index, char *dst, size_t capacity);
    bool (*run)(void *ctx, const char *path);
    size_t (*read_text)(void *ctx,const char *path,char *dst,size_t capacity);
    void *ctx;
} shell_services_t;

typedef struct {
    char input[96];
    char cwd[16];
    size_t used;
    shell_output_fn output;
    void *output_ctx;
    shell_services_t services;
    bool visible;
} shell_t;
void shell_init(shell_t *shell, shell_output_fn output, void *output_ctx, shell_services_t services);
void shell_execute(shell_t *shell, const char *line);
/* Returns true when the terminal changed. Converts \r to Enter; ignores controls. */
bool shell_key(shell_t *shell, char c);
#endif
