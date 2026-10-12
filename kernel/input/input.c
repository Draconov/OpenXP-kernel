#include "input/input.h"
#include <stddef.h>
static const char normal[0x3a] = {
    [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',
    [0x08]='7',[0x09]='8',[0x0a]='9',[0x0b]='0',[0x0c]='-',[0x0d]='=',
    [0x0e]='\b',[0x0f]='\t',[0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',
    [0x14]='t',[0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',
    [0x1a]='[',[0x1b]=']',[0x1c]='\n',[0x1e]='a',[0x1f]='s',[0x20]='d',
    [0x21]='f',[0x22]='g',[0x23]='h',[0x24]='j',[0x25]='k',[0x26]='l',
    [0x27]=';',[0x28]='\'',[0x29]='`',[0x2b]='\\',[0x2c]='z',[0x2d]='x',
    [0x2e]='c',[0x2f]='v',[0x30]='b',[0x31]='n',[0x32]='m',[0x33]=',',
    [0x34]='.',[0x35]='/',[0x39]=' '
};
static const char shifted[0x3a] = {
    [0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',[0x07]='^',
    [0x08]='&',[0x09]='*',[0x0a]='(',[0x0b]=')',[0x0c]='_',[0x0d]='+',
    [0x1a]='{',[0x1b]='}',[0x27]=':',[0x28]='"',[0x29]='~',
    [0x2b]='|',[0x33]='<',[0x34]='>',[0x35]='?'
};
void input_state_init(input_device_t *s,int width,int height) {
    *s=(input_device_t){0};s->width=width;s->height=height;
    s->x=width/2;s->y=height/2;
}
bool input_decode_keyboard(input_device_t *s,uint8_t code,input_event_t *out) {
    *out=(input_event_t){0};
    if(code==0xe0){s->extended=true;return false;}
    bool release=(code&0x80)!=0;
    uint8_t scan=code&0x7f;
    if(scan==0x2a||scan==0x36){s->shift=!release; s->extended=false;return false;}
    if(scan==0x3a&&!release){s->caps=!s->caps;s->extended=false;return false;}
    if(s->extended){ s->extended=false;return false; }
    if(release||scan>=sizeof(normal))return false;
    char c=normal[scan]; if(!c)return false;
    if(c>='a'&&c<='z'){if(s->shift^s->caps)c=(char)(c-'a'+'A');}
    else if(s->shift && shifted[scan])c=shifted[scan];
    *out=(input_event_t){.kind=INPUT_KEY,.key=c,.x=s->x,.y=s->y,.left=s->left};
    return true;
}
bool input_decode_mouse(input_device_t *s,uint8_t b,input_event_t *out) {
    *out=(input_event_t){0};
    if(s->state==0 && (!(b&0x08)|| (b&0xc0)))return false;
    s->bytes[s->state++]=b;
    if(s->state<3)return false;
    s->state=0;
    if(s->bytes[0]&0xc0)return false;
    int dx=(s->bytes[0]&0x10) ? (int)s->bytes[1]-256 : s->bytes[1];
    int dy=(s->bytes[0]&0x20) ? (int)s->bytes[2]-256 : s->bytes[2];
    int x=s->x+dx,y=s->y-dy;
    if(x<0)x=0;
    if(y<0)y=0;
    if(x>=s->width)x=s->width-1;
    if(y>=s->height)y=s->height-1;
    bool button=(s->bytes[0]&1u)!=0;
    *out=(input_event_t){.kind=(button!=s->left?INPUT_BUTTON:INPUT_MOVE),.x=x,.y=y,.left=button};
    bool changed=button!=s->left||x!=s->x||y!=s->y;
    s->left=button;s->x=x;s->y=y;
    if(!changed)out->kind=INPUT_NONE;
    return changed;
}
#ifndef OPENXP_INPUT_HOST_TEST
static inline uint8_t inb(uint16_t port) {uint8_t b; __asm__ volatile ("inb %1,%0":"=a"(b):"Nd"(port));return b;}
static inline void outb(uint16_t port,uint8_t value) {__asm__ volatile ("outb %0,%1"::"a"(value),"Nd"(port));}
static bool writable(void) {
    for(unsigned n=0;n<100000;++n)if(!(inb(0x64)&2u))return true;
    return false;
}
void input_hardware_init(void) {
    /* Legacy PC/AT-compatible i8042 controller, QEMU PS/2 mouse.
       Disable mouse IRQ use; the shell polls the controller instead. */
    if(writable())outb(0x64,0xa8);  /* enable auxiliary port */
    if(writable())outb(0x64,0x20);  /* read configuration byte */
    uint8_t config=0;
    for(unsigned i=0;i<100000;++i)if(inb(0x64)&1u){config=inb(0x60);break;}
    if(writable())outb(0x64,0x60);
    if(writable())outb(0x60,(uint8_t)(config&~3u)); /* keep IRQ1/12 masked */
    if(writable())outb(0x64,0xd4);
    if(writable())outb(0x60,0xf4); /* enable mouse reporting */
}
bool input_poll(input_device_t *s,input_event_t *out) {
    uint8_t status=inb(0x64);
    if(!(status&1u))return false;
    uint8_t data=inb(0x60);
    if(status&0x20u)return input_decode_mouse(s,data,out);
    return input_decode_keyboard(s,data,out);
}
#else
void input_hardware_init(void) {}
bool input_poll(input_device_t *s,input_event_t *out){(void)s;(void)out;return false;}
#endif
