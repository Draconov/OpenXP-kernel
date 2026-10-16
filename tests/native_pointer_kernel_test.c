#include "gui/native.h"
#include <assert.h>
#include <stdio.h>
#define EVT(kind,x,y) ((uint32_t)(kind)|(((uint32_t)(x)&1023u)<<10)|((uint32_t)(y)&1023u))
int main(void){
    native_reset();
    assert(!native_pointer_enable(777));
    assert(!native_pointer_enabled_slot(0));
    assert(native_open(123,"Mouse",5));
    assert(!native_pointer_enabled_slot(0)); /* legacy apps remain untouched */
    assert(native_pointer_enable(123));
    assert(native_pointer_enabled_slot(0));
    native_push_event_slot(0,EVT(NATIVE_EVENT_POINTER_DOWN,24,54));
    for(unsigned i=0;i<120u;++i)
        native_push_event_slot(0,EVT(NATIVE_EVENT_POINTER_MOVE,24+i,54));
    native_push_event_slot(0,'Q');
    native_push_event_slot(0,EVT(NATIVE_EVENT_POINTER_UP,143,54));
    assert(native_next_event(123)==EVT(NATIVE_EVENT_POINTER_DOWN,24,54));
    assert(native_next_event(123)==EVT(NATIVE_EVENT_POINTER_MOVE,143,54));
    assert(native_next_event(123)=='Q');
    assert(native_next_event(123)==EVT(NATIVE_EVENT_POINTER_UP,143,54));
    assert(!native_next_event(123));
    for(unsigned i=0;i<NATIVE_EVENT_CAP-1u;++i)native_push_event_slot(0,'a'+i);
    native_push_event_slot(0,EVT(NATIVE_EVENT_POINTER_MOVE,1,1)); /* drop if full */
    native_push_event_slot(0,EVT(NATIVE_EVENT_POINTER_UP,2,2)); /* must survive */
    unsigned saw_up=0;
    for(uint32_t e;(e=native_next_event(123))!=0;){
        if((e&NATIVE_EVENT_POINTER_MASK)==NATIVE_EVENT_POINTER_UP)saw_up++;
        assert((e&NATIVE_EVENT_POINTER_MASK)!=NATIVE_EVENT_POINTER_MOVE);
    }
    assert(saw_up==1);
    native_owner_exit(123);
    assert(native_open(456,"Fresh",5));
    assert(!native_pointer_enabled_slot(0)); /* don't leak subscription across PID reuse */
    puts("PASS: per-owner subscription, motion coalescing, control priority, reset on close");
    return 0;
}
