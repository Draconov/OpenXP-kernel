#ifndef OPENXP_DIAG_CRASH_H
#define OPENXP_DIAG_CRASH_H
#include <stdint.h>
struct exception_frame;
/* Always prints to COM1 without allocation, blocking, or touching the heap.
 * exception_dispatch handles the actual halt and self-test markers. */
void crash_report_exception(const struct exception_frame *frame, uint64_t cr2);
#endif
