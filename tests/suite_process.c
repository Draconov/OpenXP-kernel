/* OpenXP host test suite: process.
 * Select exactly one original test per compilation using -DOPENXP_TEST_NAME.
 * Individual test binaries and sanitizers remain unchanged. */
#if defined(OPENXP_TEST_PROCESS)
#include "mm/address_space.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "user/elf.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define FRAMES 2048
static unsigned char backing[FRAMES][4096] __attribute__((aligned(4096)));
static bool in_use[FRAMES];
static unsigned allocated;
uint64_t pmm_alloc_zeroed_page(void) {
    for (unsigned i=1;i<FRAMES;++i) if (!in_use[i]) {
        in_use[i]=true; ++allocated;
        memset(backing[i], 0, 4096);
        return (uint64_t)i*4096;
    }
    return 0;
}
void *pmm_direct_map(uint64_t pa) {
    if (!pa || pa % 4096 || pa/4096 >= FRAMES || !in_use[pa/4096]) return NULL;
    return backing[pa/4096];
}
bool pmm_free_page(uint64_t pa) {
    if (!pa || pa%4096 || pa/4096>=FRAMES || !in_use[pa/4096]) return false;
    in_use[pa/4096] = false;
    --allocated;
    return true;
}
static void p16(uint8_t *b, unsigned at, uint16_t x) {
    memcpy(b+at,&x,2);
}
static void p32(uint8_t *b, unsigned at, uint32_t x) {
    memcpy(b+at,&x,4);
}
static void p64(uint8_t *b, unsigned at, uint64_t x) {
    memcpy(b+at,&x,8);
}
static size_t make_elf(uint8_t *elf, uint64_t va) {
    memset(elf,0,512);
    memcpy(elf,"\177ELF",4); elf[4]=2;elf[5]=1;elf[6]=1;
    p16(elf,16,2);p16(elf,18,62);p32(elf,20,1);
    p64(elf,24,va);p64(elf,32,64);
    p16(elf,52,64);p16(elf,54,56);p16(elf,56,1);
    p32(elf,64,1);p32(elf,68,5);
    p64(elf,72,128);p64(elf,80,va);
    p64(elf,96,4);p64(elf,104,8192);
    p64(elf,112,1);
    elf[128]=0x48;elf[129]=0x31;elf[130]=0xc0;elf[131]=0xc3;
    return 132;
}
static void failed_elf(const uint8_t *image, size_t n) {
    unsigned before=allocated;
    address_space_t s={0};
    assert(aspace_create(&s));
    uint64_t entry=0;
    assert(!elf64_load(&s,image,n,&entry));
    assert(aspace_destroy(&s));
    assert(allocated==before);
}
int main(void) {
    uint64_t root=pmm_alloc_zeroed_page();
    /* Mock kernel higher-half mapping: must survive process destruction,
       and be supervisor-only even if the cloned entry started with U=1. */
    uint64_t *t=pmm_direct_map(root);
    uint64_t shared=pmm_alloc_zeroed_page();
    t[300] = shared|7;
    assert(vmm_test_attach(root,301));
    assert(vmm_user_init());
    assert(vmm_user_base() == 1ULL<<39);
    assert(aspace_restore(root));
    unsigned original=allocated;
    address_space_t a={0},b={0};
    assert(aspace_create(&a) && aspace_create(&b));
    assert(!( ((uint64_t *)pmm_direct_map(a.cr3))[300] & 4));
    assert(((uint64_t *)pmm_direct_map(a.cr3))[300] & 1);
    assert(((uint64_t *)pmm_direct_map(a.cr3))[1] == 0);
    uint64_t va=vmm_user_base()+0x100000;
    uint64_t pa=pmm_alloc_zeroed_page(), pb=pmm_alloc_zeroed_page();
    assert(aspace_map(&a,va,pa,VMM_PAGE_WRITE));
    assert(aspace_map(&b,va,pb,VMM_PAGE_WRITE));
    uint64_t chk;
    assert(aspace_query(&a,va,&chk,NULL) && chk == pa);
    assert(aspace_query(&b,va,&chk,NULL) && chk == pb);
    const char data[]="OpenXP user memory";
    char dst[sizeof(data)];
    assert(aspace_copy_to_user(&a,va+200,data,sizeof(data)));
    assert(aspace_copy_from_user(&a,dst,va+200,sizeof(dst)) && !memcmp(data,dst,sizeof(dst)));
    assert(aspace_copy_from_user(&b,dst,va+200,sizeof(dst)) && memcmp(data,dst,sizeof(dst)));
    uint64_t next=pmm_alloc_zeroed_page();
    assert(aspace_map(&a,va+4096,next,VMM_PAGE_WRITE));
    uint8_t cross[130],readback[130];
    memset(cross,0x5a,sizeof(cross));
    assert(aspace_copy_to_user(&a,va+4090,cross,sizeof(cross)));
    assert(aspace_copy_from_user(&a,readback,va+4090,sizeof(readback)) &&
           !memcmp(cross,readback,sizeof(cross)));
    assert(!aspace_copy_from_user(&a,readback,va+8192,sizeof(readback)));
    assert(!aspace_copy_from_user(&a,readback,UINT64_MAX-8,20));
    assert(!aspace_copy_to_user(&a,va+4090,cross,9000));
    assert(!aspace_map(&a,va,pa,0));
    assert(!aspace_map(&a,va+8192,pa,VMM_PAGE_WRITE|VMM_PAGE_EXEC));
    assert(!aspace_map(&a,va+8192,0,0));
    assert(!aspace_map(&a,va+8192,pa|1,0));
    assert(aspace_unmap(&a,va+4096));
    assert(!aspace_query(&a,va+4096,NULL,NULL));
    uint64_t previous=aspace_activate(&a);
    assert(previous==root && aspace_current_root()==a.cr3);
    assert(!aspace_destroy(&a));
    assert(aspace_restore(previous));
    assert(aspace_unmap(&a,va));
    assert(!aspace_unmap(&a,va));
    assert(aspace_destroy(&a) && aspace_destroy(&b));
    assert(allocated==original);

    uint8_t elf[512];size_t n=make_elf(elf,va);
    address_space_t c={0};
    assert(aspace_create(&c));
    uint64_t entry=0;
    assert(elf64_load(&c,elf,n,&entry) && entry==va);
    assert(aspace_range_valid(&c,va,8192,false));
    assert(!aspace_range_valid(&c,va,1,true));
    assert(!aspace_copy_to_user(&c,va,elf,1));
    uint8_t out[4];
    assert(aspace_copy_from_user(&c,out,va,4) && !memcmp(out,elf+128,4));
    assert(aspace_copy_from_user(&c,out,va+4096,4));
    assert(out[0]==0 && out[1]==0 && out[2]==0 && out[3]==0);
    assert(aspace_destroy(&c));
    assert(allocated==original);
    /* Relocation-free ET_DYN images use a relative entry and program VA;
     * the loader rebases them into the chosen private user PML4 slot. */
    p16(elf,16,3);
    p64(elf,24,0x100000); p64(elf,80,0x100000);
    address_space_t pie={0};
    assert(aspace_create(&pie));
    assert(elf64_load(&pie,elf,n,&entry) && entry==va);
    assert(aspace_copy_from_user(&pie,out,va,4) && !memcmp(out,elf+128,4));
    assert(aspace_destroy(&pie) && allocated == original);
    p64(elf,24,4ULL * 1024 * 1024); failed_elf(elf,n);
    p64(elf,24,0x100000);
    p64(elf,80,UINT64_MAX); failed_elf(elf,n);
    p64(elf,80,0x100000);
    p16(elf,16,2);
    p64(elf,24,va); p64(elf,80,va);
    elf[0]=0; failed_elf(elf,n); elf[0]=0x7f;
    p16(elf,16,99); failed_elf(elf,n); p16(elf,16,2);
    p16(elf,56,17); failed_elf(elf,n); p16(elf,56,1);
    p64(elf,32,UINT64_MAX); failed_elf(elf,n); p64(elf,32,64);
    p32(elf,68,7); failed_elf(elf,n); p32(elf,68,5);
    p64(elf,24,va+16384); failed_elf(elf,n); p64(elf,24,va);
    p64(elf,96,10000); failed_elf(elf,n); p64(elf,96,4);
    p64(elf,80,UINT64_MAX-128); failed_elf(elf,n); p64(elf,80,va);
    p64(elf,104,0); failed_elf(elf,n); p64(elf,104,8192);
    p64(elf,112,4096); failed_elf(elf,n); p64(elf,112,1);
    failed_elf(elf,n-1);
    assert(allocated==original);
    assert(pmm_free_page(shared) && pmm_free_page(root));
    assert(allocated==0);
    puts("PASS: private CR3 roots, mapping permissions, copyin/copyout, cleanup, ELF64 validation");
    return 0;
}
#elif defined(OPENXP_TEST_PROCESS_MANAGER)
#include "user/process.h"
#include "user/elf.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define MOCK_BASE 0xffff900000000000ULL
#define MOCK_USER 0x10000000000ULL
#define SLOT_PAGES 48u

