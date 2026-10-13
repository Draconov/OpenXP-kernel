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
static uint8_t modifiers(const input_device_t *s) {
    return (uint8_t)((s->shift?KEYMOD_SHIFT:0) | (s->ctrl?KEYMOD_CTRL:0) |
                     (s->alt?KEYMOD_ALT:0) | (s->win?KEYMOD_WIN:0));
}
bool input_decode_keyboard(input_device_t *s,uint8_t code,input_event_t *out) {
    *out=(input_event_t){0};
    if(code==0xe0){s->extended=true;return false;}
    /* Pause uses a distinct E1 sequence and is not used as a hotkey. */
    if(code==0xe1){s->extended=false;return false;}
    bool release=(code&0x80)!=0;
    uint8_t scan=code&0x7f;
    bool ext=s->extended;
    s->extended=false;
    if(!ext && (scan==0x2a || scan==0x36)) {
        if(scan==0x2a)s->left_shift=!release;
        else s->right_shift=!release;
        s->shift=s->left_shift||s->right_shift;
        return false;
    }
    if(scan==0x1d){s->ctrl=!release;return false;}
    if(scan==0x38){s->alt=!release;return false;}
    if(ext && (scan==0x5b || scan==0x5c)) {
        s->win=!release;
        if(release)return false;
        *out=(input_event_t){.kind=INPUT_KEY,.code=KEY_WINDOWS,
            .modifiers=modifiers(s),.x=s->x,.y=s->y,.left=s->left};
        return true;
    }
    if(!ext && scan==0x3a){if(!release)s->caps=!s->caps;return false;}
    if(release)return false;
    input_keycode_t keycode=KEY_NONE;
    if(ext) {
        switch(scan) {
        case 0x48:keycode=KEY_UP;break;
        case 0x50:keycode=KEY_DOWN;break;
        case 0x4b:keycode=KEY_LEFT;break;
        case 0x4d:keycode=KEY_RIGHT;break;
        case 0x47:keycode=KEY_HOME;break;
        case 0x4f:keycode=KEY_END;break;
        case 0x53:keycode=KEY_DELETE;break;
        case 0x1c:keycode=KEY_ENTER;break;
        default: return false;
        }
    } else {
        switch(scan){
        case 0x01:keycode=KEY_ESCAPE;break;
        case 0x0e:keycode=KEY_BACKSPACE;break;
        case 0x0f:keycode=KEY_TAB;break;
        case 0x1c:keycode=KEY_ENTER;break;
        case 0x3c:keycode=KEY_F2;break;
        case 0x3e:keycode=KEY_F4;break;
        case 0x3f:keycode=KEY_F5;break;
        default: if(scan<sizeof(normal) && normal[scan])keycode=KEY_CHARACTER;break;
        }
    }
    if(keycode==KEY_NONE)return false;
    char c=keycode==KEY_CHARACTER?normal[scan]:0;
    if(keycode==KEY_ENTER)c='\n';
    if(keycode==KEY_BACKSPACE)c='\b';
    if(keycode==KEY_TAB)c='\t';
    if(c>='a'&&c<='z'){if(s->shift^s->caps)c=(char)(c-'a'+'A');}
    else if(keycode==KEY_CHARACTER && s->shift && shifted[scan])c=shifted[scan];
    *out=(input_event_t){.kind=INPUT_KEY,.key=c,.code=keycode,
        .modifiers=modifiers(s),.x=s->x,.y=s->y,.left=s->left};
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
    bool right=(s->bytes[0]&2u)!=0;
    *out=(input_event_t){.kind=(button!=s->left||right!=s->right?INPUT_BUTTON:INPUT_MOVE),
                         .x=x,.y=y,.left=button,.right=right};
    bool changed=button!=s->left||right!=s->right||x!=s->x||y!=s->y;
    s->left=button;s->right=right;s->x=x;s->y=y;
    if(!changed)out->kind=INPUT_NONE;
    return changed;
}
void input_queue_reset(input_raw_queue_t *q) { *q=(input_raw_queue_t){0}; }
bool input_queue_push(input_raw_queue_t *q,uint8_t byte,bool auxiliary) {
    unsigned next=(q->head+1u)%INPUT_RAW_CAPACITY;
    if(next==q->tail){++q->dropped;return false;}
    q->bytes[q->head]=(input_raw_byte_t){byte,(uint8_t)auxiliary};
    q->head=next;return true;
}
bool input_queue_pop(input_raw_queue_t *q,input_raw_byte_t *out) {
    if(q->head==q->tail)return false;
    *out=q->bytes[q->tail];q->tail=(q->tail+1u)%INPUT_RAW_CAPACITY;
    return true;
}
#ifndef OPENXP_INPUT_HOST_TEST
static inline uint8_t inb(uint16_t port) {uint8_t b; __asm__ volatile ("inb %1,%0":"=a"(b):"Nd"(port));return b;}
static inline void outb(uint16_t port,uint8_t value) {__asm__ volatile ("outb %0,%1"::"a"(value),"Nd"(port));}
static input_raw_queue_t irq_raw;
static bool irq_configured;
static bool writable(void) {
    for(unsigned n=0;n<100000;++n)if(!(inb(0x64)&2u))return true;
    return false;
}
/* Hardware initialization happens with IF=0, before timer_enable().  Legacy
   8042 IRQ output is explicitly enabled only after draining setup replies. */
