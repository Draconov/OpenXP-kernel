#include <stdint.h>
#include <stddef.h>
#include "limine_min.h"
#include "serial.h"
#include "arch/x86_64/interrupts.h"
#include "arch/x86_64/gdt.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "sched/scheduler.h"
#include "sync/wait_queue.h"
#include "sync/event.h"
#include "sync/mutex.h"
#include "sync/ipc.h"
#include "arch/x86_64/timer.h"
#include "arch/x86_64/irq_state.h"
#include "user/userspace.h"
#include "fs/initrd.h"
#include "video/fb.h"
#include "gui/desktop.h"
#include "input/input.h"
#include "user/process.h"

__attribute__((used, section(".limine_requests_start"))) static volatile uint64_t requests_start[] = LIMINE_REQUESTS_START_MARKER;
__attribute__((used, section(".limine_requests"))) static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(3);
__attribute__((used, section(".limine_requests"))) static volatile struct limine_framebuffer_request framebuffer_request = { .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0 };
__attribute__((used, section(".limine_requests"))) static volatile struct limine_memmap_request memmap_request = { .id = LIMINE_MEMMAP_REQUEST, .revision = 0 };
__attribute__((used, section(".limine_requests"))) static volatile struct limine_hhdm_request hhdm_request = { .id = LIMINE_HHDM_REQUEST, .revision = 0 };
__attribute__((used, section(".limine_requests_end"))) static volatile uint64_t requests_end[] = LIMINE_REQUESTS_END_MARKER;

extern const uint8_t openxp_initrd_start[], openxp_initrd_end[];
static fb_surface_t desktop_fb;
static desktop_t desktop;
static input_device_t desktop_input;
static uint64_t shell_ticks(void *ctx) {(void)ctx;return timer_ticks();}
static uint64_t shell_free(void *ctx) {(void)ctx;return pmm_free_pages();}
static size_t shell_count(void *ctx) {(void)ctx;return initrd_count();}
static bool shell_name(void *ctx,size_t index,char *buf,size_t cap) {
    (void)ctx;return initrd_name_at(index,buf,cap);
}
/* A cautious one-at-a-time app launcher; stdout is serial, not a GUI pipe.
 * Requires the existing process manager initialized during boot self-tests. */
