#ifndef OPENXP_ARCH_RTC_H
#define OPENXP_ARCH_RTC_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { unsigned year,month,day; } rtc_date_t;
/* RTC is used only to choose the initial calendar page. It does not establish
   timezone, accurate local time, or a trusted wall clock. */
bool rtc_read_date(rtc_date_t *date);
#endif
