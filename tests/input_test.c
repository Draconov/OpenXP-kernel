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
    assert(mouse(&d,0x18,0xff,0,&e)&&d.x==324);
    puts("PASS: PS/2 set-1 keyboard and 3-byte mouse decoding");
}