static bool wait_reply(uint8_t *byte) {
    for(unsigned i=0;i<100000u;++i)
        if(inb(0x64)&1u){*byte=inb(0x60);return true;}
    return false;
}
void input_hardware_init(void) {
    irq_configured=false;
    input_queue_reset(&irq_raw);
    if(!writable())return;
    outb(0x64,0xad); /* disable keyboard interface during setup */
    if(!writable())return;
    outb(0x64,0xa7); /* disable mouse interface */
    /* Drain any stale bytes or pending mouse ACK. */
    for(unsigned i=0;i<64u && (inb(0x64)&1u);++i)(void)inb(0x60);
    if(!writable())return;
    outb(0x64,0x20);
    uint8_t config=0;
    if(!wait_reply(&config))return;
    if(!writable())return;
    outb(0x64,0xae);
    if(!writable())return;
    outb(0x64,0xa8);
    if(!writable())return;
    outb(0x64,0x60);
    if(!writable())return;
    /* IRQ1 and IRQ12 enabled in the controller; keep existing translation. */
    outb(0x60,(uint8_t)((config&~0x30u)|3u)); /* clear interface clock-disable bits */
    if(!writable())return;
    outb(0x64,0xd4);
    if(!writable())return;
    outb(0x60,0xf4); /* enable mouse streaming */
    uint8_t ack=0;
    if(!wait_reply(&ack) || ack!=0xfa)return;
    /* There must be no deferred PS/2 reply when the IRQ lines are unmasked. */
    for(unsigned i=0;i<64u && (inb(0x64)&1u);++i)(void)inb(0x60);
    irq_configured=true;
}
bool input_irq_ready(void){return irq_configured;}
void input_irq_dispatch(unsigned irq) {
    uint8_t status=inb(0x64);
    if(status&1u){
        uint8_t byte=inb(0x60);
        (void)input_queue_push(&irq_raw,byte,(status&0x20u)!=0);
    }
    if(irq==12u)outb(0xa0,0x20); /* slave before master */
    outb(0x20,0x20);
}
bool input_poll(input_device_t *s,input_event_t *out) {
    /* The timer IRQ can interrupt the foreground consumer: protect tail/head
       updates with a very short IF-disabled section, restoring prior flags. */
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli":"=r"(flags)::"memory");
    input_raw_byte_t byte;
    bool found=input_queue_pop(&irq_raw,&byte);
    if(flags&(1ull<<9))__asm__ volatile ("sti":::"memory");
    if(!found) {
        /* VMs whose i8042 rejects IRQ setup retain legacy polling. */
        if(irq_configured)return false;
        uint8_t status=inb(0x64);
        if(!(status&1u))return false;
        byte=(input_raw_byte_t){inb(0x60),(uint8_t)((status&0x20u)!=0)};
    }
    if(byte.auxiliary)(void)input_decode_mouse(s,byte.value,out);
    else (void)input_decode_keyboard(s,byte.value,out);
    return true;
}
#else
void input_hardware_init(void) {}
bool input_irq_ready(void){return false;}
void input_irq_dispatch(unsigned irq){(void)irq;}
bool input_poll(input_device_t *s,input_event_t *out){(void)s;(void)out;return false;}
#endif
