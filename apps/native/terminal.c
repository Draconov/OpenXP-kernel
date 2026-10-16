#include "oxp.h"
#include "ui_widgets.h"
/* Small Ring 3 terminal. File writes are confined by the kernel to HOME. */
static char input[55],lines[7][55];
static unsigned used,row,listing_offset;
static char cwd[96];
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
static int absolute(const char *p){
    return starts(p,"home/")||starts(p,"bin/")||starts(p,"docs/")||
        starts(p,"windows/")||starts(p,"program files/")||starts(p,"system apps/")||
        starts(p,"my documents/")||starts(p,"documents and settings/");
}
static void resolve(const char *from,char *out,unsigned cap){
    unsigned n=0;out[0]=0;
    if(starts(from,"c:"))from+=2;
    if(*from=='/' || *from=='\\')++from;
    else if(!absolute(from))for(unsigned i=0;cwd[i]&&n+1<cap;++i)out[n++]=cwd[i];
    while(*from && n+1<cap){char c=*from++;
        if(c>='A'&&c<='Z')c+=32;
        out[n++]=c=='\\'?'/':c;
    }out[n]=0;
}
static void cd(const char *dest){
    if(same(dest,"..")){
        unsigned n=0;while(cwd[n])++n;
        if(n&&cwd[n-1]=='/')--n;
        while(n&&cwd[n-1]!='/')--n;
        cwd[n]=0;listing_offset=0;print("PARENT FOLDER");return;
    }
    if(same(dest,"\\")||same(dest,"/")||same(dest,"c:\\")){
        cwd[0]=0;listing_offset=0;print("C:\\");return;
    }
    char path[96];resolve(dest,path,sizeof path);
    req=(app_file_request_t){0};oxp_copy(req.path,path,sizeof req.path);
    if(oxp_file(APP_FS_KIND,&req)!=3){print("FOLDER NOT FOUND");return;}
    unsigned n=0;while(path[n]&&n+2<sizeof cwd){cwd[n]=path[n];++n;}
    if(path[n]){print("PATH TOO LONG");return;}
    cwd[n++]='/';cwd[n]=0;listing_offset=0;print("DIRECTORY CHANGED");
}
static void list(void){
    unsigned displayed=0;
    for(unsigned i=listing_offset;i<listing_offset+5;++i){
        req=(app_file_request_t){0};req.index=i;req.dest[0]='D';
        oxp_copy(req.path,cwd,sizeof req.path);
        unsigned n=0;while(req.path[n])++n;
        if(n && req.path[n-1]=='/')req.path[n-1]=0;
        if(oxp_file(APP_FS_LIST,&req))break;
        print(req.path);++displayed;
    }
    if(displayed==5){listing_offset+=5;print("LS AGAIN FOR MORE FILES");}
    else {listing_offset=0;if(!displayed)print("END OF FILE LIST");}
}
static void read_file(const char *name){
    req=(app_file_request_t){0};resolve(name,req.path,sizeof req.path);
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
        print("CD DIR LS CAT COPY REN DEL WRITE TOUCH");
        print("MKDIR RMDIR STAT RUN MEM UPTIME PWD");
    }else if(same(input,"clear")||same(input,"cls")){
        row=0;for(unsigned i=0;i<7;++i)lines[i][0]=0;
    }else if(same(input,"ls")||same(input,"dir"))list();
    else if(same(input,"pwd")){
        char here[55]="C:\\";unsigned n=3;
        for(unsigned i=0;cwd[i]&&n+1<sizeof here;++i)
            here[n++]=cwd[i]=='/'?'\\':cwd[i];
        here[n]=0;print(here);
    }
    else if(starts(input,"cd "))cd(word(input+3));
    else if(starts(input,"mkdir ")||starts(input,"rmdir ")){
        unsigned creating=starts(input,"mkdir ");
        char path[96];resolve(word(input+(creating?6:6)),path,sizeof path);
        req=(app_file_request_t){0};oxp_copy(req.path,path,sizeof req.path);
        print(oxp_file(creating?APP_FS_MKDIR:APP_FS_RMDIR,&req)==0?
              (creating?"FOLDER CREATED":"EMPTY FOLDER REMOVED"):
              "FOLDER OP FAILED / NOT EMPTY");
        listing_offset=0;
    }else if(starts(input,"stat ")){
        char path[96];resolve(word(input+5),path,sizeof path);
        req=(app_file_request_t){0};oxp_copy(req.path,path,sizeof req.path);
        if(oxp_file(APP_FS_STAT,&req))print("FILE NOT FOUND");
        else {
            print(req.index==3u?"DIRECTORY":req.index==2u?"WRITABLE FILE":"READ ONLY FILE");
            number("SIZE BYTES ",req.size);
        }
    }
    else if(same(input,"mem"))number("FREE PAGES ",oxp_call(26,0,0));
    else if(same(input,"uptime"))number("UPTIME SEC ",oxp_ticks()/100);
    else if(starts(input,"echo "))print(input+5);
    else if(starts(input,"cat ")||starts(input,"type "))
        read_file(word(input+(starts(input,"cat ")?4:5)));
    else if(starts(input,"run ")){
        char path[96];char *name=word(input+4);
        if(absolute(name)||starts(name,"c:")||starts(name,"/")){
            resolve(name,path,sizeof path);
        }else{
            unsigned n=0;while(name[n])++n;
            if(n>4 && same(name+n-4,".exe")){
                oxp_copy(path,"system apps/",sizeof path);
                unsigned j=12,i=0;
                while(name[i]&&j+1<sizeof path){path[j++]=name[i++];}
                path[j]=0;
            }else{
                oxp_copy(path,"bin/",sizeof path);
                unsigned j=4,i=0;
                while(name[i]&&j+1<sizeof path){path[j++]=name[i++];}
                path[j]=0;
            }
        }
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
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    oxp_draw_cmd_t shapes[]={
        {OXP_DRAW_CLEAR,0,0,0,0,0x101d30},{OXP_DRAW_FILL,0,0,w,22,0x264c7d}};
    oxp_draw(shapes,2);oxp_ui_appbar(w,"TERMINAL");
    unsigned prompt=h>18u?h-18u:0u;
    unsigned rows=prompt>30u?(prompt-30u)/17u:0u;
    if(rows>7u)rows=7u;
    unsigned count=row<rows?row:rows;
    for(unsigned i=0;i<count;++i)oxp_label(9,30+i*17,lines[(row-count+i)%7],1,0xaddbb3);
    if(h>15u){oxp_fill(0,prompt>5u?prompt-5u:0u,w,1u,oxp_theme_accent(oxp_ui_theme));
        oxp_label(9,prompt,">",1,0x82e7b3);oxp_label(20,prompt,input,1,0xd6f4db);}
}
void oxp_main(void){
    if(oxp_open("Terminal - native ELF64"))oxp_exit(1);
    oxp_ui_atlas_init_widgets();
    print("TYPE HELP FOR COMMANDS");
    for(;;){render();uint32_t e=oxp_ui_wait_theme();
        if(e==OXP_CLOSE||e==27)break;
        if(e==10){execute();continue;}
        if(e==8){if(used)input[--used]=0;continue;}
        if(e>=32&&e<127&&used+1<sizeof input){input[used++]=(char)e;input[used]=0;}
    }oxp_exit(0);
}
