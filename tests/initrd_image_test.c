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
    assert(le64(data + 72) == 120 && le64(data + 80) == 0x100000);
    assert(le64(data + 96) == size - 120);
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
    assert(initrd_mount(data, (size_t)n) && initrd_count() == 3);
    verify_file("bin/hello");
    verify_file("bin/fault");
    char name[64];
    assert(initrd_name_at(2,name,sizeof(name)) && !strcmp(name,"docs/readme.txt"));
    assert(!initrd_name_at(3,name,sizeof(name)));
    const void *text=NULL;size_t length=0;
    assert(initrd_lookup("docs/readme.txt",&text,&length) && length>20 &&
           !memcmp(text,"OpenXP 1.8.0",12));
    initrd_unmount();
    free(data);
    puts("PASS: built initrd archive contains two ELF64 programs plus desktop README");
    return 0;
}
