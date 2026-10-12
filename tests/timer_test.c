#include "arch/x86_64/timer.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

int main(void) {
    assert(timer_divisor_for_hz(0) == 0);
    assert(timer_divisor_for_hz(1) == 0); /* divisor too large */
    assert(timer_divisor_for_hz(18) == 0);
    assert(timer_divisor_for_hz(19) > 0);
    assert(timer_divisor_for_hz(100) == 11932);
    assert(timer_actual_hz(timer_divisor_for_hz(100)) == 100);
    assert(timer_divisor_for_hz(1000) == 1193);
    assert(timer_actual_hz(timer_divisor_for_hz(1000)) == 1000);
    assert(timer_divisor_for_hz(PIT_INPUT_HZ) == 1);
    assert(timer_divisor_for_hz(UINT32_MAX) == 0);
    assert(timer_actual_hz(0) == 0);
    uint16_t d = timer_divisor_for_hz(100);
    assert(timer_ticks_to_milliseconds(0, d) == 0);
    assert(timer_ticks_to_milliseconds(0, 0) == 0);
    assert(timer_ticks_to_milliseconds(100, d) >= 999);
    assert(timer_ticks_to_milliseconds(100, d) <= 1001);
    assert(timer_ticks_to_milliseconds(1000, d) >= 9999);
    assert(timer_ticks_to_milliseconds(UINT64_MAX, UINT16_MAX) == UINT64_MAX);
    assert(timer_ticks_to_milliseconds(UINT64_MAX, 1) > 0);
    puts("PASS: PIT divisor bounds, tick-to-ms monotonic conversion and saturation");
}
