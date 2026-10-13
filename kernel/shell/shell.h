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
    /* Optional diagnostic snapshot; false for vacant/out-of-range slots. */
    bool (*process_entry)(void *ctx,size_t slot,uint64_t *pid,unsigned *state);
    bool (*file_copy)(void *ctx,const char *from,const char *to);
    bool (*file_rename)(void *ctx,const char *from,const char *to);
    bool (*file_remove)(void *ctx,const char *path);
} shell_services_t;

typedef struct {
    char input[96];
    char cwd[16];
    size_t used;
    size_t cursor;
    char history[8][96];
    char draft[96];
    unsigned history_count,history_next;
    int history_index; /* -1 = fresh input, 0 = latest command */
    shell_output_fn output;
    void *output_ctx;
    shell_services_t services;
    bool visible;
} shell_t;
void shell_init(shell_t *shell, shell_output_fn output, void *output_ctx, shell_services_t services);
void shell_execute(shell_t *shell, const char *line);
/* Returns true when the terminal changed. Converts \r to Enter; ignores controls. */
bool shell_key(shell_t *shell, char c);
/* Editing and browsing are separate from text input to support PS/2 arrows. */
bool shell_history(shell_t *shell,int direction);
bool shell_left(shell_t *shell);
bool shell_right(shell_t *shell);
bool shell_home(shell_t *shell);
bool shell_end(shell_t *shell);
bool shell_delete(shell_t *shell);
#endif
