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
