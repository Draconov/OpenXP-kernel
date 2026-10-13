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
