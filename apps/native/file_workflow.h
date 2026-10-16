#ifndef OPENXP_FILE_WORKFLOW_H
#define OPENXP_FILE_WORKFLOW_H
#include "filetypes.h"
#include <stdint.h>
#include <stddef.h>
/* Common, bounded document routing. A handoff is scoped to its target app;
 * unknown file extensions never become executable just because they have
 * a file name. No ELF arguments or new kernel privileges are required. */
static inline const char *oxp_doc_leaf(const char *s){
    const char *p=s;
    while(*s){if(*s=='/')p=s+1;++s;}
    return p;
}
static inline int oxp_doc_home(const char *s){
    return s[0]=='h'&&s[1]=='o'&&s[2]=='m'&&s[3]=='e'&&s[4]=='/';
}
static inline const char *oxp_doc_program(const char *path){
    oxp_filetype_t t=oxp_filetype(path,0);
    if(t==OXP_FT_DRAWING)return "bin/paint";
    if(t==OXP_FT_TEXT){
        const char *name=oxp_doc_leaf(path),*dot=0;
        for(const char *p=name;*p;++p)if(*p=='.')dot=p;
        if(dot && (oxp_iequal(dot,".md")))return "bin/wordpad";
        return "bin/notepad";
    }
    if(t==OXP_FT_CONFIG)return "bin/notepad";
    return NULL;
}
static inline const char *oxp_doc_mailbox(const char *program){
    if(!program)return NULL;
    if(oxp_iequal(program,"bin/paint"))return "home/open-paint.cfg";
    if(oxp_iequal(program,"bin/wordpad"))return "home/open-wordpad.cfg";
    if(oxp_iequal(program,"bin/notepad"))return "home/open-notepad.cfg";
    return NULL;
}
/* Clipboard is tagged binary to keep file operations distinct from text
 * clipboard contents (which are also shared across applications). */
typedef struct { uint8_t marker[6]; uint8_t move; char path[96]; } oxp_file_clip_t;
static inline void oxp_clip_file_pack(oxp_file_clip_t *c,const char *path,int move){
    static const uint8_t mark[6]={0,'O','X','F','C',1};
    for(unsigned i=0;i<6u;++i)c->marker[i]=mark[i];
    c->move=(uint8_t)!!move;
    unsigned i=0;
    for(;i+1u<sizeof c->path&&path[i];++i)c->path[i]=path[i];
    c->path[i]=0;
}
static inline int oxp_clip_file_valid(const oxp_file_clip_t *c,size_t len){
    static const uint8_t mark[6]={0,'O','X','F','C',1};
    if(len!=sizeof *c||c->move>1)return 0;
    for(unsigned i=0;i<6u;++i)if(c->marker[i]!=mark[i])return 0;
    unsigned n=0;
    while(n<sizeof c->path&&c->path[n]){
        unsigned char ch=(unsigned char)c->path[n++];
        if(ch<33||ch>126||ch=='\\'||ch==':')return 0;
    }
    if(n==0||n>=sizeof c->path)return 0;
    return 1;
}
/* Home RAMFS has a 48-byte filename limit. Respect it before invoking the
 * kernel; collision suffixes are reserved and do not overwrite data. */
static inline int oxp_clip_destination(char out[96],const char *src,
                                      unsigned suffix){
    const char *leaf=oxp_doc_leaf(src);
    if(!*leaf)return 0;
    unsigned n=0;const char prefix[]="home/";
    while(prefix[n]){out[n]=prefix[n];++n;}
    unsigned used=0;
    while(leaf[used]){
        char c=leaf[used];
        if(c=='/'||c=='\\'||c==':')return 0;
        ++used;
        if(used>40u)return 0;
    }
    char extra[12];unsigned elen=0;
    if(suffix){
        const char *p=".copy";
        while(*p)extra[elen++]=*p++;
        if(suffix>1u){
            char rev[6];unsigned rn=0,v=suffix;
            do{rev[rn++]=(char)('0'+v%10u);v/=10u;}while(v&&rn<sizeof rev);
            if(v)return 0;
            while(rn)extra[elen++]=rev[--rn];
        }
    }
    if(n+used+elen>=48u || n+used+elen>=96u)return 0;
    /* Insert the suffix before the extension, so copied documents keep
     * their association: notes.txt -> notes.copy.txt. */
    unsigned split=used;
    if(suffix){
        for(unsigned i=1u;i<used;++i)if(leaf[i]=='.')split=i;
    }
    for(unsigned i=0;i<split;++i)out[n++]=leaf[i];
    for(unsigned i=0;i<elen;++i)out[n++]=extra[i];
    for(unsigned i=split;i<used;++i)out[n++]=leaf[i];
    out[n]=0;return 1;
}
/* Construct a collision-free copy name under any writable Documents
 * directory without trusting an oversized path or changing its extension. */
static inline int oxp_clip_destination_in(char out[96],const char *source,
                                           const char *dir,unsigned suffix){
    char home[96];
    if(!oxp_clip_destination(home,source,suffix) || !dir)return 0;
    unsigned n=0;
    while(dir[n]){if(n>=94u)return 0;out[n]=dir[n];++n;}
    if(n==0)return 0;
    if(dir[n-1u]!='/')out[n++]='/';
    unsigned j=5u;
    while(home[j]){if(n>=47u)return 0;out[n++]=home[j++];}
    out[n]=0;return 1;
}
/* Drop on an editor opens a NEW document window. The source document is
 * unchanged. Never reinterpret executable names as documents. */
static inline const char *oxp_drop_editor(const char *path,const char *title){
    if(!title || !path || !*path)return NULL;
    oxp_filetype_t type=oxp_filetype(path,0);
    if(type==OXP_FT_DRAWING){
        return title[0]=='P' && title[1]=='a' && title[2]=='i' && title[3]=='n' &&
               title[4]=='t'?"bin/paint":NULL;
    }
    if(type!=OXP_FT_TEXT && type!=OXP_FT_CONFIG)return NULL;
    if(title[0]=='N'&&title[1]=='o'&&title[2]=='t'&&title[3]=='e'&&
       title[4]=='p'&&title[5]=='a'&&title[6]=='d')return "bin/notepad";
    if(title[0]=='W'&&title[1]=='o'&&title[2]=='r'&&title[3]=='d'&&
       title[4]=='P'&&title[5]=='a'&&title[6]=='d')return "bin/wordpad";
    return NULL;
}
/* Double click requires two releases on the same entry, from within a
 * finite timer window. Timestamp wrap is handled by unsigned subtraction. */
typedef struct {char path[96];uint64_t tick;int armed;} oxp_double_click_t;
static inline int oxp_double_click(oxp_double_click_t *s,const char *path,uint64_t tick){
    int same=s->armed&&oxp_iequal(s->path,path)&&(tick-s->tick<=35u);
    s->armed=0;
    if(same)return 1;
    unsigned i=0;while(path[i]&&i+1u<sizeof s->path){s->path[i]=path[i];++i;}
    s->path[i]=0;s->tick=tick;s->armed=1;
    return 0;
}
#endif
