#include "user/capipc.h"
#ifndef OPENXP_CAPIPC_HOST_TEST
#include "arch/x86_64/irq_state.h"
#include "sched/scheduler.h"
#endif

#define INVALID_SLOT UINT32_MAX
#define MAX_CLIENT_CHANNELS 2u
struct message { uint8_t data[CAPIPC_MSG_BYTES]; uint8_t len; };
struct endpoint {
    uint64_t pid, waiter;
    uint32_t generation, peer;
    uint8_t head, count;
    bool used, accepted, peer_closed;
    struct message queue[CAPIPC_QUEUE_CAP];
};
static struct endpoint ep[CAPIPC_MAX_ENDPOINTS];
static uint32_t generations[CAPIPC_MAX_ENDPOINTS];
static uint64_t authorized_server, listening_server, accept_waiter;

static uint64_t lock_ipc(void) {
#ifndef OPENXP_CAPIPC_HOST_TEST
    return irq_save_disable();
#else
    return 0;
#endif
}
static void unlock_ipc(uint64_t flags) {
#ifndef OPENXP_CAPIPC_HOST_TEST
    irq_restore(flags);
#else
    (void)flags;
#endif
}
static void wake(uint64_t tid) {
#ifndef OPENXP_CAPIPC_HOST_TEST
    if (tid) (void)scheduler_wake(tid);
#else
    (void)tid;
#endif
}
static uint64_t token(unsigned i) {
    return ((uint64_t)ep[i].generation << 32) | (uint64_t)(i+1u);
}
static int lookup(uint64_t pid, uint64_t handle) {
    uint32_t lo=(uint32_t)handle;
    if(!pid || !lo || lo>CAPIPC_MAX_ENDPOINTS || !(handle>>32)) return -1;
    unsigned i=lo-1;
    return ep[i].used && ep[i].pid==pid && ep[i].generation==(uint32_t)(handle>>32) ? (int)i : -1;
}
static int free_slot(void) {
    for(unsigned i=0;i<CAPIPC_MAX_ENDPOINTS;++i)if(!ep[i].used)return (int)i;
    return -1;
}
static void wipe(unsigned i) {
    /* The generation counter survives reuse, preventing stale-handle access. */
    ep[i]=(struct endpoint){0};
    ++generations[i];
    if(!generations[i])++generations[i];
}
static void close_slot(unsigned i) {
    if(!ep[i].used)return;
    uint32_t other=ep[i].peer;
    wake(ep[i].waiter);
    if(other<CAPIPC_MAX_ENDPOINTS && ep[other].used && ep[other].peer==i) {
        ep[other].peer=INVALID_SLOT;
        ep[other].peer_closed=true;
        wake(ep[other].waiter);
    }
    wipe(i);
}
void capipc_init(void) {
    uint64_t flags=lock_ipc();
    for(unsigned i=0;i<CAPIPC_MAX_ENDPOINTS;++i)wipe(i);
    authorized_server=0;listening_server=0;accept_waiter=0;
    unlock_ipc(flags);
}
bool capipc_grant_server(uint64_t pid) {
    if(!pid)return false;
    uint64_t flags=lock_ipc();
    bool ok=!authorized_server && !listening_server;
    if(ok)authorized_server=pid;
    unlock_ipc(flags);return ok;
}
capipc_result_t capipc_listen(uint64_t pid) {
    uint64_t flags=lock_ipc();
    capipc_result_t r=pid && pid==authorized_server && (!listening_server || listening_server==pid)
        ? CAPIPC_OK:CAPIPC_DENIED;
    if(r==CAPIPC_OK)listening_server=pid;
    unlock_ipc(flags);return r;
}
capipc_result_t capipc_connect(uint64_t pid,uint64_t *handle) {
    if(!pid || !handle)return CAPIPC_INVALID;
    uint64_t flags=lock_ipc();
    if(!listening_server || pid==listening_server) {unlock_ipc(flags);return CAPIPC_CLOSED;}
    unsigned owned=0;
    for(unsigned i=0;i<CAPIPC_MAX_ENDPOINTS;++i)
        if(ep[i].used && ep[i].pid==pid)++owned;
    if(owned>=MAX_CLIENT_CHANNELS){unlock_ipc(flags);return CAPIPC_NOSPACE;}
    int a=free_slot();
    if(a<0){unlock_ipc(flags);return CAPIPC_NOSPACE;}
    ep[a].used=true; /* reserve client slot before selecting server slot */
    int b=free_slot();
    if(b<0){ep[a].used=false;unlock_ipc(flags);return CAPIPC_NOSPACE;}
    uint32_t ga=generations[a],gb=generations[b];
    ep[a]=(struct endpoint){.used=true,.accepted=true,.pid=pid,.generation=ga,.peer=(uint32_t)b};
    ep[b]=(struct endpoint){.used=true,.accepted=false,.pid=listening_server,.generation=gb,.peer=(uint32_t)a};
    *handle=token((unsigned)a);
    wake(accept_waiter);
    unlock_ipc(flags);return CAPIPC_OK;
}
capipc_result_t capipc_accept(uint64_t pid,uint64_t *handle) {
    if(!pid || !handle)return CAPIPC_INVALID;
    uint64_t flags=lock_ipc();
    if(pid!=listening_server){unlock_ipc(flags);return CAPIPC_DENIED;}
    for(unsigned i=0;i<CAPIPC_MAX_ENDPOINTS;++i)
        if(ep[i].used && ep[i].pid==pid && !ep[i].accepted && !ep[i].peer_closed) {
            ep[i].accepted=true;*handle=token(i);
            unlock_ipc(flags);return CAPIPC_OK;
        }
    unlock_ipc(flags);return CAPIPC_EMPTY;
}
capipc_result_t capipc_send(uint64_t pid,uint64_t handle,const void *buf,size_t len) {
    if(!buf || !len || len>CAPIPC_MSG_BYTES)return CAPIPC_INVALID;
    uint64_t flags=lock_ipc();
    int index=lookup(pid,handle);
    if(index<0){unlock_ipc(flags);return CAPIPC_DENIED;}
    struct endpoint *self=&ep[index];
    unsigned peer=self->peer;
    if(self->peer_closed || peer>=CAPIPC_MAX_ENDPOINTS || !ep[peer].used ||
       ep[peer].peer!=(uint32_t)index) {unlock_ipc(flags);return CAPIPC_CLOSED;}
    struct endpoint *dst=&ep[peer];
    if(dst->count==CAPIPC_QUEUE_CAP){unlock_ipc(flags);return CAPIPC_FULL;}
    unsigned tail=(unsigned)(dst->head+dst->count)%CAPIPC_QUEUE_CAP;
    for(size_t i=0;i<len;++i)dst->queue[tail].data[i]=((const uint8_t *)buf)[i];
    dst->queue[tail].len=(uint8_t)len;
    ++dst->count;
    wake(dst->waiter);
    unlock_ipc(flags);return CAPIPC_OK;
}
capipc_result_t capipc_receive(uint64_t pid,uint64_t handle,void *buf,size_t cap,size_t *len) {
    if(!buf || !len || cap==0)return CAPIPC_INVALID;
    uint64_t flags=lock_ipc();
    int index=lookup(pid,handle);
    if(index<0){unlock_ipc(flags);return CAPIPC_DENIED;}
    struct endpoint *p=&ep[index];
    if(!p->count){capipc_result_t r=p->peer_closed?CAPIPC_CLOSED:CAPIPC_EMPTY;
        unlock_ipc(flags);return r;}
    unsigned n=p->queue[p->head].len;
    if(cap<n){unlock_ipc(flags);return CAPIPC_INVALID;}
    for(unsigned i=0;i<n;++i)((uint8_t *)buf)[i]=p->queue[p->head].data[i];
    p->head=(p->head+1u)%CAPIPC_QUEUE_CAP;--p->count;
    *len=n;
    unlock_ipc(flags);return CAPIPC_OK;
}
capipc_result_t capipc_close(uint64_t pid,uint64_t handle) {
    uint64_t flags=lock_ipc();int i=lookup(pid,handle);
    if(i<0){unlock_ipc(flags);return CAPIPC_DENIED;}
    close_slot((unsigned)i);unlock_ipc(flags);return CAPIPC_OK;
}
void capipc_process_exit(uint64_t pid) {
    if(!pid)return;
    uint64_t flags=lock_ipc();
    if(authorized_server==pid)authorized_server=0;
    if(listening_server==pid){listening_server=0;wake(accept_waiter);accept_waiter=0;}
    for(unsigned i=0;i<CAPIPC_MAX_ENDPOINTS;++i)
        if(ep[i].used && ep[i].pid==pid)close_slot(i);
    unlock_ipc(flags);
}
capipc_result_t capipc_wait(uint64_t pid,uint64_t handle,uint64_t timeout_ticks) {
    if(!pid || timeout_ticks>=(UINT64_C(1)<<63))return CAPIPC_INVALID;
    uint64_t flags=lock_ipc();
    uint64_t *waiter=NULL;
    bool ready=false;
    if(handle==CAPIPC_SERVER_WAIT_HANDLE){
        if(pid!=listening_server){unlock_ipc(flags);return CAPIPC_DENIED;}
        for(unsigned i=0;i<CAPIPC_MAX_ENDPOINTS;++i)
            if(ep[i].used && ep[i].pid==pid && !ep[i].accepted && !ep[i].peer_closed)ready=true;
        waiter=&accept_waiter;
    }else{
        int i=lookup(pid,handle);
        if(i<0){unlock_ipc(flags);return CAPIPC_DENIED;}
        ready=ep[i].count!=0 || ep[i].peer_closed;
        waiter=&ep[i].waiter;
    }
    if(ready){unlock_ipc(flags);return CAPIPC_OK;}
#ifndef OPENXP_CAPIPC_HOST_TEST
    uint64_t tid=scheduler_current_id();
    if(!tid || *waiter){unlock_ipc(flags);return CAPIPC_INVALID;}
    *waiter=tid;
    bool slept=timeout_ticks?scheduler_sleep_current_locked(timeout_ticks):scheduler_park_current_locked();
    /* Slot may have been recycled during sleep: never dereference the old
     * waiter pointer unless the handle still names the original endpoint. */
    if(handle==0) {
        if(listening_server==pid && accept_waiter==tid)accept_waiter=0;
    }else{
        int i=lookup(pid,handle);
        if(i>=0 && ep[i].waiter==tid)ep[i].waiter=0;
    }
    unlock_ipc(flags);
    return slept?CAPIPC_OK:CAPIPC_INVALID;
#else
    (void)waiter;
    unlock_ipc(flags);return CAPIPC_EMPTY;
#endif
}