static size_t shell_read_text(void *ctx,const char *path,char *dst,size_t capacity) {
    (void)ctx;
    const void *file=NULL;size_t n=0;
    if(!initrd_lookup(path,&file,&n)||n>=capacity||!n)return 0;
    const uint8_t *p=(const uint8_t *)file;
    for(size_t i=0;i<n;++i)
        if(p[i]!=10 && p[i]!=13 && (p[i]<32||p[i]>126))return 0;
    for(size_t i=0;i<n;++i)dst[i]=(char)p[i];
    dst[n]=0;return n;
}
static bool shell_launch(void *ctx,const char *path) {
    (void)ctx;
    const void *image=NULL;size_t bytes=0;
    if(!initrd_lookup(path,&image,&bytes))return false;
    uint64_t flags=irq_save_disable();
    uint64_t pid=process_create_elf(image,bytes);
    bool passed=false;
    if(pid) {
        process_info_t result={0};
        passed=process_run(pid,&result)&&result.state==PROCESS_EXITED&&result.status==0;
        if(!process_reap(pid))passed=false;
    }
    irq_restore(flags);
    return passed;
}
static void desktop_interactive_loop(void) __attribute__((noreturn));
static void desktop_interactive_loop(void) {
    shell_services_t services={shell_ticks,shell_free,shell_count,shell_name,shell_launch,shell_read_text,NULL};
    desktop_init(&desktop,&desktop_fb,services);
    input_state_init(&desktop_input,desktop_fb.width,desktop_fb.height);
    input_hardware_init();
    desktop_draw(&desktop);
    serial_puts(input_irq_ready()?"OpenXP input: PS/2 IRQ1/IRQ12 buffered\n":
                                   "OpenXP input: legacy PS/2 polling fallback\n");
    serial_puts("OpenXP desktop: keyboard, mouse + COM1 terminal enabled\n");
    serial_puts("OPENXP_INPUT_OK\nOPENXP_GUI_OK\nOPENXP_SHELL_OK\nOPENXP_WM_OK\n");
    timer_enable();
    uint64_t last_second=timer_ticks()/100u;
    for (;;) {
        bool redraw=false;
        input_event_t event;
        /* Consume whole PS/2 packets rather than stopping after the first byte.
           Pure cursor movements restore only the cursor's 9x13 underlay; a
           whole-screen repaint on each movement was prohibitively expensive. */
        for(unsigned i=0;i<128 && input_poll(&desktop_input,&event);++i) {
            if(event.kind==INPUT_NONE)continue;
            desktop_tick(&desktop,timer_ticks());
            if(event.kind==INPUT_KEY) {
                int before=desktop_top_window(&desktop);
                bool had_menu=desktop.start_open;
                desktop_key_event(&desktop,&event);
                if(!redraw && !had_menu && !desktop.start_open &&
                   before>=0 && before==desktop_top_window(&desktop))
                    desktop_redraw_active(&desktop);
                else redraw=true;
            } else {
                bool was_dragging=desktop.dragging;
                int which=desktop.drag_window;
                int oldx=0,oldy=0;
                if(was_dragging && which>=0 && which<DESKTOP_WINDOWS) {
                    oldx=desktop.windows[which].x;
                    oldy=desktop.windows[which].y;
                }
                desktop_mouse(&desktop,event.x,event.y,event.left);
                desktop_right_mouse(&desktop,event.x,event.y,event.right);
                if(desktop.paint_dirty){
                    if(!redraw && event.kind==INPUT_MOVE)
                        desktop_redraw_paint(&desktop);
                    desktop.paint_dirty=false;
                }
                if(was_dragging && which>=0 && which<DESKTOP_WINDOWS &&
                   (oldx!=desktop.windows[which].x || oldy!=desktop.windows[which].y)) {
                    if(!redraw)desktop_redraw_moved_window(&desktop,oldx,oldy,which);
                } else if(event.kind==INPUT_BUTTON || (!was_dragging && desktop.dragging))
                    redraw=true;
                else if(!redraw)desktop_cursor_update(&desktop);
            }
        }
        char c;
        for(unsigned i=0;i<64 && serial_try_getc(&c);++i) {
            if(c=='\r')c='\n';
            int before=desktop_top_window(&desktop);
            desktop_key(&desktop,c);
            if(!redraw && !desktop.start_open &&
               before==DESKTOP_TERMINAL &&
               desktop_top_window(&desktop)==DESKTOP_TERMINAL)
                desktop_redraw_terminal(&desktop);
            else redraw=true;
        }
        uint64_t now=timer_ticks();
        desktop_tick(&desktop,now);
        if(now/100u!=last_second) {
            last_second=now/100u;
            if(!redraw)desktop_redraw_taskbar(&desktop);
            if(!redraw && desktop_top_window(&desktop)==DESKTOP_MONITOR)
                desktop_redraw_active(&desktop);
        }
        if(redraw)desktop_draw(&desktop);
        /* IRQ1/IRQ12 enqueue PS/2 bytes and wake HLT immediately; the
           foreground loop does all decoding, GUI and shell work. */
        __asm__ volatile ("hlt" ::: "memory");
    }
}
/* Map a small kernel-owned virtual heap between permanently unmapped guards.
 * Its allocated backing frames remain owned for the kernel's lifetime. */
