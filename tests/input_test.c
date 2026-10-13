#include "input/input.h"
#include <assert.h>
#include <stdio.h>
static bool mouse(input_device_t *s,uint8_t a,uint8_t b,uint8_t c,input_event_t *e) {
    assert(!input_decode_mouse(s,a,e));assert(!input_decode_mouse(s,b,e));return input_decode_mouse(s,c,e);
}
int main(void){
    input_device_t d; input_event_t e;
    input_state_init(&d,640,480);assert(d.x==320 && d.y==240);
    assert(input_decode_keyboard(&d,0x1e,&e)&&e.key=='a');
    assert(!input_decode_keyboard(&d,0x2a,&e));
    assert(input_decode_keyboard(&d,0x1e,&e)&&e.key=='A');
    assert(!input_decode_keyboard(&d,0xaa,&e));
    assert(!input_decode_keyboard(&d,0x3a,&e));
    assert(input_decode_keyboard(&d,0x1e,&e)&&e.key=='A');
    assert(input_decode_keyboard(&d,0x1c,&e)&&e.key=='\n');
    assert(mouse(&d,0x09,5,2,&e));assert(e.kind==INPUT_BUTTON&&d.x==325&&d.y==238);
    assert(mouse(&d,0x08,0,0,&e));assert(e.kind==INPUT_BUTTON&&!e.left);
    assert(!mouse(&d,0x08,0,0,&e));
    assert(mouse(&d,0x0a,0,0,&e) && e.kind==INPUT_BUTTON && e.right && !e.left);
    assert(mouse(&d,0x08,0,0,&e) && e.kind==INPUT_BUTTON && !e.right);
    assert(mouse(&d,0x18,0xff,0,&e)&&d.x==324);
    /* E0-prefixed Windows keys and modifier-aware shortcuts. */
    assert(!input_decode_keyboard(&d,0xe0,&e));
    assert(input_decode_keyboard(&d,0x5b,&e));
    assert(e.code==KEY_WINDOWS && (e.modifiers&KEYMOD_WIN));
    assert(input_decode_keyboard(&d,0x12,&e) && (e.key=='e'||e.key=='E') &&
           (e.modifiers&KEYMOD_WIN));
    assert(!input_decode_keyboard(&d,0xe0,&e));
    assert(!input_decode_keyboard(&d,0xdb,&e));
    assert(!d.win);
    assert(!input_decode_keyboard(&d,0x38,&e));
    assert(input_decode_keyboard(&d,0x0f,&e) && e.code==KEY_TAB &&
           (e.modifiers&KEYMOD_ALT));
    assert(input_decode_keyboard(&d,0x3e,&e) && e.code==KEY_F4);
    assert(!input_decode_keyboard(&d,0xb8,&e));
    assert(!input_decode_keyboard(&d,0xe0,&e));
    assert(input_decode_keyboard(&d,0x48,&e) && e.code==KEY_UP);
    assert(!input_decode_keyboard(&d,0xe0,&e));
    assert(!input_decode_keyboard(&d,0xc8,&e));
    assert(!input_decode_keyboard(&d,0x2a,&e));
    assert(!input_decode_keyboard(&d,0x36,&e));
    assert(!input_decode_keyboard(&d,0xaa,&e));
    assert(d.shift); /* releasing one Shift must retain the other */
    assert(!input_decode_keyboard(&d,0xb6,&e));
    assert(!d.shift);
    input_raw_queue_t q;
    input_queue_reset(&q);
    for(unsigned i=0;i<INPUT_RAW_CAPACITY-1u;++i)
        assert(input_queue_push(&q,(uint8_t)i,(i&1u)!=0));
    assert(!input_queue_push(&q,0,false));
    assert(q.dropped==1);
    for(unsigned i=0;i<INPUT_RAW_CAPACITY-1u;++i) {
        input_raw_byte_t raw;
        assert(input_queue_pop(&q,&raw));
        assert(raw.value==(uint8_t)i && raw.auxiliary==(i&1u));
    }
    input_raw_byte_t raw;
    assert(!input_queue_pop(&q,&raw));
    assert(input_queue_push(&q,0xaa,true));
    assert(input_queue_pop(&q,&raw) && raw.value==0xaa && raw.auxiliary);
    puts("PASS: PS/2 packet decoding, modifiers, Windows key, special keys");
}
