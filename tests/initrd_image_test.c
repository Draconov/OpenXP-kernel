#include "fs/initrd.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint64_t le64(const uint8_t *p) {
    uint64_t v = 0;
    for (unsigned i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (i * 8);
    return v;
}
static void verify_file(const char *path) {
    const uint8_t *data = NULL;
    size_t size = 0;
    assert(initrd_lookup(path, (const void **)&data, &size));
    assert(size >= 120 && !memcmp(data, "\177ELF", 4));
    assert(data[4] == 2 && data[5] == 1 && le16(data + 16) == 3);
    assert(le16(data + 18) == 62);
    assert(le64(data + 24) == 0x100000 && le64(data + 32) == 64);
    size_t first_offset = le16(data + 56) == 2 ? 176u : 120u;
    assert(le64(data + 72) == first_offset && le64(data + 80) == 0x100000);
    assert(le64(data + 96) == size - first_offset);
    if(first_offset == 176u){
        /* The native app's writable zero-initialized pages are isolated from
         * executable text. Two segments: RX, RW. */
        assert((data[68] & 7u) == 5u);
        assert((data[124] & 7u) == 6u);
    }
}
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    long n = ftell(file);
    assert(n > 0 && n <= INITRD_MAX_SIZE);
    rewind(file);
    uint8_t *data = malloc((size_t)n);
    assert(data);
    assert(fread(data, 1, (size_t)n, file) == (size_t)n);
    assert(fclose(file) == 0);
    assert(initrd_mount(data, (size_t)n) && initrd_count() >= 8);
    verify_file("bin/hello");
    verify_file("bin/fault");
    char name[64];
    for(size_t i=0;i<initrd_count();++i)assert(initrd_name_at(i,name,sizeof(name)));
    assert(!initrd_name_at(initrd_count(),name,sizeof(name)));
    verify_file("bin/calculator");
    verify_file("bin/clock");
    verify_file("bin/notepad");
    verify_file("bin/probe");
    verify_file("bin/canvas");
    const void *text=NULL;size_t length=0;
    assert(initrd_lookup("docs/readme.txt",&text,&length) && length>20 &&
           !memcmp(text,"OpenXP - Exp",12));
    initrd_unmount();
    free(data);
    puts("PASS: built initrd includes seven ELF64 executables and desktop README");
    return 0;
}