#define INITIAL_HEAP_PAGES 32u
static bool bootstrap_heap(void) {
    const uint64_t virtual_start = vmm_region_base() + VMM_PAGE_SIZE;
    uint64_t physical_pages[INITIAL_HEAP_PAGES] = {0};
    unsigned mapped = 0;
    for (; mapped < INITIAL_HEAP_PAGES; ++mapped) {
        physical_pages[mapped] = pmm_alloc_zeroed_page();
        if (!physical_pages[mapped] ||
            !vmm_map_page(virtual_start + (uint64_t)mapped * VMM_PAGE_SIZE,
                          physical_pages[mapped], VMM_PAGE_WRITE)) break;
    }
    if (mapped != INITIAL_HEAP_PAGES) {
        for (unsigned i = 0; i < mapped; ++i) {
            (void)vmm_unmap_page(virtual_start + (uint64_t)i * VMM_PAGE_SIZE);
            (void)pmm_free_page(physical_pages[i]);
        }
        if (mapped < INITIAL_HEAP_PAGES && physical_pages[mapped])
            (void)pmm_free_page(physical_pages[mapped]);
        return false;
    }
    if (!heap_init((void *)(uintptr_t)virtual_start,
                   (size_t)INITIAL_HEAP_PAGES * VMM_PAGE_SIZE)) return false;
    uint64_t backing = 0;
    unsigned flags = 0;
    if (vmm_query_page(heap_guard_low(), NULL, NULL) ||
        vmm_query_page(heap_guard_high(), NULL, NULL) ||
        !vmm_query_page(virtual_start, &backing, &flags) ||
        backing != physical_pages[0] || flags != VMM_PAGE_WRITE) return false;

    uint64_t *words = (uint64_t *)kcalloc(16, sizeof(uint64_t));
    uint8_t *aligned = (uint8_t *)kmalloc_aligned(63, 64);
    if (!words || !aligned || ((uintptr_t)aligned & 63u) ||
        kmalloc(SIZE_MAX) != NULL) return false;
    for (unsigned i = 0; i < 16; ++i) {
        if (words[i]) return false;
        words[i] = 0x4f50454e58500000ULL | i;
    }
    for (unsigned i = 0; i < 16; ++i)
        if (words[i] != (0x4f50454e58500000ULL | i)) return false;
    for (unsigned i = 0; i < 63; ++i) aligned[i] = (uint8_t)i;
    if (!kfree(aligned) || kfree(aligned)) return false;
    uint8_t *reused = (uint8_t *)kmalloc_aligned(48, 64);
    if (!reused || (uintptr_t)reused != (uintptr_t)aligned || !kfree(reused) ||
        !kfree(words)) return false;
    serial_puts("Deep-cloned page tables, NX heap, and guard pages established\nOPENXP_VMM_OK\n");
    serial_puts("Kernel heap initialized (128 KiB)\nOPENXP_HEAP_OK\n");
    return true;
}


#ifdef OPENXP_DEEP_SELFTEST
/* Deterministic integration exercise. Each task executes on its own guarded
 * stack and yields via the real x86-64 assembly context switch. It is not
 * timer-preemptive and does not enable hardware interrupts. */
static volatile unsigned demo_index;
static volatile unsigned demo_order[6];
static void demo_thread(void *arg) {
    unsigned identity = (unsigned)(uintptr_t)arg;
    for (unsigned i = 0; i < 3; ++i) {
        unsigned at = demo_index++;
        if (at < 6) demo_order[at] = identity;
        scheduler_yield();
    }
}
static bool bootstrap_scheduler(void) {
    uint64_t before = pmm_free_pages();
    if (!scheduler_init()) return false;
    uint64_t a = scheduler_create(demo_thread, (void *)(uintptr_t)1);
    uint64_t b = scheduler_create(demo_thread, (void *)(uintptr_t)2);
    if (!a || !b || a == b) return false;
    for (unsigned i = 0; i < 16 && scheduler_live_workers(); ++i)
        scheduler_yield();
    if (scheduler_live_workers() || demo_index != 6 ||
        scheduler_switch_count() < 6) return false;
    for (unsigned i = 0; i < 6; ++i)
        if (demo_order[i] != 1 + (i & 1u)) return false;
    if (scheduler_reap() != 2 || pmm_free_pages() != before) return false;
    serial_puts("Cooperative threads: context switches, ordering, guard stacks, reclaim OK\n");
    serial_puts("OPENXP_SCHED_OK\n");
    return true;
}

/* Timer-preempted workers share the kernel address space. Keep them in
 * bounded work loops, then reclaim all stacks with IRQs masked to exercise
 * the thread lifecycle instead of leaking two permanent worker slots. */
