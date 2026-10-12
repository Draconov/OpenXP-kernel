#include "fs/initrd.h"
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static uint8_t buf[256];
static void put32(size_t n, uint32_t x) {
    for (unsigned i = 0; i < 4; ++i) buf[n+i] = (uint8_t)(x >> (8*i));
}
static uint32_t crc32(const void *data, size_t n) {
    const uint8_t *p = data;
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ ((uint32_t)-(int)(crc & 1u) & 0xedb88320u);
    }
    return ~crc;
}
static void make_archive(void) {
    memset(buf, 0, sizeof(buf));
    memcpy(buf, "OXRD", 4);
    put32(4, 1); put32(8, 2); put32(12, 147);
    memcpy(buf+16, "bin/hello", 9);
    memcpy(buf+80, "bin/fault", 9);
    memcpy(buf+144, "HI!", 3);
    memcpy(buf+147, "UD2", 3);
    /* two records 16+2*64 -> data at byte 144, total 150 */
    put32(12, 150);
    put32(64, 144); put32(68, 3); put32(72, crc32(buf+144, 3)); put32(76, 1);
    put32(128, 147); put32(132, 3); put32(136, crc32(buf+147, 3)); put32(140, 1);
}
int main(void) {
    const void *out = NULL;
    size_t length = 0;
    assert(!initrd_lookup("bin/hello", &out, &length));
    make_archive();
    assert(initrd_mount(buf, 150) && initrd_count() == 2);
    assert(initrd_lookup("bin/hello", &out, &length) && length == 3 &&
           !memcmp(out, "HI!", 3));
    assert(initrd_lookup("bin/fault", &out, &length) && length == 3 &&
           !memcmp(out, "UD2", 3));
    assert(!initrd_lookup("bin/nope", &out, &length));
    assert(!initrd_lookup("bin/hello-extra", &out, &length));
    assert(!initrd_lookup("", &out, &length));
    assert(!initrd_lookup("bin/hello", NULL, &length));
    assert(!initrd_lookup("bin/hello", &out, NULL));
    assert(!initrd_mount(buf, 149) && !initrd_count());
    make_archive(); buf[146] ^= 1; /* corrupted file */
    assert(!initrd_mount(buf, 150));
    make_archive(); memcpy(buf+16, "bin/../hi", 9);
    assert(!initrd_mount(buf, 150));
    make_archive(); memcpy(buf+80, "bin/hello", 9);
    assert(!initrd_mount(buf, 150));
    make_archive(); put32(128, 146); /* overlap */
    assert(!initrd_mount(buf, 150));
    make_archive(); put32(132, 4); /* beyond end */
    assert(!initrd_mount(buf, 150));
    make_archive(); put32(8, INITRD_MAX_FILES+1);
    assert(!initrd_mount(buf, 150));
    make_archive(); put32(4, 42);
    assert(!initrd_mount(buf, 150));
    make_archive(); put32(140, 0);
    assert(!initrd_mount(buf, 150));
    make_archive();
    assert(initrd_mount(buf, 150));
    initrd_unmount(); assert(initrd_count() == 0);
    puts("PASS: initrd mount/lookup, CRC, names, overlap, truncated and corrupt archives");
    return 0;
}
