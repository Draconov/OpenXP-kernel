#ifndef OPENXP_NATIVE_EDIT_SESSION_H
#define OPENXP_NATIVE_EDIT_SESSION_H
/* Shared bounded plain-text editing operations. No allocation, no syscalls
 * inside the history core. Clipboard functions are provided by caller. */
#include <stddef.h>
#include <stdint.h>
#include "text_edit.h"
#include "text_layout.h"
#include "../../kernel/gui/editor_keycodes.h"
#define OXP_HISTORY_STEPS 12u
#define OXP_EDITOR_MAX 960u
#define OXP_EDIT_LEFT 0x10005u
#define OXP_EDIT_RIGHT 0x10006u
#define OXP_EDIT_UP 0x10003u
#define OXP_EDIT_DOWN 0x10004u
#define OXP_EDIT_GROUP_NONE 0u
#define OXP_EDIT_GROUP_TEXT 1u
#define OXP_EDIT_GROUP_ERASE 2u

typedef struct { char text[OXP_EDITOR_MAX]; size_t length,cursor,anchor; } oxp_edit_snapshot_t;
typedef struct {
    oxp_edit_snapshot_t steps[OXP_HISTORY_STEPS];
    unsigned count,index,group;
} oxp_edit_history_t;
typedef int64_t (*oxp_edit_clip_set)(const void *,size_t);
typedef int64_t (*oxp_edit_clip_get)(void *,size_t);
static inline void oxp_edit_snapshot(oxp_edit_snapshot_t *dst,const char *text,
                                     size_t length,size_t cursor,size_t anchor){
    for(size_t i=0;i<length;++i)dst->text[i]=text[i];
    dst->text[length]=0;
    dst->length=length;dst->cursor=cursor;dst->anchor=anchor;
}
static inline void oxp_history_init(oxp_edit_history_t *h,const char *text,
                                    size_t length,size_t cursor,size_t anchor){
    h->count=1;h->index=h->group=0;
    oxp_edit_snapshot(&h->steps[0],text,length,cursor,anchor);
}
static inline void oxp_history_break(oxp_edit_history_t *h){h->group=0;}
static inline void oxp_history_push(oxp_edit_history_t *h,const char *text,
                                    size_t length,size_t cursor,size_t anchor,
                                    unsigned group){
    if(h->count==0)oxp_history_init(h,text,length,cursor,anchor);
    if(h->index+1<h->count)h->count=h->index+1;
    /* Consecutive keystrokes are one undo action, until cursor moves. */
    if(group && group==h->group && h->index>0 && h->index+1==h->count){
        oxp_edit_snapshot(&h->steps[h->index],text,length,cursor,anchor);
        return;
    }
    if(h->count==OXP_HISTORY_STEPS){
        for(unsigned j=1;j<h->count;++j)h->steps[j-1]=h->steps[j];
        --h->count;
        if(h->index)--h->index;
    }
    ++h->index;h->count=h->index+1;h->group=group;
    oxp_edit_snapshot(&h->steps[h->index],text,length,cursor,anchor);
}
static inline int oxp_history_step(oxp_edit_history_t *h,int direction,char *text,
                                   size_t *length,size_t *cursor,size_t *anchor,size_t cap){
    if(!h->count || (direction<0&&h->index==0) ||
       (direction>0&&h->index+1>=h->count))return 0;
    unsigned index=direction<0?h->index-1:h->index+1;
    oxp_edit_snapshot_t *s=&h->steps[index];
    if(s->length>=cap)return 0;
    for(size_t i=0;i<=s->length;++i)text[i]=s->text[i];
    *length=s->length;*cursor=s->cursor;*anchor=s->anchor;
    h->index=index;h->group=0;return 1;
}
static inline int oxp_replace_selected(char *text,size_t *length,size_t *cursor,
                                        size_t anchor,size_t cap,
                                        const char *insert,size_t add){
    if(!text||!length||!cursor||!insert||cap>OXP_EDITOR_MAX||(!*length&&*cursor))return 0;
    if(*cursor>*length||anchor>*length||*length>=cap)return 0;
    size_t lo=anchor<*cursor?anchor:*cursor,hi=anchor>*cursor?anchor:*cursor;
    if(add>cap-1-(*length-(hi-lo)))return 0;
    /* No embedded NULs or unsupported control sequences in text paste. */
    for(size_t j=0;j<add;++j)
        if((unsigned char)insert[j]<32u && insert[j]!='\n')return 0;
    size_t new_len=*length-(hi-lo)+add;
    if(add>hi-lo){
        size_t shift=add-(hi-lo);
        for(size_t i=*length+1;i>hi;){--i;text[i+shift]=text[i];}
    }else if(add<hi-lo){
        for(size_t i=hi;i<=*length;++i)text[i-(hi-lo-add)]=text[i];
    }
    for(size_t j=0;j<add;++j)text[lo+j]=insert[j];
    text[new_len]=0;*length=new_len;*cursor=lo+add;
    return 1;
}
static inline size_t oxp_edit_line_start(const char *text,size_t len,size_t cursor,unsigned cols){
    oxp_text_position_t p=oxp_text_position(text,len,cursor,cols);
    return oxp_text_offset_at(text,len,cols,p.line,0);
}
static inline size_t oxp_edit_line_end(const char *text,size_t len,size_t cursor,unsigned cols){
    oxp_text_position_t p=oxp_text_position(text,len,cursor,cols);
    return oxp_text_offset_at(text,len,cols,p.line,cols);
}
/* 0 not handled; 1 handled with no content change; 2 document changed.
 * Caller follows caret after the result and redraws. */