static volatile uint64_t preempt_progress[3];
static volatile bool preempt_stop;
static void preempt_worker(void *arg) {
    unsigned worker = (unsigned)(uintptr_t)arg;
    while (!preempt_stop) {
        ++preempt_progress[worker];
        __asm__ volatile ("pause" ::: "memory");
    }
}
static bool bootstrap_timer(void) {
    uint64_t before = pmm_free_pages();
    if (!scheduler_enable_preemption() || !timer_init(TIMER_TARGET_HZ)) return false;
    preempt_stop = false;
    uint64_t a = scheduler_create(preempt_worker, (void *)(uintptr_t)0);
    uint64_t b = scheduler_create(preempt_worker, (void *)(uintptr_t)1);
    if (!a || !b || a == b) return false;
    serial_puts("PIT 100 Hz / PIC IRQ0: beginning thread-lifecycle test\n");
    timer_enable();
    /* Two busy loops cannot progress without involuntary timer switching. */
    while (timer_ticks() < 25) __asm__ volatile ("pause" ::: "memory");
    /* Critical section protects the scheduler + PMM/VMM stack allocator
     * from interrupt-driven switching while a new worker is published. */
    uint64_t flags = irq_save_disable();
    uint64_t c = scheduler_create(preempt_worker, (void *)(uintptr_t)2);
    irq_restore(flags);
    if (!c) { timer_disable(); return false; }
    while (timer_ticks() < 70) __asm__ volatile ("pause" ::: "memory");
    preempt_stop = true;
    /* If timer dispatch hangs, scripts/smoke.sh's QEMU watchdog catches it.
     * Workers terminate on their own stack, then the IRQ scheduler returns
     * to the boot thread; only the boot thread is allowed to reap. */
    while (scheduler_live_workers() && timer_ticks() < 130)
        __asm__ volatile ("pause" ::: "memory");
    timer_disable();
    if (scheduler_live_workers() || !preempt_progress[0] || !preempt_progress[1] ||
        !preempt_progress[2] || scheduler_preempt_switch_count() < 10)
        return false;
    if (scheduler_reap() != 3 || pmm_free_pages() != before) return false;
    /* Reused slot should have a new identifier even in preemptive mode. */
    uint64_t newer = scheduler_create(preempt_worker, (void *)(uintptr_t)0);
    if (!newer || newer <= c) return false;
    timer_enable();
    while (scheduler_live_workers() && timer_ticks() < 160)
        __asm__ volatile ("pause" ::: "memory");
    timer_disable();
    if (scheduler_live_workers() || scheduler_reap() != 1 ||
        pmm_free_pages() != before) return false;
    serial_puts("IRQ0 ticks: "); serial_u64(timer_ticks());
    serial_puts(" | Preemptive switches: ");
    serial_u64(scheduler_preempt_switch_count());
    serial_puts("\nReaped 4 timer workers; all stack frames reclaimed\n");
    serial_puts("OPENXP_REAP_OK\nOPENXP_IRQ_OK\nOPENXP_TIMER_OK\n");
#ifdef OPENXP_TIMER_SELFTEST
    serial_puts("OPENXP_TIMER_IRQ_TEST_OK\n");
#endif
    return true;
}

/* v0.8 boot exercise: a pair of real sleeping threads and a manually
 * blocked worker. The boot thread sleeps too, proving that idle progress is
 * driven by the PIT and not a busy loop in every worker. */