static uint64_t mapped[SLOT_PAGES];
static uint64_t physical_next = 0x1000;
static unsigned frames, fail_after;
static uint64_t root_next = 0x100000;
static uint64_t active_root = 0x8000;
static uint64_t gdt_top = 0xffff8000000f0000ULL;
static unsigned restore_calls, stack_changes;
static struct { uint64_t cr3; unsigned owned_frames; } roots[64];
static int demo_mode;
static int expect_document_arg;
static bool irq_enabled;
static uint64_t mock_ticks;
static unsigned sleep_calls;

bool irq_are_enabled(void) { return irq_enabled; }
uint64_t irq_save_disable(void) {
    uint64_t flags = irq_enabled ? (1ULL << 9) : 0;
    irq_enabled = false;
    return flags;
}
void irq_restore(uint64_t flags) { irq_enabled = (flags & (1ULL << 9)) != 0; }
uint64_t timer_ticks(void) { return mock_ticks; }
bool scheduler_sleep_ticks(uint64_t ticks) {
    assert(irq_enabled && ticks == 1);
    mock_ticks += ticks;
    ++sleep_calls;
    return true;
}
uint64_t vmm_region_base(void) { return MOCK_BASE; }
uint64_t vmm_user_base(void) { return MOCK_USER; }
static size_t page_index(uint64_t va) {
    assert(va >= MOCK_BASE + 3ULL * 1024 * 1024);
    size_t index = (size_t)((va - (MOCK_BASE + 3ULL * 1024 * 1024)) / VMM_PAGE_SIZE);
    assert(index < SLOT_PAGES && (va & (VMM_PAGE_SIZE-1)) == 0);
    return index;
}
bool vmm_query_page(uint64_t va, uint64_t *pa, unsigned *flags) {
    uint64_t p = mapped[page_index(va)];
    if (!p) return false;
    if (pa) *pa = p;
    if (flags) *flags = VMM_PAGE_WRITE;
    return true;
}
bool vmm_map_page(uint64_t va, uint64_t pa, unsigned flags) {
    (void)flags;
    size_t idx = page_index(va);
    if (!pa || mapped[idx]) return false;
    mapped[idx] = pa;
    return true;
}
bool vmm_unmap_page(uint64_t va) {
    size_t idx = page_index(va);
    if (!mapped[idx]) return false;
    mapped[idx] = 0;
    return true;
}
uint64_t pmm_alloc_zeroed_page(void) {
    if (fail_after && frames >= fail_after) return 0;
    uint64_t r = physical_next;
    physical_next += VMM_PAGE_SIZE;
    ++frames;
    return r;
}
bool pmm_free_page(uint64_t pa) {
    assert(pa && frames);
    --frames;
    return true;
}
uint64_t gdt_kernel_stack_top(void) { return gdt_top; }
bool gdt_set_kernel_stack(uint64_t top) {
    if (!top || (top & 15)) return false;
    gdt_top = top;
    ++stack_changes;
    return true;
}
bool aspace_create(address_space_t *s) {
    if (!s || s->cr3) return false;
    s->cr3 = root_next;
    root_next += VMM_PAGE_SIZE;
    ++frames;
    unsigned n = (unsigned)(s->cr3 / VMM_PAGE_SIZE);
    (void)n;
    for (unsigned i=0; i<64; ++i) if (!roots[i].cr3) {
        roots[i].cr3 = s->cr3; roots[i].owned_frames = 1; break;
    }
    return true;
}
bool aspace_destroy(address_space_t *s) {
    if (!s || !s->cr3 || s->cr3 == active_root) return false;
    /* Account for all fully or partially mapped user frames and the root. */
    for (unsigned i=0; i<64; ++i) if (roots[i].cr3 == s->cr3) {
        assert(frames >= roots[i].owned_frames);
        frames -= roots[i].owned_frames;
        roots[i].cr3 = 0; roots[i].owned_frames = 0;
        break;
    }
    s->cr3 = 0;
    return true;
}
bool elf64_load(address_space_t *s, const void *image, size_t len, uint64_t *entry) {
    if (!s->cr3 || !image || !len || *(const char *)image == 'X') return false;
    ++frames; /* mock image page */
    for (unsigned i=0; i<64; ++i) if (roots[i].cr3 == s->cr3) ++roots[i].owned_frames;
    *entry = MOCK_USER + 0x100000;
    return true;
}
bool aspace_query(const address_space_t *s, uint64_t va, uint64_t *pa, unsigned *flags) {
    (void)s; (void)pa; (void)flags;
    /* All requested stack locations are initially empty. */
    assert(va >= MOCK_USER);
    return false;
}
bool aspace_map(address_space_t *s, uint64_t va, uint64_t pa, unsigned flags) {
    (void)va; (void)flags;
    if (!s || !s->cr3 || !pa) return false;
    for (unsigned i=0; i<64; ++i) if (roots[i].cr3 == s->cr3) ++roots[i].owned_frames;
    return true;
}
uint64_t aspace_current_root(void) { return active_root; }
uint64_t aspace_activate(const address_space_t *s) {
    if (!s || !s->cr3) return 0;
    uint64_t prev = active_root;
    active_root = s->cr3;
    return prev;
}
bool aspace_restore(uint64_t prev) {
    ++restore_calls;
    active_root = prev;
    return true;
}
void arch_user_enter(uint64_t ip, uint64_t sp, uint64_t *continuation, unsigned irq) {
    assert(continuation && !irq);
    *continuation = 0x1000;
    assert(process_current_resume_sp() == *continuation);
    assert(ip == MOCK_USER+0x100000);
    assert(sp > MOCK_USER && !(sp & 15));
    assert(process_current_pid() != 0);
    char arg[PROCESS_LAUNCH_ARG_MAX]={0};
    size_t arg_size=process_current_launch_arg(arg,sizeof arg);
    if(expect_document_arg)assert(arg_size==14u && !strcmp(arg,"home/hello.txt"));
    else assert(arg_size==0);
    assert(process_current_space() && process_current_space()->cr3 == active_root);
    assert(gdt_top != 0xffff8000000f0000ULL);
    if (demo_mode) process_record_fault(14, 0xf00d);
    else process_finish(7);
}
int main(void) {
    assert(!process_create_elf("ok", 2));
    assert(process_init());
    assert(!process_init());
    assert(!process_get_info(5, NULL));
    uint64_t pids[PROCESS_MAX_SLOTS];
    for (unsigned i = 0; i < PROCESS_MAX_SLOTS; ++i) {
        pids[i] = process_create_elf("valid", 5);
        assert(pids[i] == (uint64_t)(i + 1));
        process_info_t info;
        assert(process_get_info(pids[i], &info) && info.state == PROCESS_READY);
        assert(info.completion_sequence == 0);
        assert(!process_state_terminal(info.state));
    }
    assert(process_set_launch_arg(pids[0],"home/hello.txt"));
    assert(!process_set_launch_arg(pids[0],NULL));
    assert(!process_set_launch_arg(999,"home/hello.txt"));
    assert(!process_create_elf("full", 4));
    assert(frames == PROCESS_MAX_SLOTS * (4 + 1 + 1 + 4));
    assert(!process_reap(pids[0]));
    process_info_t pending = {0};
    irq_enabled = true;
    assert(!process_wait_ticks(pids[0], 3, &pending));
    assert(sleep_calls == 3 && mock_ticks == 3);
    assert(!process_wait_ticks(pids[0], 0, &pending));
    assert(!process_wait_ticks(0, 10, &pending));
    assert(!process_wait_ticks(pids[0], UINT64_MAX, &pending));
    irq_enabled = false;
    /* A mapped former guard must prevent entry, even with a valid PID. */
    mapped[0] = 0xdead000;
    assert(!process_run(pids[0], NULL));
    mapped[0] = 0;
    process_info_t result;
    for (unsigned i = 0; i < PROCESS_MAX_SLOTS; ++i) {
        demo_mode = (i & 1);
        expect_document_arg=(i==0);
        assert(process_run(pids[i], &result));
        assert(!process_set_launch_arg(pids[i],"home/changed.txt"));
        assert(result.pid == pids[i] && result.status == (demo_mode ? 142 : 7));
        assert(result.state == (demo_mode ? PROCESS_FAULTED : PROCESS_EXITED));
        assert(process_state_terminal(result.state) && result.completion_sequence == i + 1);
        irq_enabled = true;
        process_info_t waited = {0};
        assert(process_wait_ticks(pids[i], 1, &waited) && waited.pid == pids[i]);
        assert(waited.completion_sequence == result.completion_sequence);
        irq_enabled = false;
        assert(!demo_mode || (result.fault_vector == 14 && result.fault_address == 0xf00d));
        assert(!process_current_pid() && active_root == 0x8000);
        assert(gdt_top == 0xffff8000000f0000ULL);
        assert(!process_run(pids[i], NULL));
        assert(process_reap(pids[i]));
        assert(!process_get_info(pids[i], &result));
    }
    assert(frames == 0 && restore_calls == PROCESS_MAX_SLOTS &&
           stack_changes == 2 * PROCESS_MAX_SLOTS);
    for (unsigned i=0; i<SLOT_PAGES; ++i) assert(!mapped[i]);
    assert(process_create_elf("Xbad", 4) == 0 && frames == 0);
    fail_after = 2;
    assert(!process_create_elf("ok", 2) && frames == 0);
    fail_after = 0;
    uint64_t cancelled = process_create_elf("cancel", 6);
    assert(cancelled && frames>0);
    assert(process_cancel_ready(cancelled) && frames==0);
    assert(!process_cancel_ready(cancelled));
    uint64_t new_pid = process_create_elf("ok", 2);
    assert(new_pid > pids[PROCESS_MAX_SLOTS-1]);
    expect_document_arg=0;
    assert(process_run(new_pid, NULL) && process_reap(new_pid) && frames == 0);
    puts("PASS: process table, unique PIDs, guarded stacks, CR3, exit/fault and full reclamation");
    return 0;
}
#elif defined(OPENXP_TEST_APP_LAUNCH)
#include "user/app_launch.h"
#include "arch/x86_64/irq_state.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
    char out[48];app_launch_reset();
    assert(!app_launch_allowed("bin/windowd"));
    assert(!app_launch_allowed("bin/fault"));
    assert(!app_launch_allowed("bin/../paint"));
    assert(!app_launch_submit("bin/evil"));
    assert(!app_launch_take(out,sizeof out));
    assert(!app_launch_take(out,3));
    assert(app_launch_submit("bin/paint"));
    assert(!app_launch_submit("bin/wordpad"));
    assert(app_launch_take(out,sizeof out));
    assert(!strcmp(out,"bin/paint"));
    assert(!app_launch_take(out,sizeof out));
    assert(app_launch_allowed("system apps/notepad.exe"));
    assert(app_launch_allowed("windows/system32/notepad.exe"));
    assert(!app_launch_allowed("windows/system32/evil.exe"));
    assert(app_launch_submit("system apps/notepad.exe"));
    assert(app_launch_take(out,sizeof out));
    assert(!strcmp(out,"bin/notepad"));
    assert(app_launch_submit("bin/wordpad"));
    app_launch_reset();assert(!app_launch_take(out,sizeof out));
    assert(app_launch_submit("bin/filemanager"));
    assert(app_launch_take(out,sizeof out));
    assert(!strcmp(out,"bin/filemanager"));
    app_launch_request_t queued={0};
    assert(app_launch_submit_with_arg("bin/notepad","home/example.txt"));
    assert(!app_launch_submit_with_arg("bin/paint","home/drawing.oxp"));
    assert(app_launch_take_request(&queued));
    assert(!strcmp(queued.executable,"bin/notepad"));
    assert(!strcmp(queued.argument,"home/example.txt"));
    assert(!app_launch_take_request(&queued));
    assert(!app_launch_submit_with_arg("bin/notepad","home/../secret.txt"));
    assert(!app_launch_submit_with_arg("bin/notepad","/etc/passwd"));
    assert(!app_launch_submit_with_arg("bin/notepad","home/"));
    assert(!app_launch_submit_with_arg("bin/notepad","home/foo\\bar"));
    assert(!app_launch_submit_with_arg("bin/windowd","home/example.txt"));
    assert(app_launch_submit("bin/paint"));
    assert(app_launch_take_request(&queued));
    assert(!queued.argument[0]);
    puts("PASS: isolated app launch allowlist, queue, consumption and reset");
    return 0;
}
#elif defined(OPENXP_TEST_CAPIPC)
#include "user/capipc.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_exchange(void) {
    capipc_init();
    assert(capipc_listen(42)==CAPIPC_DENIED);
    assert(capipc_grant_server(42));
    assert(!capipc_grant_server(43));
    assert(capipc_listen(43)==CAPIPC_DENIED);
    assert(capipc_listen(42)==CAPIPC_OK);
    uint64_t a=0,b=0;char buf[CAPIPC_MSG_BYTES]={0};size_t n=0;
    assert(capipc_accept(42,&b)==CAPIPC_EMPTY);
    assert(capipc_connect(7,&a)==CAPIPC_OK);
    assert(capipc_accept(42,&b)==CAPIPC_OK);
    assert(a!=b && a && b);
    assert(capipc_send(8,a,"evil",4)==CAPIPC_DENIED);
    assert(capipc_receive(8,b,buf,sizeof(buf),&n)==CAPIPC_DENIED);
    assert(capipc_send(7,a,"PING",4)==CAPIPC_OK);
    assert(capipc_receive(42,b,buf,3,&n)==CAPIPC_INVALID);
    assert(capipc_receive(42,b,buf,sizeof(buf),&n)==CAPIPC_OK);
    assert(n==4 && memcmp(buf,"PING",4)==0);
    assert(capipc_send(42,b,"PONG",4)==CAPIPC_OK);
    assert(capipc_close(42,b)==CAPIPC_OK);
    assert(capipc_receive(7,a,buf,sizeof(buf),&n)==CAPIPC_OK);
    assert(n==4 && memcmp(buf,"PONG",4)==0);
    assert(capipc_receive(7,a,buf,sizeof(buf),&n)==CAPIPC_CLOSED);
    assert(capipc_send(7,a,"x",1)==CAPIPC_CLOSED);
    assert(capipc_close(7,a)==CAPIPC_OK);
    assert(capipc_send(7,a,"x",1)==CAPIPC_DENIED);
}
static void test_reuse_and_limits(void) {
    capipc_init();assert(capipc_grant_server(99));assert(capipc_listen(99)==CAPIPC_OK);
    uint64_t handles[CAPIPC_MAX_ENDPOINTS/2]={0};
    for(unsigned i=0;i<CAPIPC_MAX_ENDPOINTS/2;++i)
        assert(capipc_connect(10+i,&handles[i])==CAPIPC_OK);
    uint64_t unused=0;
    assert(capipc_connect(77,&unused)==CAPIPC_NOSPACE);
    for(unsigned i=0;i<CAPIPC_MAX_ENDPOINTS/2;++i){
        uint64_t peer=0;assert(capipc_accept(99,&peer)==CAPIPC_OK);
        assert(capipc_close(99,peer)==CAPIPC_OK);
        assert(capipc_close(10+i,handles[i])==CAPIPC_OK);
    }
    assert(capipc_connect(10,&unused)==CAPIPC_OK);
    assert(unused!=handles[0]);
    assert(capipc_close(10,handles[0])==CAPIPC_DENIED);
    capipc_process_exit(10);
    capipc_process_exit(99);
    assert(capipc_connect(100,&unused)==CAPIPC_CLOSED);
    assert(capipc_grant_server(101));
}
/* Six occupied client connections must not make Settings unable to open
 * a fresh control-plane IPC connection to the desktop service. */
