#include "oxp.h"
/* Small Ring 3 terminal. File writes are confined by the kernel to HOME. */
static char input[55],lines[7][55];
static unsigned used,row,listing_offset;
static app_file_request_t req;
static int same(const char *a,const char *b){
    while(*a&&*b){char x=*a++,y=*b++;
        if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;
        if(x!=y)return 0;
    }return !*a&&!*b;
}
static int starts(const char *a,const char *b){
    while(*b){char x=*a++,y=*b++;
        if(x>='A'&&x<='Z')x+=32;if(x!=y)return 0;
    }return 1;
}
static void print(const char *s){oxp_copy(lines[row%7],s,sizeof lines[0]);++row;}
static void number(const char *label,uint64_t value){
    char b[55];size_t n=0;oxp_puts(b,&n,sizeof b,label);
    oxp_u64(b,&n,sizeof b,value);print(b);
}
static char *word(char *s){while(*s==' ')++s;return s;}
static void split(char *s,char **left,char **right){
    s=word(s);*left=s;while(*s && *s!=' ')++s;
    if(*s){*s++=0;*right=word(s);}else *right=s;
}
static void list(void){
    unsigned displayed=0;
    for(unsigned i=listing_offset;i<listing_offset+5;++i){
        req=(app_file_request_t){0};req.index=i;
        if(oxp_file(APP_FS_LIST,&req))break;
        print(req.path);++displayed;
    }
    if(displayed==5){listing_offset+=5;print("LS AGAIN FOR MORE FILES");}
    else {listing_offset=0;if(!displayed)print("END OF FILE LIST");}
}
static void read_file(const char *name){
    req=(app_file_request_t){0};oxp_copy(req.path,name,sizeof req.path);
    if(oxp_file(APP_FS_READ,&req)){print("NO FILE OR TOO LARGE");return;}
    char b[55];unsigned n=0;
    for(unsigned i=0;i<req.size;++i){
        char c=(char)req.data[i];
        if(c=='\n'||n==54){b[n]=0;print(b);n=0;if(c=='\n')continue;}
        if(c>=32&&c<127)b[n++]=c;
    }
    if(n){b[n]=0;print(b);}
}
static void execute(void){
    print(input);
    if(same(input,"help")){
        print("LS CAT COPY REN DEL WRITE TOUCH");
        print("RUN MEM UPTIME PWD ECHO CLEAR");
    }else if(same(input,"clear")||same(input,"cls")){
        row=0;for(unsigned i=0;i<7;++i)lines[i][0]=0;
    }else if(same(input,"ls")||same(input,"dir"))list();
    else if(same(input,"pwd"))print("BOOT + HOME (RAM)");
    else if(same(input,"mem"))number("FREE PAGES ",oxp_call(26,0,0));
    else if(same(input,"uptime"))number("UPTIME SEC ",oxp_ticks()/100);
    else if(starts(input,"echo "))print(input+5);
    else if(starts(input,"cat ")||starts(input,"type "))
        read_file(word(input+(starts(input,"cat ")?4:5)));
    else if(starts(input,"run ")){
        char path[48]="bin/";char *name=word(input+4);
        if(starts(name,"bin/"))oxp_copy(path,name,sizeof path);
        else {unsigned i=0;while(name[i] && i<sizeof(path)-5){
            char c=name[i];if(c>='A'&&c<='Z')c+=32;path[i+4]=c;++i;}
            path[i+4]=0;}
        print(oxp_launch(path)==0?"LAUNCH QUEUED":"UNKNOWN/BUSY");
    }else if(starts(input,"copy ")||starts(input,"ren ")){
        unsigned copy=starts(input,"copy ");
        char *a,*b;split(input+(copy?5:4),&a,&b);
        if(!*a||!*b){print("USAGE: COPY/REN SRC DEST");}
        else {req=(app_file_request_t){0};
            oxp_copy(req.path,a,sizeof req.path);oxp_copy(req.dest,b,sizeof req.dest);
            print(oxp_file(copy?APP_FS_COPY:APP_FS_RENAME,&req)==0?"OK":"FILE OP FAILED");}
        listing_offset=0;
    }else if(starts(input,"del ")||starts(input,"rm ")){
        req=(app_file_request_t){0};
        oxp_copy(req.path,word(input+(starts(input,"del ")?4:3)),sizeof req.path);
        print(oxp_file(APP_FS_DELETE,&req)==0?"DELETED":"DELETE FAILED");
        listing_offset=0;
    }else if(starts(input,"touch ")||starts(input,"write ")){
        unsigned touch=starts(input,"touch ");
        char *a,*b;split(input+(touch?6:6),&a,&b);
        if(!*a){print("USAGE: WRITE HOME/FILE TEXT");}
        else {req=(app_file_request_t){0};oxp_copy(req.path,a,sizeof req.path);
            req.size=touch?0u:(uint32_t)oxp_len(b);
            if(req.size>APP_FILE_DATA){print("TEXT TOO LONG");}
            else {for(unsigned i=0;i<req.size;++i)req.data[i]=(uint8_t)b[i];
                print(oxp_file(APP_FS_WRITE,&req)==0?"SAVED TO HOME":"WRITE FAILED");}}
        listing_offset=0;
    }else if(used)print("UNKNOWN COMMAND. TRY HELP");
    input[0]=0;used=0;
}
static void render(void){
    oxp_draw_cmd_t shapes[]={
        {OXP_DRAW_CLEAR,0,0,0,0,0x101d30},{OXP_DRAW_FILL,0,0,320,22,0x264c7d}};
    oxp_draw(shapes,2);oxp_label(9,7,"OPENXP TERMINAL - NATIVE",1,0xffffff);
    unsigned count=row<7?row:7;
    for(unsigned i=0;i<count;++i)oxp_label(9,30+i*17,lines[(row-count+i)%7],1,0xaddbb3);
    oxp_label(9,160,">",1,0x82e7b3);oxp_label(20,160,input,1,0xd6f4db);
}
void oxp_main(void){
    if(oxp_open("Terminal - native ELF64"))oxp_exit(1);
    print("TYPE HELP FOR COMMANDS");
    for(;;){render();uint32_t e=oxp_wait(0);
        if(e==OXP_CLOSE||e==27)break;
        if(e==10){execute();continue;}
        if(e==8){if(used)input[--used]=0;continue;}
        if(e>=32&&e<127&&used+1<sizeof input){input[used++]=(char)e;input[used]=0;}
    }oxp_exit(0);
}