static volatile uint64_t sleep_wake_tick[2];
static volatile uint64_t sleep_start_tick[2];
static volatile unsigned sleep_failures;
static volatile bool blocked_resumed;
static void sleep_demo_worker(void *arg) {
    unsigned worker = (unsigned)(uintptr_t)arg;
    sleep_start_tick[worker] = timer_ticks();
    if (!scheduler_sleep_ticks(5u + worker * 5u)) ++sleep_failures;
    sleep_wake_tick[worker] = timer_ticks();
}
static void block_demo_worker(void *arg) {
    (void)arg;
    if (!scheduler_block_current()) ++sleep_failures;
    blocked_resumed = true;
}
static bool bootstrap_sleep(void) {
    const uint64_t before = pmm_free_pages();
    sleep_failures = 0;
    blocked_resumed = false;
    for (unsigned i = 0; i < 2; ++i) {
        sleep_wake_tick[i] = 0;
        sleep_start_tick[i] = 0;
    }
    uint64_t a = scheduler_create(sleep_demo_worker, (void *)(uintptr_t)0);
    uint64_t b = scheduler_create(sleep_demo_worker, (void *)(uintptr_t)1);
    uint64_t c = scheduler_create(block_demo_worker, NULL);
    if (!a || !b || !c) return false;
    const uint64_t start = timer_ticks();
    timer_enable();
    bool boot_slept = scheduler_sleep_ticks(20);
    const uint64_t elapsed_ticks = timer_ticks() - start;
    /* Wake the blocked worker with a short IF=0 critical section. */
    uint64_t flags = irq_save_disable();
    bool awakened = scheduler_wake(c);
    irq_restore(flags);
    /* A bounded watchdog avoids spinning indefinitely if a worker never
     * exits. The external QEMU smoke timeout catches missing PIT IRQs. */
    while (scheduler_live_workers() && timer_ticks() - start < 100)
        __asm__ volatile ("pause" ::: "memory");
    timer_disable();
    if (!boot_slept || !awakened || elapsed_ticks < 20 || sleep_failures ||
        !blocked_resumed || !sleep_wake_tick[0] || !sleep_wake_tick[1] ||
        sleep_wake_tick[0] - sleep_start_tick[0] < 5 ||
        sleep_wake_tick[1] - sleep_start_tick[1] < 10 ||
        scheduler_live_workers()) return false;
    if (scheduler_reap() != 3 || pmm_free_pages() != before) return false;
    serial_puts("PIT uptime milliseconds: "); serial_u64(timer_monotonic_ms());
    serial_puts("\nOPENXP_CLOCK_OK\nOPENXP_SLEEP_OK\nOPENXP_BLOCK_OK\n");
    return true;
}

/* v0.9: deterministic live IPC/self-test, only on experimental UP PIT/PIC.
 * The queue is deliberately smaller than the message burst to exercise
 * backpressure, blocking, and wake-one in each direction. */
static ipc_queue_t demo_messages;
static event_t demo_start, demo_done;
static mutex_t demo_mutex;
static volatile unsigned demo_ipc_error;
static unsigned demo_ipc_order;
static unsigned demo_ipc_updates;
#define DEMO_MESSAGE_COUNT 18u
static void ipc_producer(void *arg) {
    (void)arg;
    if (!event_wait(&demo_start)) { ++demo_ipc_error; return; }
    for (unsigned i = 1; i <= DEMO_MESSAGE_COUNT; ++i) {
        if (ipc_send(&demo_messages, &i, sizeof(i)) != IPC_OK) {
            ++demo_ipc_error; return;
        }
        if (!mutex_lock(&demo_mutex)) { ++demo_ipc_error; return; }
        ++demo_ipc_updates;
        if (!mutex_unlock(&demo_mutex)) { ++demo_ipc_error; return; }
    }
}
static void ipc_consumer(void *arg) {
    (void)arg;
    if (!event_wait(&demo_start)) { ++demo_ipc_error; event_set(&demo_done); return; }
    for (unsigned i = 1; i <= DEMO_MESSAGE_COUNT; ++i) {
        unsigned message = 0;
        size_t received = 0;
        if (ipc_receive(&demo_messages, &message, sizeof(message), &received) != IPC_OK ||
            received != sizeof(message) || message != i) {
            ++demo_ipc_error;
            break;
        }
        if (!mutex_lock(&demo_mutex)) { ++demo_ipc_error; break; }
        demo_ipc_order = message;
        ++demo_ipc_updates;
        if (!mutex_unlock(&demo_mutex)) { ++demo_ipc_error; break; }
    }
    event_set(&demo_done);
}
static bool bootstrap_sync(void) {
    uint64_t before = pmm_free_pages();
    demo_ipc_error = demo_ipc_updates = demo_ipc_order = 0;
    ipc_init(&demo_messages);
    event_init(&demo_start, false);
    event_init(&demo_done, false);
    mutex_init(&demo_mutex);
    uint64_t p = scheduler_create(ipc_producer, NULL);
    uint64_t c = scheduler_create(ipc_consumer, NULL);
    if (!p || !c) return false;
    timer_enable();
    /* Event is set only after both threads have been published. */
    event_set(&demo_start);
    /* The boot thread blocks without spinning, resuming at completion. */
    bool completed = event_wait(&demo_done);
    const uint64_t start = timer_ticks();
    while (scheduler_live_workers() && timer_ticks() - start < 70)
        __asm__ volatile ("pause" ::: "memory");
    timer_disable();
    if (!completed || scheduler_live_workers() || demo_ipc_error ||
        demo_ipc_order != DEMO_MESSAGE_COUNT ||
        demo_ipc_updates != 2 * DEMO_MESSAGE_COUNT || demo_messages.count != 0 ||
        scheduler_reap() != 2 || pmm_free_pages() != before) return false;
    ipc_close(&demo_messages);
    serial_puts("Wait queues, event notification, mutex and FIFO IPC OK\n");
    serial_puts("OPENXP_WAITQ_OK\nOPENXP_EVENT_OK\n");
    serial_puts("OPENXP_MUTEX_OK\nOPENXP_IPC_OK\n");
#ifdef OPENXP_SYNC_SELFTEST
    serial_puts("OPENXP_SYNC_TEST_OK\n");
#endif
    return true;
}

