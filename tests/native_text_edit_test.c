#include "../apps/native/text_edit.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    char t[5]="abc";size_t len=3,cursor=1;
    assert(oxp_insert(t,&len,&cursor,sizeof t,'X'));
    assert(!strcmp(t,"aXbc")&&len==4&&cursor==2);
    assert(!oxp_insert(t,&len,&cursor,sizeof t,'Y'));
    assert(oxp_backspace(t,&len,&cursor));
    assert(!strcmp(t,"abc")&&cursor==1);
    assert(oxp_delete(t,&len,&cursor));
    assert(!strcmp(t,"ac")&&len==2);
    cursor=0;assert(!oxp_backspace(t,&len,&cursor));
    cursor=len;assert(!oxp_delete(t,&len,&cursor));
    assert(!oxp_insert(t,&len,&cursor,0,'X'));
    puts("PASS: native text insertion, removal, cursor and capacity");
    return 0;
}
