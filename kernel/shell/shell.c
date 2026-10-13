#include "shell/shell.h"

static bool same(const char *a, const char *b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static bool prefix(const char *a, const char *b) {
    while (*b) if (*a++ != *b++) return false;
    return true;
}
static void copy(char *dest,const char *src,size_t cap) {
    if(!cap)return;
    size_t i=0;
    while(src[i] && i+1<cap){dest[i]=src[i];++i;}
    dest[i]=0;
}
static void out(shell_t *s, const char *str) { if (s->output) s->output(s->output_ctx, str); }
static void resolve(const shell_t *s,const char *src,char *dest,size_t cap) {
    size_t n=0;
    if (*src=='/'||*src=='\\') ++src;
    else if(s->cwd[0] && !prefix(src,"bin/") && !prefix(src,"docs/") && !prefix(src,"home/")) {
        for(size_t j=0;s->cwd[j] && n+1<cap;++j)dest[n++]=s->cwd[j];
    }
    for(size_t j=0;src[j] && n+1<cap;++j) {
        char c=src[j];dest[n++]=(c=='\\')?'/':c;
    }
    dest[n]=0;
}
static void decimal(uint64_t n, char *buffer) {
    char reverse[21]; size_t count=0, i=0;
    do { reverse[count++] = (char)('0' + n%10); n/=10; } while(n);
    while (count) buffer[i++] = reverse[--count];
    buffer[i]=0;
}
static void listing(shell_t *s) {
    size_t count=s->services.file_count ? s->services.file_count(s->services.ctx):0;
    out(s,"\nBOOT FILES + TEMPORARY RAM FILES:");
    unsigned listed=0;
    for (size_t i=0;i<count;++i) {
        char name[64];
        if (s->services.file_name && s->services.file_name(s->services.ctx,i,name,sizeof(name))) {
            if(!s->cwd[0] || prefix(name,s->cwd)) {out(s,name);++listed;}
        }
    }
    if (!listed) out(s,"(no files in this folder)");
}
static void tree(shell_t *s) {
    out(s,"C:\\ (read-only boot files + writable RAM files)");
    out(s,"  BIN\\");
    out(s,"  DOCS\\");
    out(s,"  HOME\\ (temporary)");
    out(s,"Use DIR to see available files.");
}
static bool parse_number(const char **text,uint64_t *value) {
    const char *p=*text;
    while(*p==' ')++p;
    if(*p<'0'||*p>'9')return false;
    uint64_t n=0;
    while(*p>='0' && *p<='9') {
        /* Input is capped to 1e9 to avoid arithmetic overflow in CALC. */
        if(n>100000000u)return false;
        n=n*10u+(uint64_t)(*p-'0');
        if(n>1000000000u)return false;
        ++p;
    }
    *value=n;*text=p;return true;
}
static void calculator(shell_t *s,const char *expr) {
    uint64_t a,b,value;char sign=0;
    if(!parse_number(&expr,&a))goto invalid;
    while(*expr==' ')++expr;
    sign=*expr++;
    if(sign!='+' && sign!='-' && sign!='*' && sign!='/')goto invalid;
    if(!parse_number(&expr,&b))goto invalid;
    while(*expr==' ')++expr;
    if(*expr)goto invalid;
    if(sign=='/' && !b) {out(s,"CALC: division by zero");return;}
    bool negative=sign=='-'&&b>a;
    switch(sign) {
    case '+':value=a+b;break;
    case '-':value=negative?b-a:a-b;break;
    case '*':value=a*b;break;
    default:value=a/b;break;
    }
    char number[24],buffer[25];decimal(value,number);
    size_t i=0,j=0;
    if(negative)buffer[i++]='-';
    while(number[j] && i+1<sizeof(buffer))buffer[i++]=number[j++];
    buffer[i]=0;
    out(s,buffer);return;
invalid:
    out(s,"Usage: CALC 12 + 34   (operators: + - * /)");
}
/* Two whitespace-delimited file operands. No quoting or wildcard expansion.
 * Invalid or extra operands cause a clean failure, never truncated paths. */
static bool file_pair(const shell_t *s,const char *args,char *from,char *to,size_t cap){
    char first[96],second[96];size_t a=0,b=0;
    while(*args==' ')++args;
    while(*args && *args!=' '){if(a+1>=sizeof(first))return false;first[a++]=*args++;}
    first[a]=0;
    while(*args==' ')++args;
    while(*args && *args!=' '){if(b+1>=sizeof(second))return false;second[b++]=*args++;}
    second[b]=0;
    while(*args==' ')++args;
    if(!a||!b||*args)return false;
    resolve(s,first,from,cap);resolve(s,second,to,cap);
    return *from&&*to;
}
static void history_store(shell_t *s,const char *cmd) {
    if(!*cmd)return;
    copy(s->history[s->history_next],cmd,sizeof(s->history[0]));
    s->history_next=(s->history_next+1)%8;
    if(s->history_count<8)++s->history_count;
}
void shell_init(shell_t *s, shell_output_fn output, void *ctx, shell_services_t services) {
    *s = (shell_t){0};
    s->output=output; s->output_ctx=ctx; s->services=services; s->visible=true;
    s->history_index=-1;
    out(s, "OpenXP Command Interpreter");
    out(s, "Type HELP for available commands.");
}
void shell_execute(shell_t *s, const char *cmd) {
    if (!s || !cmd) return;
    while (*cmd == ' ') ++cmd;
    if (!*cmd) return;
    char lower[96];size_t c=0;
    while(cmd[c] && c+1<sizeof(lower)){
        char ch=cmd[c];lower[c]=(ch>='A'&&ch<='Z')?(char)(ch+32):ch;++c;
    }
    while(c && lower[c-1]==' ')--c;
    lower[c]=0;
    if (same(lower,"help") || prefix(lower,"help ")) {
        if(prefix(lower,"help ")) {
            out(s,"Commands: HELP VER CLS ECHO DIR CD TYPE RUN MEM UPTIME");
            out(s,"          PS PWD TREE HISTORY CALC WHOAMI HOSTNAME");
            out(s,"          UNAME SYSINFO APPS COPY REN DEL EXIT");
        } else {
            out(s,"HELP VER CLS ECHO DIR CD TYPE MEM UPTIME PS RUN EXIT");
            out(s,"Also: LS CAT CLEAR PWD TREE HISTORY CALC WHOAMI");
            out(s,"      HOSTNAME UNAME SYSINFO APPS COPY REN DEL");
            out(s,"Try CALC 8 * 7, TREE, or RUN BIN/HELLO.");
        }
    } else if (same(lower,"ver") || same(lower,"uname")) {
        out(s,"OpenXP - experimental x86-64 kernel");
    } else if (same(lower,"cls") || same(lower,"clear")) {
        out(s,"\f");
    } else if (same(lower,"echo")) {
        out(s,"");
    } else if (prefix(lower,"echo ")) {
        out(s,cmd+5);
    } else if (same(lower,"dir") || same(lower,"ls")) {
        listing(s);
    } else if (same(lower,"tree")) {
        tree(s);
    } else if (same(lower,"ps")) {
        out(s,"PID 0  RUNNING  KERNEL / DESKTOP");
        if (!s->services.process_entry) {
            out(s,"Process listing not available.");
        } else {
            static const char *const states[]={"EMPTY","READY","RUNNING","EXITED","FAULTED"};
            unsigned listed=0;
            for(size_t slot=0;slot<16;++slot){
                uint64_t pid=0;unsigned state=0;
                if(!s->services.process_entry(s->services.ctx,slot,&pid,&state))continue;
                char line[66],digits[24];decimal(pid,digits);
                copy(line,"PID ",sizeof(line));
                size_t pos=4,i=0;
                while(digits[i]&&pos+1<sizeof(line))line[pos++]=digits[i++];
                line[pos++]=' ';line[pos++]=' ';
                const char *name=state<5?states[state]:"UNKNOWN";
                i=0;while(name[i]&&pos+1<sizeof(line))line[pos++]=name[i++];
                line[pos]=0;out(s,line);++listed;
            }
            if(!listed)out(s,"No independent user processes.");
        }
    } else if (same(lower,"mem") || same(lower,"free")) {
        char num[24]; decimal(s->services.free_pages?s->services.free_pages(s->services.ctx):0,num);
        out(s,"FREE 4K PAGES:"); out(s,num);
    } else if (same(lower,"uptime")) {
        char num[24]; decimal(s->services.ticks?s->services.ticks(s->services.ctx):0,num);
        out(s,"PIT TICKS:"); out(s,num);
    } else if (same(lower,"pwd")) {
        if(s->cwd[0]=='b')out(s,"C:\\BIN");
        else if(s->cwd[0]=='d')out(s,"C:\\DOCS");
        else if(s->cwd[0]=='h')out(s,"C:\\HOME");
        else out(s,"C:\\");
    } else if (same(lower,"whoami")) {
        out(s,"SYSTEM (kernel-hosted desktop)");
    } else if (same(lower,"hostname")) {
        out(s,"OPENXP-PC");
    } else if (same(lower,"sysinfo")) {
        out(s,"OpenXP | x86-64 | freestanding kernel");
        out(s,"Graphics: software RGB framebuffer");
        out(s,"Storage: read-only boot files + temporary RAM files");
        out(s,"RAM files disappear on reboot; no disk writes");
        out(s,"Input: legacy PS/2 keyboard and mouse");
    } else if (same(lower,"apps")) {
        out(s,"Built-in: Terminal, My Computer, About OpenXP");
        out(s,"ELF programs: DIR, then RUN <path>");
        out(s,"Calculator: CALC <number> <operator> <number>");
    } else if (same(lower,"history")) {
        for(unsigned i=0;i<s->history_count;++i) {
            unsigned slot=(s->history_next+8-s->history_count+i)%8;
            out(s,s->history[slot]);
        }
        if(!s->history_count)out(s,"(no commands yet)");
    } else if (same(lower,"exit")) {
        s->visible=false; out(s,"Terminal minimized; click taskbar to reopen.");
    } else if (prefix(lower,"calc ")) {
        calculator(s,lower+5);
    } else if (same(lower,"calc")) {
        out(s,"Usage: CALC 12 + 34   (operators: + - * /)");
    } else if (prefix(lower,"run ")) {
        char resolved[96];resolve(s,lower+4,resolved,sizeof(resolved));
        if (!*resolved || !s->services.run || !s->services.run(s->services.ctx,resolved))
            out(s,"Could not run executable (see serial console).");
        else out(s,"Launch requested; use PS to inspect processes.");
    } else if(prefix(lower,"copy ") || prefix(lower,"ren ")){
        bool rename=prefix(lower,"ren ");
        char from[96],to[96];
        if(!file_pair(s,lower+(rename?4:5),from,to,sizeof(from))) {
            out(s,"Usage: COPY source target  or  REN old new");
        }else{
            bool ok=rename?(s->services.file_rename&&
                s->services.file_rename(s->services.ctx,from,to)):
                (s->services.file_copy&&s->services.file_copy(s->services.ctx,from,to));
            out(s,ok?(rename?"File renamed.":"File copied."):
                  "Operation failed (RAM HOME files only; no overwrite).");
        }
    } else if(prefix(lower,"del ")){
        char path[96];resolve(s,lower+4,path,sizeof(path));
        out(s,s->services.file_remove&&s->services.file_remove(s->services.ctx,path)?
              "File deleted.":"Cannot delete (RAM HOME files only).");
    } else if (prefix(lower,"cd ") || same(lower,"cd")) {
        const char *dest=same(lower,"cd")?"":lower+3;
        if(!*dest) {
            shell_execute(s,"pwd");
        } else if(same(dest,"..") || same(dest,"/") || same(dest,"\\") || same(dest,"c:\\")) {
            s->cwd[0]=0;out(s,"ROOT\\");
        } else if(same(dest,"bin")||same(dest,"bin/")||same(dest,"/bin")) {
            copy(s->cwd,"bin/",sizeof(s->cwd));out(s,"C:\\BIN");
        } else if(same(dest,"docs")||same(dest,"docs/")||same(dest,"/docs")) {
            copy(s->cwd,"docs/",sizeof(s->cwd));out(s,"C:\\DOCS");
        } else if(same(dest,"home")||same(dest,"home/")||same(dest,"/home")) {
            copy(s->cwd,"home/",sizeof(s->cwd));out(s,"C:\\HOME (TEMP)");
        } else out(s,"Directory not found.");
    } else if (prefix(lower,"type ") || prefix(lower,"cat ")) {
        char resolved[96],buffer[512];
        resolve(s,lower+(lower[0]=='t'?5:4),resolved,sizeof(resolved));
        size_t n=s->services.read_text ?
            s->services.read_text(s->services.ctx,resolved,buffer,sizeof(buffer)):0;
        if(!n)out(s,"Not a readable text file.");
        else {
            size_t at=0;
            while(at<n){
                char line[90];size_t used=0;
                while(at<n && buffer[at]!='\n' && used+1<sizeof(line))line[used++]=buffer[at++];
                if(at<n && buffer[at]=='\n')++at;
                line[used]=0;out(s,line);
            }
        }
    } else {
        out(s,"Unrecognized command. Type HELP.");
    }
}
bool shell_key(shell_t *s,char c) {
    if (!s) return false;
    if (c=='\r' || c=='\n') {
        s->input[s->used]=0;
        out(s,s->input);
        history_store(s,s->input);
        shell_execute(s,s->input);
        s->used=0;s->cursor=0;s->input[0]=0;
        s->history_index=-1;
        return true;
    }
    if (c=='\b' || c==127) {
        if(!s->cursor)return false;
        for(size_t i=s->cursor-1;i<s->used;++i)s->input[i]=s->input[i+1];
        --s->used;--s->cursor;return true;
    }
    if (c<32 || c>126 || s->used+1>=sizeof(s->input)) return false;
    for(size_t i=s->used+1;i>s->cursor;--i)s->input[i]=s->input[i-1];
    s->input[s->cursor++]=c;
    ++s->used;
    return true;
}
bool shell_history(shell_t *s,int direction) {
    if(!s || !s->history_count || (direction!=-1 && direction!=1))return false;
    if(direction<0) {
        if(s->history_index==-1)copy(s->draft,s->input,sizeof(s->draft));
        if((unsigned)(s->history_index+1)<s->history_count)++s->history_index;
        else return false;
        unsigned slot=(s->history_next+8-1-(unsigned)s->history_index)%8;
        copy(s->input,s->history[slot],sizeof(s->input));
    } else {
        if(s->history_index<0)return false;
        --s->history_index;
        if(s->history_index<0)copy(s->input,s->draft,sizeof(s->input));
        else {
            unsigned slot=(s->history_next+8-1-(unsigned)s->history_index)%8;
            copy(s->input,s->history[slot],sizeof(s->input));
        }
    }
    s->used=0;while(s->input[s->used])++s->used;
    s->cursor=s->used;return true;
}
bool shell_left(shell_t *s){if(!s||!s->cursor)return false;--s->cursor;return true;}
bool shell_right(shell_t *s){if(!s||s->cursor>=s->used)return false;++s->cursor;return true;}
bool shell_home(shell_t *s){if(!s||!s->cursor)return false;s->cursor=0;return true;}
bool shell_end(shell_t *s){if(!s||s->cursor==s->used)return false;s->cursor=s->used;return true;}
bool shell_delete(shell_t *s){
    if(!s||s->cursor>=s->used)return false;
    for(size_t i=s->cursor;i<s->used;++i)s->input[i]=s->input[i+1];
    --s->used;return true;
}