#endif /* OPENXP_DEEP_SELFTEST */

void _start(void) {
    serial_init();serial_puts("OpenXP kernel 1.8.0: entered _start\n");
    if(!LIMINE_BASE_REVISION_SUPPORTED(base_revision)){serial_puts("ERROR: Limine base revision unsupported\n");kernel_halt();}
    gdt_init();
    interrupts_init();
    const struct limine_memmap_response *m=memmap_request.response;
    const struct limine_hhdm_response *h=hhdm_request.response;
    if (!m || !h) { serial_puts("ERROR: memory map or HHDM missing\n"); kernel_halt(); }
    if (!pmm_init(m, h->offset)) { serial_puts("ERROR: PMM initialization failed\n"); kernel_halt(); }
    const uint64_t initial_free = pmm_free_pages();
    uint64_t page_a = pmm_alloc_zeroed_page();
    uint64_t page_b = pmm_alloc_zeroed_page();
    if (!page_a || !page_b || page_a == page_b ||
        (page_a & (PMM_PAGE_SIZE - 1)) || (page_b & (PMM_PAGE_SIZE - 1)) ||
        !pmm_direct_map(page_a) || !pmm_direct_map(page_b)) {
        serial_puts("ERROR: PMM allocation test failed\n"); kernel_halt();
    }
    volatile uint64_t *test_word = (volatile uint64_t *)pmm_direct_map(page_a);
    if (*test_word != 0) { serial_puts("ERROR: PMM zero-fill failed\n"); kernel_halt(); }
    *test_word = 0x4f50454e5850ULL;
    if (*test_word != 0x4f50454e5850ULL || !pmm_free_page(page_a) ||
        pmm_free_page(page_a) || !pmm_free_page(page_b) ||
        pmm_free_pages() != initial_free) {
        serial_puts("ERROR: PMM free/integrity test failed\n"); kernel_halt();
    }
    serial_puts("Managed physical pages: "); serial_u64(pmm_total_pages());
    serial_puts(" | Free: "); serial_u64(pmm_free_pages());
    serial_puts("\nOPENXP_PMM_OK\n");
    if (!vmm_init()) { serial_puts("ERROR: VMM initialization failed\n"); kernel_halt(); }
    if (!bootstrap_heap()) { serial_puts("ERROR: kernel heap initialization failed\n"); kernel_halt(); }
    if (!gdt_install_guarded_stacks()) {
        serial_puts("ERROR: TSS guarded stack installation failed\n"); kernel_halt();
    }
#ifdef OPENXP_DEEP_SELFTEST
    if (!bootstrap_scheduler()) {
        serial_puts("ERROR: cooperative thread self-test failed\n"); kernel_halt();
    }
    if (!bootstrap_timer()) {
        serial_puts("ERROR: timer preemption self-test failed\n"); kernel_halt();
    }
    if (!bootstrap_sleep()) {
        serial_puts("ERROR: sleep/clock/block self-test failed\n"); kernel_halt();
    }
    if (!bootstrap_sync()) {
        serial_puts("ERROR: synchronization/IPC self-test failed\n"); kernel_halt();
    }
    if (!vmm_user_init()) { serial_puts("ERROR: userspace address region unavailable\n"); kernel_halt(); }
#if defined(OPENXP_USER_PROTECTION_SELFTEST)
    if (!userspace_demo_run(2)) { serial_puts("ERROR: ring-3 kernel-memory protection failed\n"); kernel_halt(); }
#elif defined(OPENXP_USER_FAULT_SELFTEST)
    if (!userspace_demo_run(1)) { serial_puts("ERROR: user fault isolation failed\n"); kernel_halt(); }
#else
    if (!userspace_demo_run(0)) { serial_puts("ERROR: ring-3 syscall demo failed\n"); kernel_halt(); }
#endif
#else
    /* Normal boot prioritizes bringing up an interactive desktop. Keep the
       risky ring-3/interrupt stress tests available as explicit diagnostics. */
    if (!scheduler_init() || !scheduler_enable_preemption() ||
        !timer_init(TIMER_TARGET_HZ) || !vmm_user_init() || !process_init()) {
        serial_puts("ERROR: essential desktop subsystem initialization failed\n");kernel_halt();
    }
#endif
    if (!initrd_mount(openxp_initrd_start, (size_t)(openxp_initrd_end - openxp_initrd_start)) ||
        initrd_count() != 3) {
        serial_puts("ERROR: initrd validation failed\n"); kernel_halt();
    }
#ifdef OPENXP_DEEP_SELFTEST
    if (!userspace_initrd_demo_run()) {
        serial_puts("ERROR: initrd ELF or fault isolation test failed\n"); kernel_halt();
    }
    if (!userspace_preempt_demo_run()) {
        serial_puts("ERROR: preemptive Ring 3 integration self-test failed\n");
        kernel_halt();
    }
    serial_puts("OPENXP_DEEP_OK\n");
#endif
    uint64_t usable=0;
    if(m){for(uint64_t i=0;i<m->entry_count;i++)if(m->entries[i]->type==LIMINE_MEMMAP_USABLE)usable+=m->entries[i]->length;}
    serial_puts("Memory map entries: ");serial_u64(m?m->entry_count:0);serial_puts("\nUsable RAM MiB: ");serial_u64(usable/1048576);serial_puts("\n");
    const struct limine_framebuffer_response *r=framebuffer_request.response;
    if(!r||!r->framebuffer_count){serial_puts("WARNING: no framebuffer\n");kernel_halt();}
    struct limine_framebuffer *f=r->framebuffers[0];
    if (!f->address || f->bpp != 32 || f->memory_model != 1 ||
        f->width > UINT64_MAX / 4 || f->pitch < f->width * 4 ||
        f->red_mask_size == 0 || f->red_mask_size > 8 ||
        f->green_mask_size == 0 || f->green_mask_size > 8 ||
        f->blue_mask_size == 0 || f->blue_mask_size > 8 ||
        f->red_mask_shift > 24 || f->green_mask_shift > 24 || f->blue_mask_shift > 24) {
        serial_puts("ERROR: unsupported RGB framebuffer layout\n"); kernel_halt();
    }
    if(!fb_init(&desktop_fb,f)) {
        serial_puts("ERROR: graphical surface initialization failed\n");kernel_halt();
    }
    serial_puts("Framebuffer ready: ");serial_u64(f->width);serial_putc('x');
    serial_u64(f->height);serial_puts("\nOPENXP_BOOT_OK\n");
#ifdef OPENXP_EXCEPTION_SELFTEST
    serial_puts("Triggering intentional invalid-opcode exception (UD2)\n");
    __asm__ volatile("ud2");
#endif
#ifdef OPENXP_GUARD_SELFTEST
    serial_puts("Triggering intentional heap-guard page fault\n");
    volatile uint64_t guard_value = *(volatile uint64_t *)(uintptr_t)heap_guard_low();
    (void)guard_value;
#endif
    desktop_interactive_loop();
}