static inline int oxp_edit_key(uint32_t key,char *text,size_t *length,
        size_t *cursor,size_t *anchor,size_t cap,unsigned columns,
        oxp_edit_history_t *history,oxp_edit_clip_set put,oxp_edit_clip_get get){
    if(!history||!text||!length||!cursor||!anchor||cap>OXP_EDITOR_MAX||
       !cap||*length>=cap||*cursor>*length||*anchor>*length)return 0;
    size_t lo=*anchor<*cursor?*anchor:*cursor;
    size_t hi=*anchor>*cursor?*anchor:*cursor;
    if(key==OXP_KEY_COPY||key==OXP_KEY_CUT){
        oxp_history_break(history);
        if(lo==hi || !put || put(text+lo,hi-lo)!=(int64_t)(hi-lo))return 1;
        if(key==OXP_KEY_CUT && oxp_replace_selected(text,length,cursor,*anchor,cap,"",0)){
            *anchor=*cursor;oxp_history_push(history,text,*length,*cursor,*anchor,0);return 2;
        }
        return 1;
    }
    if(key==OXP_KEY_PASTE){
        oxp_history_break(history);
        if(!get)return 1;
        char clip[OXP_EDITOR_MAX];int64_t n=get(clip,sizeof clip);
        if(n<=0||n>=(int64_t)sizeof clip)return 1;
        if(!oxp_replace_selected(text,length,cursor,*anchor,cap,clip,(size_t)n))return 1;
        *anchor=*cursor;oxp_history_push(history,text,*length,*cursor,*anchor,0);return 2;
    }
    if(key==OXP_KEY_UNDO||key==OXP_KEY_REDO)
        return oxp_history_step(history,key==OXP_KEY_UNDO?-1:1,text,length,cursor,anchor,cap)?2:1;
    if(key==OXP_KEY_SELECT_ALL){*anchor=0;*cursor=*length;oxp_history_break(history);return 1;}
    int selected=(key==OXP_KEY_SELECT_LEFT||key==OXP_KEY_SELECT_RIGHT||
                  key==OXP_KEY_SELECT_HOME||key==OXP_KEY_SELECT_END||
                  key==OXP_KEY_SELECT_UP||key==OXP_KEY_SELECT_DOWN||
                  key==OXP_KEY_SELECT_DOC_HOME||key==OXP_KEY_SELECT_DOC_END);
    size_t next=*cursor;
    if(key==OXP_EDIT_LEFT||key==OXP_KEY_SELECT_LEFT){if(next)--next;}
    else if(key==OXP_EDIT_RIGHT||key==OXP_KEY_SELECT_RIGHT){if(next<*length)++next;}
    else if(key==OXP_KEY_HOME||key==OXP_KEY_SELECT_HOME)next=oxp_edit_line_start(text,*length,next,columns);
    else if(key==OXP_KEY_END||key==OXP_KEY_SELECT_END)next=oxp_edit_line_end(text,*length,next,columns);
    else if(key==OXP_KEY_DOC_HOME||key==OXP_KEY_SELECT_DOC_HOME)next=0;
    else if(key==OXP_KEY_DOC_END||key==OXP_KEY_SELECT_DOC_END)next=*length;
    else if(key==OXP_EDIT_UP||key==OXP_EDIT_DOWN||key==OXP_KEY_SELECT_UP||key==OXP_KEY_SELECT_DOWN){
        oxp_text_position_t p=oxp_text_position(text,*length,next,columns);
        unsigned row=p.line;
        if(key==OXP_EDIT_UP||key==OXP_KEY_SELECT_UP){if(row)--row;}
        else if(row<oxp_text_position(text,*length,*length,columns).line)++row;
        next=oxp_text_offset_at(text,*length,columns,row,p.col);
    }else if(key==OXP_KEY_DELETE){
        oxp_history_break(history);
        if(lo!=hi){if(!oxp_replace_selected(text,length,cursor,*anchor,cap,"",0))return 1;}
        else if(!oxp_delete(text,length,cursor))return 1;
        *anchor=*cursor;oxp_history_push(history,text,*length,*cursor,*anchor,OXP_EDIT_GROUP_ERASE);return 2;
    }else if(key==8u){
        if(lo!=hi){if(!oxp_replace_selected(text,length,cursor,*anchor,cap,"",0))return 1;}
        else if(!oxp_backspace(text,length,cursor))return 1;
        *anchor=*cursor;oxp_history_push(history,text,*length,*cursor,*anchor,OXP_EDIT_GROUP_ERASE);return 2;
    }else if(key==10u || (key>=32u && key<=126u)){
        char ch=(char)key;
        if(!oxp_replace_selected(text,length,cursor,*anchor,cap,&ch,1))return 1;
        *anchor=*cursor;
        oxp_history_push(history,text,*length,*cursor,*anchor,
                         ch=='\n'||lo!=hi?0u:OXP_EDIT_GROUP_TEXT);
        return 2;
    }else return 0;
    *cursor=next;if(!selected)*anchor=*cursor;
    oxp_history_break(history);return 1;
}
#endif