static void test_theme_control_capacity(void){
    capipc_init();assert(capipc_grant_server(99));assert(capipc_listen(99)==CAPIPC_OK);
    uint64_t clients[6],servers[6];
    for(unsigned i=0;i<6u;++i){
        assert(capipc_connect(200u+i,&clients[i])==CAPIPC_OK);
        assert(capipc_accept(99,&servers[i])==CAPIPC_OK);
    }
    uint64_t control=0,accepted=0;
    assert(capipc_connect(222,&control)==CAPIPC_OK);
    assert(capipc_accept(99,&accepted)==CAPIPC_OK);
    assert(capipc_send(222,control,"T",1u)==CAPIPC_OK);
    char reply[2]={0};size_t got=0;
    assert(capipc_receive(99,accepted,reply,sizeof(reply),&got)==CAPIPC_OK);
    assert(got==1u && reply[0]=='T');
    assert(capipc_close(99,accepted)==CAPIPC_OK);
    assert(capipc_close(222,control)==CAPIPC_OK);
    for(unsigned i=0;i<6u;++i){
        assert(capipc_close(99,servers[i])==CAPIPC_OK);
        assert(capipc_close(200u+i,clients[i])==CAPIPC_OK);
    }
}
static void test_client_quota(void) {
    capipc_init();assert(capipc_grant_server(7));assert(capipc_listen(7)==CAPIPC_OK);
    uint64_t a=0,b=0,c=0;
    assert(capipc_connect(8,&a)==CAPIPC_OK);
    assert(capipc_connect(8,&b)==CAPIPC_OK);
    assert(capipc_connect(8,&c)==CAPIPC_NOSPACE);
    assert(capipc_close(8,a)==CAPIPC_OK);
    assert(capipc_connect(8,&c)==CAPIPC_OK);
}
static void test_server_wait_any(void){
    capipc_init();
    assert(capipc_grant_server(50));
    assert(capipc_listen(50)==CAPIPC_OK);
    assert(capipc_wait(51,CAPIPC_ANY_WAIT_HANDLE,5)==CAPIPC_DENIED);
    assert(capipc_wait(50,CAPIPC_ANY_WAIT_HANDLE,5)==CAPIPC_EMPTY);
    uint64_t a=0,b=0,c=0,d=0;
    assert(capipc_connect(60,&a)==CAPIPC_OK);
    assert(capipc_wait(50,CAPIPC_ANY_WAIT_HANDLE,5)==CAPIPC_OK);
    assert(capipc_accept(50,&b)==CAPIPC_OK);
    assert(capipc_wait(50,CAPIPC_ANY_WAIT_HANDLE,5)==CAPIPC_EMPTY);
    assert(capipc_connect(61,&c)==CAPIPC_OK);
    assert(capipc_accept(50,&d)==CAPIPC_OK);
    assert(capipc_send(61,c,"2",1)==CAPIPC_OK);
    assert(capipc_wait(50,CAPIPC_ANY_WAIT_HANDLE,5)==CAPIPC_OK);
    char tmp[2]={0};size_t len=0;
    assert(capipc_receive(50,d,tmp,sizeof(tmp),&len)==CAPIPC_OK && len==1);
    assert(capipc_wait(50,CAPIPC_ANY_WAIT_HANDLE,5)==CAPIPC_EMPTY);
    assert(capipc_close(60,a)==CAPIPC_OK);
    assert(capipc_wait(50,CAPIPC_ANY_WAIT_HANDLE,5)==CAPIPC_OK);
    assert(capipc_close(50,b)==CAPIPC_OK);
    assert(capipc_wait(50,CAPIPC_ANY_WAIT_HANDLE,5)==CAPIPC_EMPTY);
    assert(capipc_close(50,d)==CAPIPC_OK);
}
static void test_overflow_and_exit(void){
    capipc_init();assert(capipc_grant_server(50));assert(capipc_listen(50)==CAPIPC_OK);
    uint64_t client=0,server=0;
    assert(capipc_connect(60,&client)==CAPIPC_OK);
    assert(capipc_accept(50,&server)==CAPIPC_OK);
    uint8_t payload[CAPIPC_MSG_BYTES]={0};size_t n=0;
    assert(capipc_send(60,client,payload,0)==CAPIPC_INVALID);
    assert(capipc_send(60,client,payload,CAPIPC_MSG_BYTES+1)==CAPIPC_INVALID);
    for(unsigned i=0;i<CAPIPC_QUEUE_CAP;++i)assert(capipc_send(60,client,payload,sizeof(payload))==CAPIPC_OK);
    assert(capipc_send(60,client,payload,sizeof(payload))==CAPIPC_FULL);
    capipc_process_exit(60);
    for(unsigned i=0;i<CAPIPC_QUEUE_CAP;++i)assert(capipc_receive(50,server,payload,sizeof(payload),&n)==CAPIPC_OK && n==sizeof(payload));
    assert(capipc_receive(50,server,payload,sizeof(payload),&n)==CAPIPC_CLOSED);
    assert(capipc_close(50,server)==CAPIPC_OK);
}
int main(void){test_exchange();test_reuse_and_limits();test_overflow_and_exit();test_client_quota();test_theme_control_capacity();test_server_wait_any();puts("capipc_test: passed");return 0;}
#elif defined(OPENXP_TEST_SYSCALL)
#include "user/syscall.h"
#include "user/shell_abi.h"
#include "user/app_file.h"
#include "user/app_launch.h"
#include "arch/x86_64/rtc.h"
#include "user/wm_display.h"
#include "user/capipc.h"
#include "gui/native.h"
#include "mm/address_space.h"
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
static bool active;
static uint64_t exit_status;
static char emitted;
static address_space_t mock_space = { .cr3 = 4096 };
static char output[128];
static char mock_user_memory[64] = "Hello from user memory";
static union { app_file_request_t file; native_bitmap_req_t bitmap; unsigned char bytes[sizeof(native_bitmap_req_t)]; } mock_storage;
#define mock_file mock_storage.file
static size_t output_len;
const address_space_t *userspace_current_space(void) { return active ? &mock_space : NULL; }
bool aspace_range_valid(const address_space_t *s, uint64_t ptr, size_t len, bool writable) {
    if(s != &mock_space)return false;
    if(ptr>=2000 && ptr-2000<=sizeof(mock_storage))return len<=sizeof(mock_storage)-(size_t)(ptr-2000);
    return !writable && ptr>=1000 && ptr-1000<=64 && len<=64-(size_t)(ptr-1000);
}
bool aspace_copy_from_user(const address_space_t *s, void *dst, uint64_t ptr, size_t len) {
    if (!aspace_range_valid(s,ptr,len,false)) return false;
    if(ptr>=2000)memcpy(dst,mock_storage.bytes+(ptr-2000),len);
    else memcpy(dst,mock_user_memory+(ptr-1000),len);
    return true;
}

