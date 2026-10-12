#include "fs/initrd.h"
#include <stdint.h>
#include <stddef.h>

#define HEADER_BYTES 16u
#define RECORD_BYTES 64u
#define NAME_BYTES 48u
#define FORMAT_VERSION 1u

static const uint8_t *mounted;
static size_t mounted_length;
static uint32_t mounted_count;

static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static bool valid_name(const uint8_t *name) {
    size_t n = 0;
    while (n < NAME_BYTES && name[n]) {
        uint8_t c = name[n];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' ||
              c == '.' || c == '/')) return false;
        ++n;
    }
    if (n == 0 || n == NAME_BYTES || name[0] == '/' || name[n - 1] == '/') return false;
    size_t begin = 0;
    for (size_t i = 0; i <= n; ++i) {
        if (i == n || name[i] == '/') {
            if (i == begin || (i - begin == 1 && name[begin] == '.') ||
                (i - begin == 2 && name[begin] == '.' && name[begin + 1] == '.'))
                return false;
            begin = i + 1;
        }
    }
    for (size_t i = n + 1; i < NAME_BYTES; ++i) if (name[i]) return false;
    return true;
}
static bool same_name(const uint8_t *a, const uint8_t *b) {
    for (size_t i = 0; i < NAME_BYTES; ++i) if (a[i] != b[i]) return false;
    return true;
}
static uint32_t crc32(const uint8_t *p, size_t n) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ ((uint32_t)-(int32_t)(crc & 1u) & UINT32_C(0xedb88320));
    }
    return ~crc;
}
void initrd_unmount(void) {
    mounted = NULL;
    mounted_length = 0;
    mounted_count = 0;
}
bool initrd_mount(const void *data, size_t length) {
    initrd_unmount();
    if (!data || length < HEADER_BYTES || length > INITRD_MAX_SIZE) return false;
    const uint8_t *p = data;
    if (p[0] != 'O' || p[1] != 'X' || p[2] != 'R' || p[3] != 'D' ||
        get32(p + 4) != FORMAT_VERSION) return false;
    uint32_t count = get32(p + 8);
    if (!count || count > INITRD_MAX_FILES || get32(p + 12) != length) return false;
    size_t end_table = HEADER_BYTES + (size_t)count * RECORD_BYTES;
    if (end_table > length) return false;
    /* Require canonical tightly packed sorted records: no overlapping files,
     * unreferenced gaps, or trailing data in the mounted archive. */
    size_t cursor = end_table;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t *entry = p + HEADER_BYTES + i * RECORD_BYTES;
        if (!valid_name(entry)) return false;
        for (uint32_t j = 0; j < i; ++j)
            if (same_name(entry, p + HEADER_BYTES + j * RECORD_BYTES)) return false;
        uint32_t off = get32(entry + 48), size = get32(entry + 52);
        uint32_t checksum = get32(entry + 56), flags = get32(entry + 60);
        if (flags != 1 || !size || off != cursor || size > length - cursor) return false;
        if (crc32(p + off, size) != checksum) return false;
        cursor += size;
    }
    if (cursor != length) return false;
    mounted = p;
    mounted_length = length;
    mounted_count = count;
    return true;
}
size_t initrd_count(void) { return mounted ? mounted_count : 0; }
bool initrd_lookup(const char *path, const void **data, size_t *length) {
    if (!mounted || !path || !data || !length) return false;
    size_t n = 0;
    while (path[n] && n < NAME_BYTES) ++n;
    if (n == 0 || n >= NAME_BYTES) return false;
    for (uint32_t i = 0; i < mounted_count; ++i) {
        const uint8_t *e = mounted + HEADER_BYTES + i * RECORD_BYTES;
        bool match = true;
        for (size_t j = 0; j <= n; ++j)
            if ((uint8_t)path[j] != e[j]) { match = false; break; }
        if (!match) continue;
        uint32_t off = get32(e + 48), size = get32(e + 52);
        if (off > mounted_length || size > mounted_length - off) return false;
        *data = mounted + off;
        *length = size;
        return true;
    }
    return false;
}

bool initrd_name_at(size_t index, char *dst, size_t capacity) {
    if (!mounted || index >= mounted_count || !dst || !capacity) return false;
    const uint8_t *entry = mounted + HEADER_BYTES + index * RECORD_BYTES;
    size_t n=0;
    while (n < NAME_BYTES && entry[n]) ++n;
    if (n == NAME_BYTES || n + 1 > capacity) return false;
    for (size_t i=0;i<n;++i) dst[i]=(char)entry[i];
    dst[n]=0;
    return true;
}
