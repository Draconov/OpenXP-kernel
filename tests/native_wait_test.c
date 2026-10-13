#include "gui/native.h"
#include "sched/scheduler.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
static uint64_t tid=9, wake_id, last_timeout;
static unsigned parks,wakes;
static unsigned action;
uint64_t scheduler_current_id(void) { return tid; }
bool scheduler_wake(uint64_t thread_id) {
    wake_id=thread_id; ++wakes; return thread_id==tid;
}
static bool simulated_sleep(uint64_t ticks) {
    last_timeout=ticks; ++parks;
    if(action==1)native_push_event_slot(0,'Z');
    if(action==2)native_owner_exit(123);
    return true;
}
bool scheduler_sleep_current_locked(uint64_t ticks) { return simulated_sleep(ticks); }
bool scheduler_park_current_locked(void) { return simulated_sleep(0); }
int main(void) {
    native_reset();
    assert(native_open(123,"Events",6));
    assert(!native_wait_event(123,UINT64_MAX));
    native_push_event_slot(0,'A');
    assert(native_wait_event(123,0)=='A' && parks==0);
    action=1;
    assert(native_wait_event(123,0)=='Z');
    assert(parks==1 && last_timeout==0 && wakes==1 && wake_id==9);
    assert(native_snapshot().waiter==0);
    action=0;
    assert(native_wait_event(123,5)==0);
    assert(parks==2 && last_timeout==5 && native_snapshot().waiter==0);
    action=2;
    assert(native_wait_event(123,0)==NATIVE_EVENT_CLOSE);
    assert(!native_snapshot().active && wakes==2);
    assert(native_open(124,"Reused",6));
    assert(!native_next_event(124));
    assert(native_wait_event(123,0)==NATIVE_EVENT_CLOSE);
    assert(native_slot_for_owner(124)==0);
    puts("PASS: pending events, atomic wake, timed wait, exit and slot isolation");
    return 0;
}
