#include "gui/shell_bridge.h"
#include "input/input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t pixels[32*32+2];
int main(void){
    shell_bridge_reset();
    shell_bridge_attach((void *)1); /* fake attachment: never dereferenced */
    assert(!shell_bridge_active());
    oxp_shell_draw_t cmd={.op=OXP_SHELL_RECT,.x=0,.y=0,.w=3,.h=3,.color=0x345678};
    assert(shell_bridge_submit(7,&cmd,1,5));
    assert(shell_bridge_active() && shell_bridge_take_dirty());
    assert(!shell_bridge_take_dirty());
    input_event_t key={.kind=INPUT_KEY,.code=KEY_WINDOWS};
    assert(shell_bridge_route(&key,false,768));
    oxp_shell_event_t event={0};
    assert(shell_bridge_pop(&event));
    assert(event.type==OXP_SHELL_EVENT_KEY && event.code==KEY_WINDOWS);
    assert(!shell_bridge_pop(&event));
    input_event_t mouse={.kind=INPUT_BUTTON,.left=true,.x=40,.y=740};
    assert(shell_bridge_route(&mouse,false,768));
    assert(shell_bridge_pop(&event) && event.type==OXP_SHELL_EVENT_CLICK && event.x==40);
    key.code=KEY_CHARACTER;key.key='z';
    assert(!shell_bridge_route(&key,false,768));
    assert(shell_bridge_action(OXP_SHELL_MENU,1));
    assert(shell_bridge_menu_open());
    assert(shell_bridge_route(&key,true,768));
    assert(shell_bridge_pop(&event));
    oxp_shell_draw_t invalid=cmd;invalid.w=1025;
    assert(!shell_bridge_submit(7,&invalid,1,6));
    invalid=cmd;invalid.op=OXP_SHELL_TEXT;
    memset(invalid.text,'Q',sizeof(invalid.text));
    assert(!shell_bridge_submit(7,&invalid,1,6));
    fb_surface_t s={(volatile uint8_t *)(pixels+1),32,32,32*4,8,8,8,16,8,0,false,0,0,0,0};
    pixels[0]=0x55aaff;pixels[32*32+1]=0x55aaff;
    shell_bridge_compose(&s);
    assert(pixels[1]==0x345678 && pixels[32*32+1]==0x55aaff);
    shell_bridge_watchdog(304);
    assert(shell_bridge_active());
    shell_bridge_watchdog(306);
    assert(!shell_bridge_active() && shell_bridge_take_dirty());
    assert(!shell_bridge_route(&mouse,false,768));
    assert(shell_bridge_submit(9,&cmd,1,400));
    shell_bridge_owner_exit(8);
    assert(shell_bridge_active());
    shell_bridge_owner_exit(9);
    assert(!shell_bridge_active());
    puts("PASS: Ring 3 shell frame validation, input routing, failure fallback, watchdog");
}
