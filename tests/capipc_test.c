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
static void test_client_quota(void) {
    capipc_init();assert(capipc_grant_server(7));assert(capipc_listen(7)==CAPIPC_OK);
    uint64_t a=0,b=0,c=0;
    assert(capipc_connect(8,&a)==CAPIPC_OK);
    assert(capipc_connect(8,&b)==CAPIPC_OK);
    assert(capipc_connect(8,&c)==CAPIPC_NOSPACE);
    assert(capipc_close(8,a)==CAPIPC_OK);
    assert(capipc_connect(8,&c)==CAPIPC_OK);
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
int main(void){test_exchange();test_reuse_and_limits();test_overflow_and_exit();test_client_quota();puts("capipc_test: passed");return 0;}