bool aspace_copy_to_user(address_space_t *s,uint64_t dst,const void *src,size_t len) {
    if(!aspace_range_valid(s,dst,len,true))return false;
    memcpy(mock_storage.bytes+(dst-2000),src,len);return true;
}
uint64_t process_current_pid(void) { return active ? 17 : 0; }
size_t process_current_launch_arg(char *out,size_t cap){
    const char *s="home/hello.txt";
    if(!active || cap<15u)return 0;
    memcpy(out,s,15u);return 14u;
}
bool userspace_active(void) { return active; }
void userspace_finish(uint64_t status) { exit_status = status; active = false; }
void serial_putc(char ch) { emitted = ch; if (output_len < sizeof(output)) output[output_len++] = ch; }
uint64_t timer_ticks(void) { return 1234; }
uint64_t pmm_free_pages(void) { return 437; }
bool rtc_read_date(rtc_date_t *out) { if(!out)return false;*out=(rtc_date_t){2026,10,13};return true; }
int main(void) {
    struct exception_frame f = {0};
    struct syscall_result r;
    f.cs = 0x23;
    f.rax = OPENXP_SYS_VERSION;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EPERM);
    active = true;
    f.cs = 0x08;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EPERM);
    f.cs = 0x23;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == OPENXP_API_VERSION);
    f.rax=OPENXP_SYS_RTC_DATE;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result == ((2026u<<16)|(10u<<8)|13u));
    f.rax=OPENXP_SYS_FREE_PAGES;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==437);
    f.rax = OPENXP_SYS_PUTC;
    f.rdi = 'X';
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 0 && emitted == 'X');
    f.rdi = 200;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EPERM && emitted == 'X');
    f.rax = OPENXP_SYS_WRITE;
    f.rdi = 1000;
    f.rsi = 22;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 22 && output_len == 23);
    assert(!memcmp(output+1,"Hello from user memory",22));
    f.rdi = 999;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EFAULT && output_len == 23);
    f.rdi = 1000;
    f.rsi = 1025;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EINVAL && output_len == 23);
    f.rax = OPENXP_SYS_GETPID;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 17);
    f.rax = OPENXP_SYS_TICKS;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 1234);
    /* Aurora 13 clipboard syscalls validate both directions and never
     * overwrite the stored selection on a rejected user pointer/length. */
    f.rax=OPENXP_SYS_CLIPBOARD_SET;f.rdi=2000;f.rsi=5;
    memcpy(mock_storage.bytes,"hello",5);
    r=user_syscall_dispatch(&f);assert(r.result==5);
    f.rdi=999;
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EFAULT);
    f.rdi=2000;f.rsi=961;
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EINVAL);
    f.rax=OPENXP_SYS_CLIPBOARD_GET;f.rsi=4;
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_ENOSPC);
    f.rsi=5;f.rdi=999;
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EFAULT);
    f.rdi=2000;memset(mock_storage.bytes,0,5);
    r=user_syscall_dispatch(&f);
    assert(r.result==5 && !memcmp(mock_storage.bytes,"hello",5));
    f.rax=OPENXP_SYS_CLIPBOARD_SET;f.rsi=0;f.rdi=0;
    r=user_syscall_dispatch(&f);assert(r.result==0);
    f.rax=OPENXP_SYS_CLIPBOARD_GET;f.rdi=2000;f.rsi=0;
    r=user_syscall_dispatch(&f);assert(r.result==0);
    f.rax=OPENXP_SYS_APP_GET_ARG;f.rdi=2000;f.rsi=10;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_ENOSPC);
    f.rsi=96;
    r=user_syscall_dispatch(&f);
    assert(r.result==14 && !strcmp((char*)mock_storage.bytes,"home/hello.txt"));
    f.rax=OPENXP_SYS_APP_LAUNCH_ARG;
    app_launch_request_t *launch=(app_launch_request_t*)mock_storage.bytes;
    memset(launch,0,sizeof *launch);
    strcpy(launch->executable,"bin/notepad");
    strcpy(launch->argument,"home/hello.txt");
    f.rdi=2000;f.rsi=0;
    r=user_syscall_dispatch(&f);assert(r.result==0);
    app_launch_request_t queued;
    assert(app_launch_take_request(&queued));
    assert(!strcmp(queued.executable,"bin/notepad"));
    assert(!strcmp(queued.argument,"home/hello.txt"));
    strcpy(launch->argument,"home/../oops");
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EAGAIN);
    f.rax=OPENXP_SYS_STORAGE;f.rdi=0;
    r=user_syscall_dispatch(&f);assert(r.result==0);
    f.rdi=1;r=user_syscall_dispatch(&f);assert(r.result==0);
    native_reset();
    f.rax = OPENXP_SYS_GUI_OPEN;
    f.rdi = 999;f.rsi = 5;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EFAULT);
    f.rdi = 1000;f.rsi = 5;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 0 && native_snapshot().active);
    f.rax=OPENXP_SYS_GUI_CANVAS_SIZE;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==((uint64_t)320<<32|180u));
    native_set_window_size(0,1024u,726u);
    r=user_syscall_dispatch(&f);
    assert(!r.exit && (unsigned)(r.result>>32)>320u && (unsigned)r.result==360u);
    assert(native_next_event(17)==NATIVE_EVENT_RESIZE);
    f.rax = OPENXP_SYS_GUI_TEXT;
    f.rsi = 22;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 0 && !strcmp(native_snapshot().text,"Hello from user memory"));
    f.rsi = 4096;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_EINVAL);
    f.rax=OPENXP_SYS_GUI_DRAW;
    f.rdi=999;f.rsi=1;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EFAULT);
    f.rdi=1000;f.rsi=0;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EINVAL);
    f.rsi=NATIVE_DRAW_MAX+1;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EINVAL);
    native_push_event('Q');
    f.rax = OPENXP_SYS_GUI_EVENT;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 'Q');
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == 0);
    f.rax=OPENXP_SYS_GUI_WAIT;
    f.rdi=UINT64_MAX;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EINVAL);
    f.rdi=0;
    native_push_event('W');
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result=='W');
    /* User-mode path operations copy the entire request and return bounded data. */
    f.rax=OPENXP_SYS_FS_OP;f.rsi=2000;f.rdi=APP_FS_WRITE;
    memset(&mock_file,0,sizeof mock_file);
    strcpy(mock_file.path,"home/syscall.txt");
    memcpy(mock_file.data,"TEST",4);mock_file.size=4;
    r=user_syscall_dispatch(&f);assert(r.result==0);
    f.rdi=APP_FS_READ;
    memset(mock_file.data,0,sizeof mock_file.data);
    r=user_syscall_dispatch(&f);
    assert(r.result==0 && mock_file.size==4 && !memcmp(mock_file.data,"TEST",4));
    f.rdi=APP_FS_RENAME;strcpy(mock_file.dest,"home/newname.txt");
    r=user_syscall_dispatch(&f);assert(r.result==0);
    f.rdi=APP_FS_COPY;strcpy(mock_file.path,"home/newname.txt");
    strcpy(mock_file.dest,"home/cloned.txt");
    r=user_syscall_dispatch(&f);assert(r.result==0);
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rdi=APP_FS_DELETE;strcpy(mock_file.path,"home/cloned.txt");
    r=user_syscall_dispatch(&f);assert(r.result==0);
    f.rdi=APP_FS_KIND;strcpy(mock_file.path,"home/newname.txt");
    r=user_syscall_dispatch(&f);assert(r.result==2);
    f.rdi=APP_FS_DELETE;r=user_syscall_dispatch(&f);assert(r.result==0);
    f.rdi=APP_FS_WRITE;strcpy(mock_file.path,"bin/hello");
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rdi=APP_FS_LIST;mock_file.index=0;
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EINVAL || r.result==0);
    f.rdi=APP_FS_READ;mock_file.path[0]='h';
    memset(mock_file.path,'A',sizeof mock_file.path);
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EINVAL);
    f.rax=OPENXP_SYS_APP_LAUNCH;
    memcpy(mock_user_memory,"bin/paint",9);
    f.rdi=1000;f.rsi=9;
    r=user_syscall_dispatch(&f);assert(r.result==0);
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EAGAIN);
    memcpy(mock_user_memory,"bin/fault",9);
    r=user_syscall_dispatch(&f);assert(r.result==(uint64_t)-OPENXP_EPERM);
    char pending[48];assert(app_launch_take(pending,sizeof pending));
    assert(!strcmp(pending,"bin/paint"));
    f.rax = 0xffff;
    r = user_syscall_dispatch(&f);
    assert(!r.exit && r.result == (uint64_t)-OPENXP_ENOSYS);
    /* A normal user task may not register as the trusted window server. */
    f.rax = OPENXP_SYS_IPC_LISTEN;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EPERM);
    f.rax = OPENXP_SYS_IPC_CONNECT;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EPIPE);
    f.rax = OPENXP_SYS_IPC_SEND; f.rdi=0; f.rsi=999; f.rdx=4;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EFAULT);
    f.rsi=1000;f.rdx=65;
    r=user_syscall_dispatch(&f);
    assert(!r.exit && r.result==(uint64_t)-OPENXP_EINVAL);
    /* Only an accepted channel owned by the authorized window server may
     * move/focus/close a connected client's *actual* native window. */
    f.rax=OPENXP_SYS_WM_DISPLAY;f.rdi=0;f.rsi=1000;
    f.rdx=sizeof(openxp_wm_display_cmd_t);
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    /* Untrusted apps cannot replace the desktop input policy or frame. */
    for(uint64_t forbidden=OPENXP_SYS_SHELL_EVENT;
        forbidden<=OPENXP_SYS_SHELL_WINDOW;++forbidden){
        f.rax=forbidden;
        r=user_syscall_dispatch(&f);
        assert(!r.exit && r.result==(uint64_t)-OPENXP_EPERM);
    }
    f.rax=OPENXP_SYS_SHELL_RASTER_ROW;
    f.rdi=1000;f.rsi=sizeof(oxp_shell_raster_row_t);
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rax=OPENXP_SYS_SHELL_RASTER_COMMIT;
    f.rdi=OXP_SHELL_MASK_BACKGROUND;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rax=OPENXP_SYS_NATIVE_INFO;
    f.rdi=0;f.rsi=2000;f.rdx=sizeof(oxp_native_info_t);
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rax=OPENXP_SYS_NATIVE_ROW;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rax=OPENXP_SYS_WM_DISPLAY;
    capipc_init();
    assert(capipc_grant_server(17) && capipc_listen(17)==CAPIPC_OK);
    /* The service is authorized but must still pass a valid, readable
     * 2056-byte scanline. An unprivileged or forged pointer never works. */
    f.rax=OPENXP_SYS_SHELL_RASTER_ROW;
    f.rdi=1000;f.rsi=sizeof(oxp_shell_raster_row_t);
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EFAULT);
    f.rax=OPENXP_SYS_SHELL_RASTER_COMMIT;
    f.rdi=OXP_SHELL_MASK_BACKGROUND;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EINVAL);
    f.rax=OPENXP_SYS_NATIVE_INFO;
    f.rdi=4;f.rsi=2000;f.rdx=sizeof(oxp_native_info_t);
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EINVAL);
    f.rdi=0;
    r=user_syscall_dispatch(&f);
    assert(r.result==0);
    oxp_native_info_t info;
    memcpy(&info,&mock_file,sizeof(info));
    assert(info.active==1 && info.generation!=0);
    f.rax=OPENXP_SYS_NATIVE_ROW;
    f.rdx=0;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EINVAL); /* not painted yet */
    native_draw_cmd_t sample={.op=NATIVE_DRAW_CLEAR,.color=0x00ff00};
    assert(native_draw(17,&sample,1));
    f.rax=OPENXP_SYS_NATIVE_ROW;
    r=user_syscall_dispatch(&f);
    assert(r.result==0);
    oxp_native_row_t row;
    memcpy(&row,mock_storage.bytes,sizeof(row));
    assert(row.slot==0 && row.y==0 && row.pixels[0]==0x07e0u);
    f.rdx=OXP_NATIVE_H;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EINVAL);
    /* System call 34 copies, validates and alpha-blends a caller-owned tile.
     * A forged pointer is rejected before any app pixel is changed. */
    f.rax=OPENXP_SYS_GUI_BITMAP;f.rdi=999;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EFAULT);
    f.rdi=2000;
    mock_storage.bitmap=(native_bitmap_req_t){0};
    mock_storage.bitmap.x=8;mock_storage.bitmap.y=8;
    mock_storage.bitmap.w=1;mock_storage.bitmap.h=1;
    mock_storage.bitmap.pixels[0]=0x1fu;
    mock_storage.bitmap.pixels[1]=0x00u;
    mock_storage.bitmap.pixels[2]=255u;
    r=user_syscall_dispatch(&f);
    assert(r.result==0 && native_test_pixel(0,8,8)==0x001fu);
    mock_storage.bitmap.w=33;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EINVAL);
    f.rax=OPENXP_SYS_WM_DISPLAY;
    f.rsi=1000;
    uint64_t client_handle=0,server_handle=0;
    assert(capipc_connect(18,&client_handle)==CAPIPC_OK);
    assert(capipc_accept(17,&server_handle)==CAPIPC_OK);
    assert(native_open(18,"Client",6));
    openxp_wm_display_cmd_t cmd={OPENXP_WM_SET_RECT,90,60,240,180};
    memcpy(mock_user_memory,&cmd,sizeof(cmd));
    f.rdi=client_handle;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rdi=server_handle;f.rdx=19;
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EINVAL);
    f.rdx=sizeof(cmd);
    r=user_syscall_dispatch(&f);
    assert(r.result==0 && native_snapshot_slot(1).wm_x==90 &&
           native_snapshot_slot(1).wm_y==60);
    cmd.action=OPENXP_WM_RAISE;
    memcpy(mock_user_memory,&cmd,sizeof(cmd));
    r=user_syscall_dispatch(&f);
    assert(r.result==0 && native_snapshot_slot(1).wm_focus_serial);
    assert(native_snapshot_slot(0).wm_geometry_serial==0);
    cmd.action=OPENXP_WM_REQUEST_CLOSE;
    memcpy(mock_user_memory,&cmd,sizeof(cmd));
    r=user_syscall_dispatch(&f);
    assert(r.result==0 && native_next_event(18)==NATIVE_EVENT_CLOSE);
    assert(capipc_close(18,client_handle)==CAPIPC_OK);
    r=user_syscall_dispatch(&f);
    assert(r.result==(uint64_t)-OPENXP_EPERM);
    f.rax = OPENXP_SYS_EXIT;
    f.rdi = 77;
    r = user_syscall_dispatch(&f);
    assert(r.exit && exit_status == 77 && !active && !native_snapshot().active);
    puts("PASS: syscall authorization, scalar ABI, exit, rejected requests");
    return 0;
}
#elif defined(OPENXP_TEST_WM_PROTOCOL)
#include "../apps/native/wm_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static wm_packet_t request(uint32_t op,uint32_t id){
    return (wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,.operation=op,.window_id=id};
}
static wm_packet_t send(wm_state_t *s,uint64_t owner,wm_packet_t r){
    wm_packet_t answer;wm_state_process(s,owner,&r,&answer);
    assert(answer.magic==WM_MAGIC&&answer.version==WM_VERSION);
    assert(answer.operation==(r.operation|WM_REPLY_FLAG));
    return answer;
}
int main(void){
    wm_state_t s;wm_state_init(&s);
    wm_packet_t a=request(WM_CREATE,0);
    a.x=16;a.y=32;a.width=240;a.height=140;
    memcpy(a.title,"Client A",9);
    wm_packet_t out=send(&s,11,a);
    assert(out.status==WM_OK&&out.window_id&&out.width==240);
    assert(!strcmp(out.title,"Client A"));
    uint32_t first=out.window_id;
    assert(wm_state_active_window(&s)==first);
    assert(send(&s,11,a).status==WM_NO_SLOTS);
    a=request(WM_QUERY,first);
    assert(send(&s,12,a).status==WM_NOT_FOUND);
    assert(send(&s,11,a).status==WM_OK);
    a=request(WM_MOVE,first);a.x=60;a.y=90;
    out=send(&s,11,a);assert(out.status==WM_OK&&out.x==60&&out.y==90);
    a.x=UINT32_MAX;assert(send(&s,11,a).status==WM_BAD_GEOMETRY);
    assert(send(&s,11,request(WM_QUERY,first)).x==60);
    a=request(WM_CREATE,0);a.x=30;a.y=32;a.width=300;a.height=190;
    out=send(&s,12,a);uint32_t second=out.window_id;
    assert(out.status==WM_OK && second!=first);
    assert(wm_state_active_window(&s)==second);
    assert(send(&s,11,request(WM_FOCUS,first)).status==WM_OK);
    assert(wm_state_active_window(&s)==first);
    assert(send(&s,12,request(WM_CLOSE,first)).status==WM_NOT_FOUND);
    assert(send(&s,11,request(WM_CLOSE,first)).status==WM_OK);
    assert(send(&s,11,request(WM_QUERY,first)).status==WM_NOT_FOUND);
    assert(wm_state_active_window(&s)==second);
    /* Reuse increments generation; old IDs cannot regain authority. */
    out=send(&s,11,a);
    assert(out.status==WM_OK&&out.window_id!=first);
    wm_state_disconnect(&s,11);
    assert(send(&s,11,request(WM_QUERY,out.window_id)).status==WM_NOT_FOUND);
    assert(wm_state_active_window(&s)==second);
    wm_state_disconnect(&s,12);assert(wm_state_active_window(&s)==0);
    a=request(WM_CREATE,0);a.width=1;a.height=100;
    assert(send(&s,11,a).status==WM_BAD_GEOMETRY);
    a.width=250;a.x=UINT32_MAX;
    assert(send(&s,11,a).status==WM_BAD_GEOMETRY);
    a.x=0;
    for(unsigned i=0;i<WM_MAX_WINDOWS;++i)assert(send(&s,11u+i,a).status==WM_OK);
    assert(send(&s,11+WM_MAX_WINDOWS,a).status==WM_NO_SLOTS);
    a.version=2;assert(send(&s,11,a).status==WM_BAD_REQUEST);
    a.version=WM_VERSION;a.magic=0;assert(send(&s,11,a).status==WM_BAD_REQUEST);
    assert(send(&s,11,request(WM_PING,0)).status==WM_OK);
    puts("wm_protocol_test: passed");
    return 0;
}
#elif defined(OPENXP_TEST_WM_CAPIPC)
#include "../apps/native/wm_protocol.h"
#include "user/capipc.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static wm_packet_t transact(wm_state_t *s,uint64_t client_pid,uint64_t client_handle,
                             uint64_t server_handle,wm_packet_t req){
    wm_packet_t reply={0}, wire={0};size_t len=0;
    assert(capipc_send(client_pid,client_handle,&req,sizeof(req))==CAPIPC_OK);
    assert(capipc_receive(100,server_handle,&wire,sizeof(wire),&len)==CAPIPC_OK);
    assert(len==sizeof(wire));
    wm_state_process(s,server_handle,&wire,&reply);
    assert(capipc_send(100,server_handle,&reply,sizeof(reply))==CAPIPC_OK);
    assert(capipc_receive(client_pid,client_handle,&wire,sizeof(wire),&len)==CAPIPC_OK);
    assert(len==sizeof(wire) && wire.magic==WM_MAGIC && wire.version==WM_VERSION);
    return wire;
}
int main(void){
    wm_state_t s;wm_state_init(&s);capipc_init();
    assert(capipc_grant_server(100));assert(capipc_listen(100)==CAPIPC_OK);
    uint64_t ca=0,sa=0,cb=0,sb=0;
    assert(capipc_connect(201,&ca)==CAPIPC_OK && capipc_accept(100,&sa)==CAPIPC_OK);
    assert(capipc_connect(202,&cb)==CAPIPC_OK && capipc_accept(100,&sb)==CAPIPC_OK);
    uint64_t peer=999;
    assert(capipc_server_peer(100,sa,&peer) && peer==201);
    assert(capipc_server_peer(100,sb,&peer) && peer==202);
    assert(!capipc_server_peer(201,ca,&peer) && peer==0);
    assert(!capipc_server_peer(100,ca,&peer));
    assert(!capipc_server_peer(100,UINT64_MAX,&peer));
    wm_packet_t create={.magic=WM_MAGIC,.version=WM_VERSION,.operation=WM_CREATE,
                        .width=220,.height=120,.x=10,.y=20,.title="Owned"};
    wm_packet_t a=transact(&s,201,ca,sa,create),b=transact(&s,202,cb,sb,create);
    assert(a.status==WM_OK && b.status==WM_OK && a.window_id!=b.window_id);
    wm_packet_t query={.magic=WM_MAGIC,.version=WM_VERSION,.operation=WM_QUERY,
                       .window_id=b.window_id};
    /* IPC channel ownership prevents reading another process's window. */
    wm_packet_t denial=transact(&s,201,ca,sa,query);
    assert(denial.status==WM_NOT_FOUND);
    assert(transact(&s,202,cb,sb,query).status==WM_OK);
    assert(capipc_send(202,ca,&create,sizeof(create))==CAPIPC_DENIED);
    assert(capipc_close(201,ca)==CAPIPC_OK);
    wm_state_disconnect(&s,sa);
    assert(capipc_close(100,sa)==CAPIPC_OK);
    assert(!capipc_server_peer(100,sa,&peer));
    assert(capipc_server_peer(100,sb,&peer) && peer==202);
    assert(wm_state_active_window(&s)==b.window_id);
    assert(transact(&s,202,cb,sb,query).status==WM_OK);
    /* A third client may connect after the first exits; neither the
     * disconnected owner's ID nor handle can affect the new client. */
    uint64_t cc=0,sc=0;
    assert(capipc_connect(203,&cc)==CAPIPC_OK);
    assert(capipc_accept(100,&sc)==CAPIPC_OK);
    assert(capipc_server_peer(100,sc,&peer) && peer==203);
    wm_packet_t third=transact(&s,203,cc,sc,create);
    assert(third.status==WM_OK && third.window_id!=a.window_id);
    assert(transact(&s,203,cc,sc,query).status==WM_NOT_FOUND);
    assert(transact(&s,203,cc,sc,(wm_packet_t){.magic=WM_MAGIC,.version=WM_VERSION,.operation=WM_QUERY,.window_id=third.window_id}).status==WM_OK);
    assert(capipc_wait(100,CAPIPC_ANY_WAIT_HANDLE,8)==CAPIPC_EMPTY);
    assert(capipc_send(203,cc,&query,sizeof(query))==CAPIPC_OK);
    assert(capipc_wait(100,CAPIPC_ANY_WAIT_HANDLE,8)==CAPIPC_OK);
    size_t got=0;wm_packet_t discard;
    assert(capipc_receive(100,sc,&discard,sizeof(discard),&got)==CAPIPC_OK);
    assert(capipc_close(203,cc)==CAPIPC_OK);
    assert(capipc_wait(100,CAPIPC_ANY_WAIT_HANDLE,8)==CAPIPC_OK);
    wm_state_disconnect(&s,sc);assert(capipc_close(100,sc)==CAPIPC_OK);
    capipc_process_exit(202);wm_state_disconnect(&s,sb);
    assert(wm_state_active_window(&s)==0);
    puts("wm_capipc_test: passed");
    return 0;
}
#else
#error "Compile this suite with an OPENXP_TEST_* selector"
#endif
