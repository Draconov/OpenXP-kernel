#ifndef OPENXP_FILETYPES_H
#define OPENXP_FILETYPES_H
#include <stddef.h>
/* Extension classification never grants execution permission: launch still
 * goes through the kernel allowlist, and native executables are ELF64. */
typedef enum { OXP_FT_DIR, OXP_FT_EXE, OXP_FT_TEXT, OXP_FT_BITMAP,
               OXP_FT_DRAWING, OXP_FT_CONFIG, OXP_FT_OTHER } oxp_filetype_t;
static inline int oxp_iequal(const char *a,const char *b){
    while(*a&&*b){char x=*a++,y=*b++;
        if(x>='A'&&x<='Z')x=(char)(x+32);
        if(y>='A'&&y<='Z')y=(char)(y+32);
        if(x!=y)return 0;}
    return !*a&&!*b;
}
static inline oxp_filetype_t oxp_filetype(const char *path,int directory){
    if(directory)return OXP_FT_DIR;
    const char *dot=0;for(const char *s=path;*s;++s){if(*s=='/')dot=0;else if(*s=='.')dot=s;}
    if(!dot)return OXP_FT_OTHER;
    if(oxp_iequal(dot,".exe"))return OXP_FT_EXE;
    if(oxp_iequal(dot,".txt")||oxp_iequal(dot,".log")||oxp_iequal(dot,".md"))return OXP_FT_TEXT;
    if(oxp_iequal(dot,".bmp"))return OXP_FT_BITMAP;
    if(oxp_iequal(dot,".oxp"))return OXP_FT_DRAWING;
    if(oxp_iequal(dot,".cfg")||oxp_iequal(dot,".ini"))return OXP_FT_CONFIG;
    return OXP_FT_OTHER;
}
static inline const char *oxp_filetype_label(oxp_filetype_t t){
    static const char *const names[]={"File folder","OpenXP application","Text document",
       "Bitmap image","OpenXP drawing","Configuration","File"};
    return names[t<=OXP_FT_OTHER?t:OXP_FT_OTHER];
}
static inline const char *oxp_filetype_app(oxp_filetype_t t){
    if(t==OXP_FT_TEXT||t==OXP_FT_CONFIG)return "bin/wordpad";
    if(t==OXP_FT_DRAWING)return "bin/paint";
    if(t==OXP_FT_BITMAP)return "bin/settings";
    return 0;
}
#endif
