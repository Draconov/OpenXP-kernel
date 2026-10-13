#ifndef OPENXP_THEME_H
#define OPENXP_THEME_H
#include <stdint.h>
/* Appearance scheme is deliberately finite and wire-format stable. */
#define OXP_THEME_COUNT 3u
static inline int oxp_theme_valid(uint32_t i){return i<OXP_THEME_COUNT;}
static inline uint32_t oxp_theme_wallpaper(uint32_t i){
    static const uint32_t colors[OXP_THEME_COUNT]={0x1161a8u,0x617b42u,0x202d4cu};
    return colors[i<OXP_THEME_COUNT?i:0];
}
static inline uint32_t oxp_theme_taskbar(uint32_t i){
    static const uint32_t colors[OXP_THEME_COUNT]={0x225cabu,0x425d36u,0x252f4bu};
    return colors[i<OXP_THEME_COUNT?i:0];
}
static inline uint32_t oxp_theme_start(uint32_t i){
    static const uint32_t colors[OXP_THEME_COUNT]={0x26a749u,0x64813cu,0x5274aeu};
    return colors[i<OXP_THEME_COUNT?i:0];
}
#endif
