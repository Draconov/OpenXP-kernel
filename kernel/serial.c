#include "serial.h"

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
void serial_init(void) {
    outb(0x3f9, 0x00);
    outb(0x3fb, 0x80);
    outb(0x3f8, 0x03);
    outb(0x3f9, 0x00);
    outb(0x3fb, 0x03);
    outb(0x3fa, 0xc7);
    outb(0x3fc, 0x0b);
}
bool serial_try_getc(char *c) {
    if(!c || !(inb(0x3fd)&1u))return false;
    *c=(char)inb(0x3f8);return true;
}
void serial_putc(char c) {
    if (c == '\n') serial_putc('\r');
    for (unsigned i = 0; i < 100000 && !(inb(0x3fd) & 0x20); ++i) {}
    outb(0x3f8, (uint8_t)c);
}
void serial_puts(const char *text) {
    while (*text) serial_putc(*text++);
}
void serial_u64(uint64_t value) {
    char buf[20];
    unsigned n = 0;
    do { buf[n++] = (char)('0' + value % 10); value /= 10; } while (value);
    while (n) serial_putc(buf[--n]);
}
void serial_hex64(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    serial_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4)
        serial_putc(digits[(value >> shift) & 15]);
}
__attribute__((noreturn)) void kernel_halt(void) {
    for (;;) __asm__ volatile ("cli; hlt");
}
