#ifndef OPENXP_SERIAL_H
#define OPENXP_SERIAL_H
#include <stdint.h>
#include <stdbool.h>
void serial_init(void);
void serial_putc(char c);
bool serial_try_getc(char *c);
void serial_puts(const char *text);
void serial_u64(uint64_t value);
void serial_hex64(uint64_t value);
__attribute__((noreturn)) void kernel_halt(void);
#endif
