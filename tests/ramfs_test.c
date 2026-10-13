#include "fs/ramfs.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
int main(void){
    ramfs_reset();assert(ramfs_count()==0);
    assert(!ramfs_write("../escape","x",1));
    assert(!ramfs_write("home/../escape","x",1));
    assert(!ramfs_write("home//escape","x",1));
    assert(!ramfs_write("home/folder/.","x",1));
    assert(!ramfs_write("home/foo\\bar","x",1));
    assert(!ramfs_write("home/no",NULL,1));
    assert(ramfs_write("home/note.txt","HELLO",5));
    char name[48],text[16];size_t n=99;
    assert(ramfs_count()==1 && ramfs_name_at(0,name,sizeof(name)));
    assert(strcmp(name,"home/note.txt")==0);
    assert(!ramfs_name_at(1,name,sizeof(name)));
    assert(!ramfs_read("home/note.txt",text,4,&n));
    assert(ramfs_read("home/note.txt",text,sizeof(text),&n));
    assert(n==5 && !memcmp(text,"HELLO",5));
    assert(!ramfs_write("home/note.txt",NULL,1));
    assert(ramfs_read("home/note.txt",text,sizeof(text),&n)&&n==5);
    assert(ramfs_write("home/note.txt","HEY",3));
    assert(ramfs_read("home/note.txt",text,sizeof(text),&n)&&n==3);
    assert(!memcmp(text,"HEY",3) && ramfs_count()==1);
    assert(ramfs_copy("home/note.txt","home/copy.txt"));
    assert(!ramfs_copy("home/note.txt","home/copy.txt"));
    assert(!ramfs_rename("home/note.txt","home/copy.txt"));
    assert(!ramfs_rename("home/note.txt","../unsafe"));
    assert(ramfs_rename("home/copy.txt","home/moved.txt"));
    assert(!ramfs_read("home/copy.txt",text,sizeof(text),&n));
    assert(ramfs_read("home/moved.txt",text,sizeof(text),&n)&&n==3);
    assert(!memcmp(text,"HEY",3));
    assert(ramfs_remove("home/moved.txt"));
    assert(ramfs_write("home/empty",NULL,0));
    assert(ramfs_read("home/empty",NULL,0,&n)&&n==0);
    uint8_t canvas[16000],copy[16000];
    for(size_t i=0;i<sizeof(canvas);++i)canvas[i]=(uint8_t)(i%8);
    assert(ramfs_write("home/painting.oxp",canvas,sizeof(canvas)));
    assert(ramfs_read("home/painting.oxp",copy,sizeof(copy),&n));
    assert(n==sizeof(canvas) && !memcmp(canvas,copy,n));
    assert(!ramfs_write("home/large",canvas,RAMFS_FILE_CAPACITY+1));
    assert(ramfs_count()==3);
    for(int i=0;i<12;++i){
        char path[]="home/item-0";path[10]=(char)('a'+i);
        if(i<9)assert(ramfs_write(path,"!",1));
        else assert(!ramfs_write(path,"!",1));
    }
    assert(ramfs_count()==RAMFS_MAX_FILES);
    assert(!ramfs_write("home/full","?",1));
    assert(ramfs_write("home/note.txt","replaced",8));
    assert(ramfs_count()==RAMFS_MAX_FILES);
    assert(ramfs_remove("home/note.txt"));
    assert(!ramfs_remove("home/note.txt"));
    assert(ramfs_write("home/freed","z",1));
    assert(ramfs_count()==RAMFS_MAX_FILES);
    ramfs_reset();assert(ramfs_count()==0);
    assert(!ramfs_read("home/painting.oxp",copy,sizeof(copy),&n));
    return 0;
}
