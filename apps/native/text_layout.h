#ifndef OPENXP_NATIVE_TEXT_LAYOUT_H
#define OPENXP_NATIVE_TEXT_LAYOUT_H
#include <stddef.h>
/* Document layout shared between cursor placement and rendering. A column
 * boundary wraps before the *next* character; a final empty line remains
 * visible so that the cursor does not disappear after Enter/full lines. */
typedef struct { unsigned line,col; } oxp_text_position_t;
static inline oxp_text_position_t oxp_text_position(const char *text,size_t length,
                                                     size_t offset,unsigned cols){
    oxp_text_position_t p={0,0};
    if(!cols)cols=1;
    if(offset>length)offset=length;
    for(size_t i=0;i<offset;++i){
        if(p.col==cols){++p.line;p.col=0;}
        if(text[i]=='\n'){++p.line;p.col=0;}
        else ++p.col;
    }
    if(p.col==cols){++p.line;p.col=0;}
    return p;
}
static inline size_t oxp_text_offset_at(const char *text,size_t length,unsigned cols,
                                        unsigned target_line,unsigned target_col){
    if(!cols)cols=1;
    oxp_text_position_t p={0,0};
    size_t closest=0;
    for(size_t i=0;i<=length;++i){
        if(p.col==cols){++p.line;p.col=0;}
        if(p.line==target_line && p.col<=target_col)closest=i;
        if(p.line>target_line || i==length)break;
        if(text[i]=='\n'){++p.line;p.col=0;}
        else ++p.col;
    }
    return closest;
}
#endif
