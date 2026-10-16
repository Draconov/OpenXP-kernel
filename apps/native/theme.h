#ifndef OPENXP_THEME_H
#define OPENXP_THEME_H
#include <stdint.h>
/* Stable indices are persisted as a single byte in home/appearance.cfg. */
#define OXP_THEME_COUNT 3u
#define OXP_THEME_BLUE 0u
#define OXP_THEME_OLIVE 1u
#define OXP_THEME_MIDNIGHT 2u
static inline int oxp_theme_valid(uint32_t i){return i<OXP_THEME_COUNT;}
static inline const char *oxp_theme_name(uint32_t i){
    switch(i){case 1:return "OLIVE GREEN";case 2:return "MIDNIGHT";default:return "AURORA BLUE";}
}
static inline const char *oxp_theme_skin_path(uint32_t i){
    switch(i){case 1:return "windows/resources/olive.oxs";
    case 2:return "windows/resources/midnight.oxs";
    default:return "windows/resources/aurora.oxs";}
}
static inline const char *oxp_theme_widgets_path(uint32_t i){
    switch(i){case 1:return "windows/resources/olive-widgets.oxua";
    case 2:return "windows/resources/midnight-widgets.oxua";
    default:return "windows/resources/aurora-widgets.oxua";}
}
static inline const char *oxp_theme_icons_path(uint32_t i){
    switch(i){case 1:return "windows/resources/olive-icons.oxua";
    case 2:return "windows/resources/midnight-icons.oxua";
    default:return "windows/resources/aurora-icons.oxua";}
}
static inline const char *oxp_theme_menu_path(uint32_t i){
    switch(i){case 1:return "windows/resources/olive-menu.oxua";
    case 2:return "windows/resources/midnight-menu.oxua";
    default:return "windows/resources/aurora-menu.oxua";}
}
static inline const char *oxp_theme_toolbar_path(uint32_t i){
    switch(i){case 1:return "windows/resources/olive-toolbar.oxua";
    case 2:return "windows/resources/midnight-toolbar.oxua";
    default:return "windows/resources/aurora-toolbar.oxua";}
}
static inline const char *oxp_theme_chrome_path(uint32_t i){
    switch(i){case 1:return "windows/resources/olive-chrome.oxua";
    case 2:return "windows/resources/midnight-chrome.oxua";
    default:return "windows/resources/aurora-chrome.oxua";}
}
static inline uint32_t oxp_theme_wallpaper(uint32_t i){
    return i==1u?0x617b42u:i==2u?0x202d4cu:0x1161a8u;
}
static inline uint32_t oxp_theme_taskbar(uint32_t i){
    return i==1u?0x425d36u:i==2u?0x252f4bu:0x225cabu;
}
static inline uint32_t oxp_theme_start(uint32_t i){
    return i==1u?0x64813cu:i==2u?0x5274aeu:0x26a749u;
}
static inline uint32_t oxp_theme_panel(uint32_t i){
    return i==1u?0xe9eddcu:i==2u?0x343c56u:0xf4f8feu;
}
static inline uint32_t oxp_theme_toolbar(uint32_t i){
    return i==1u?0xc9d9bcu:i==2u?0x3b4b70u:0xe0ecfbu;
}
static inline uint32_t oxp_theme_ink(uint32_t i){
    return i==1u?0x37512au:i==2u?0xe1eafb:0x234b7du;
}
static inline uint32_t oxp_theme_accent(uint32_t i){
    return i==1u?0x648c4cu:i==2u?0x546db8u:0x397dc6u;
}
static inline uint32_t oxp_theme_sidebar(uint32_t i){
    return i==1u?0xe1ebd7u:i==2u?0xcad0e1u:0xdceafau;
}
static inline uint32_t oxp_theme_selection(uint32_t i){
    return i==1u?0xbdd5a2u:i==2u?0xb7c6e5u:0xc9e4ffu;
}
#endif
