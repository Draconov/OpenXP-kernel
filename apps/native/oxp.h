#ifndef OPENXP_NATIVE_APP_H
#define OPENXP_NATIVE_APP_H
#include <stdint.h>
#include <stddef.h>
#include "../../kernel/user/app_file.h"
#define OXP_DRAW_FILL 1u
#define OXP_DRAW_CLEAR 2u
#define OXP_DRAW_GLYPH 3u
#define OXP_CANVAS_W 320u
#define OXP_CANVAS_H 180u
typedef struct { uint32_t op,x,y,w,h,color; } oxp_draw_cmd_t;
#define OXP_CLOSE 0x10000u
#define OXP_SAVE 0x10001u
#define OXP_OPEN 0x10002u
#define OXP_CLICK 0x80000000u
#define OXP_RIGHT_CLICK 0x40000000u
#define OXP_UP 0x10003u
#define OXP_DOWN 0x10004u
#define OXP_LEFT 0x10005u
#define OXP_RIGHT 0x10006u
static inline unsigned oxp_click_x(uint32_t e){return (e >> 10) & 1023u;}
static inline unsigned oxp_click_y(uint32_t e){return e & 1023u;}
static inline uint64_t oxp_call(uint64_t id,uint64_t a,uint64_t b) {
    register uint64_t rax __asm__("rax")=id;
    register uint64_t rdi __asm__("rdi")=a;
    register uint64_t rsi __asm__("rsi")=b;
    __asm__ volatile("int $0x80" : "+a"(rax) : "D"(rdi),"S"(rsi) : "memory","cc");
    return rax;
}
/* Capability IPC: opaque per-process handles, copied 1..64-byte messages.
 * Negative return values are errno-like; nonblocking send/receive. */
#define OXP_IPC_MSG_MAX 64u
static inline uint64_t oxp_call3(uint64_t id,uint64_t a,uint64_t b,uint64_t c) {
    register uint64_t rax __asm__("rax")=id;
    register uint64_t rdi __asm__("rdi")=a;
    register uint64_t rsi __asm__("rsi")=b;
    register uint64_t rdx __asm__("rdx")=c;
    __asm__ volatile("int $0x80" : "+a"(rax) : "D"(rdi),"S"(rsi),"d"(rdx) : "memory","cc");
    return rax;
}
static inline int64_t oxp_listen(void){return (int64_t)oxp_call(13,0,0);}
static inline int64_t oxp_accept(void){return (int64_t)oxp_call(14,0,0);}
static inline int64_t oxp_connect(void){return (int64_t)oxp_call(15,0,0);}
static inline int64_t oxp_send(uint64_t h,const void *p,size_t n){return (int64_t)oxp_call3(16,h,(uint64_t)p,n);}
static inline int64_t oxp_recv(uint64_t h,void *p,size_t cap){return (int64_t)oxp_call3(17,h,(uint64_t)p,cap);}
static inline int64_t oxp_close(uint64_t h){return (int64_t)oxp_call(18,h,0);}
/* 0 means wait for incoming connection; other handles wait for messages.
 * 0 ticks = indefinite park; positive ticks = relative timer deadline. */
static inline int64_t oxp_ipc_wait(uint64_t h,uint64_t ticks){return (int64_t)oxp_call(19,h,ticks);}
/* Trusted windowd service only: wake on new connection, data, or disconnect. */
static inline int64_t oxp_ipc_wait_any(uint64_t ticks){return oxp_ipc_wait(UINT64_MAX,ticks);}
static inline size_t oxp_len(const char *s){size_t n=0;while(s[n])++n;return n;}
static inline int oxp_open(const char *s){return (int)oxp_call(6,(uint64_t)s,oxp_len(s));}
static inline int oxp_text(const char *s){return (int)oxp_call(7,(uint64_t)s,oxp_len(s));}
static inline int oxp_draw(const oxp_draw_cmd_t *commands,size_t count){
    return (int)oxp_call(11,(uint64_t)commands,count);
}
static inline int oxp_fill(unsigned x,unsigned y,unsigned w,unsigned h,uint32_t color){
    oxp_draw_cmd_t cmd={OXP_DRAW_FILL,x,y,w,h,color};
    return oxp_draw(&cmd,1);
}
static inline int oxp_label(unsigned x,unsigned y,const char *text,unsigned scale,uint32_t color){
    oxp_draw_cmd_t cmd[24];size_t used=0;
    if(!text || scale<1 || scale>2)return -1;
    for(;*text && x+5u*scale<=OXP_CANVAS_W; ++text,x+=6u*scale){
        if(*text==' ')continue;
        if(*text<32 || *text>126)continue;
        cmd[used++]=(oxp_draw_cmd_t){OXP_DRAW_GLYPH,x,y,(uint32_t)(unsigned char)*text,scale,color};
        if(used==24){if(oxp_draw(cmd,used))return -1;used=0;}
    }
    return used?oxp_draw(cmd,used):0;
}
static inline int64_t oxp_file(unsigned op,app_file_request_t *req){
    return (int64_t)oxp_call(27,op,(uint64_t)req);
}
static inline int64_t oxp_launch(const char *path){
    return (int64_t)oxp_call(28,(uint64_t)path,oxp_len(path));
}
static inline void oxp_copy(char *dst,const char *src,size_t cap){
    if(!cap)return;size_t i=0;while(i+1<cap && src[i]){dst[i]=src[i];++i;}dst[i]=0;
}
static inline int oxp_hit(uint32_t e,unsigned x,unsigned y,unsigned w,unsigned h){
    if(!(e&(OXP_CLICK|OXP_RIGHT_CLICK)))return 0;
    unsigned ex=oxp_click_x(e),ey=oxp_click_y(e);
    return ex>=x+16 && ey>=y+43 && ex-x-16<w && ey-y-43<h;
}
static inline uint32_t oxp_event(void){return (uint32_t)oxp_call(8,0,0);}
/* Zero waits for an event without a deadline. For periodic refresh pass a
 * bounded timeout in PIT ticks; a zero result means the timer expired. */
static inline uint32_t oxp_wait(uint64_t ticks){return (uint32_t)oxp_call(12,ticks,0);}
static inline uint64_t oxp_ticks(void){return oxp_call(3,0,0);}
static inline uint64_t oxp_read(void *dst,size_t cap){return oxp_call(9,(uint64_t)dst,cap);}
static inline uint64_t oxp_write(const void *src,size_t n){return oxp_call(10,(uint64_t)src,n);}
static inline void oxp_exit(uint64_t rc){oxp_call(2,rc,0);for(;;)__asm__ volatile("pause");}
static inline void oxp_puts(char *dst,size_t *pos,size_t cap,const char *src){
    while(*src && *pos+1<cap)dst[(*pos)++]=*src++;
    dst[*pos]=0;
}
static inline void oxp_u64(char *dst,size_t *pos,size_t cap,uint64_t n){
    char rev[24];size_t z=0;do{rev[z++]=(char)('0'+n%10);n/=10;}while(n&&z<24);
    while(z&&*pos+1<cap)dst[(*pos)++]=rev[--z];dst[*pos]=0;
}
#endif
