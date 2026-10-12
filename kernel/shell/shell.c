#include "shell/shell.h"

static bool same(const char *a, const char *b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static bool prefix(const char *a, const char *b) {
    while (*b) if (*a++ != *b++) return false;
    return true;
}
static void out(shell_t *s, const char *str) { if (s->output) s->output(s->output_ctx, str); }
static void resolve(const shell_t *s,const char *src,char *out,size_t cap) {
    size_t n=0;
    if (*src=='/'||*src=='\\') ++src;
    else if (s->cwd[0] && src[0] != 'b' && src[0] != 'd') {
        for(size_t j=0;s->cwd[j] && n+1<cap;++j)out[n++]=s->cwd[j];
    }
    for(size_t j=0;src[j] && n+1<cap;++j) {
        char c=src[j];out[n++]=(c=='\\')?'/':c;
    }
    out[n]=0;
}
static void decimal(uint64_t n, char *buffer) {
    char reverse[21]; size_t count=0, i=0;
    do { reverse[count++] = (char)('0' + n%10); n/=10; } while(n);
    while (count) buffer[i++] = reverse[--count];
    buffer[i]=0;
}
void shell_init(shell_t *s, shell_output_fn output, void *ctx, shell_services_t services) {
    *s = (shell_t){0};
    s->output=output; s->output_ctx=ctx; s->services=services; s->visible=true;
    s->cwd[0]=0;
    out(s, "OpenXP Command Interpreter 1.8.0");
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
    lower[c]=0;
    if (same(lower,"help")) {
        out(s,"HELP VER CLS ECHO DIR CD TYPE MEM UPTIME PS RUN EXIT");
        out(s,"RUN BIN/HELLO executes an initrd application.");
    } else if (same(lower,"ver")) {
        out(s,"OpenXP 1.8.0 - experimental x86-64 kernel");
    } else if (same(lower,"cls")) {
        out(s,"\f"); /* GUI interprets as clear scrollback */
    } else if (prefix(lower,"echo ")) {
        out(s,cmd+5);
    } else if (same(lower,"dir")) {
        size_t count=s->services.file_count ? s->services.file_count(s->services.ctx):0;
        out(s,"\nREAD-ONLY INITRD:");
        for (size_t i=0;i<count;++i) {
            char name[64];
            if (s->services.file_name && s->services.file_name(s->services.ctx,i,name,sizeof(name))) {
                if(!s->cwd[0] || prefix(name,s->cwd))out(s,name);
            }
        }
        if (!count) out(s,"(no files mounted)");
    } else if (same(lower,"ps")) {
        out(s,"PID 0  KERNEL / DESKTOP");
        out(s,"Process list is experimental (no background user apps)");
    } else if (same(lower,"mem")) {
        char num[24]; decimal(s->services.free_pages?s->services.free_pages(s->services.ctx):0,num);
        out(s,"FREE 4K PAGES:"); out(s,num);
    } else if (same(lower,"uptime")) {
        char num[24]; decimal(s->services.ticks?s->services.ticks(s->services.ctx):0,num);
        out(s,"PIT TICKS:"); out(s,num);
    } else if (same(lower,"exit")) {
        s->visible=false; out(s,"Terminal minimized; click taskbar to reopen.");
    } else if (prefix(lower,"run ")) {
        char resolved[96];resolve(s,lower+4,resolved,sizeof(resolved));
        const char *path=resolved;
        if (!*path || !s->services.run || !s->services.run(s->services.ctx,path)) out(s,"Could not run executable (see serial console).");
        else out(s,"Application finished successfully.");
    } else if (prefix(lower,"cd ") || same(lower,"cd")) {
        const char *dest=same(lower,"cd")?"":lower+3;
        if(same(dest,"..") || same(dest,"/") || same(dest,"\\")) {
            s->cwd[0]=0;out(s,"ROOT\\");
        } else if(same(dest,"bin")||same(dest,"bin/")||same(dest,"/bin")) {
            s->cwd[0]='b';s->cwd[1]='i';s->cwd[2]='n';s->cwd[3]='/';s->cwd[4]=0;
            out(s,"C:\\BIN");
        } else if(same(dest,"docs")||same(dest,"docs/")||same(dest,"/docs")) {
            s->cwd[0]='d';s->cwd[1]='o';s->cwd[2]='c';s->cwd[3]='s';
            s->cwd[4]='/';s->cwd[5]=0;out(s,"C:\\DOCS");
        } else out(s,"Directory not found.");
    } else if (prefix(lower,"type ")) {
        char resolved[96],buffer[512];
        resolve(s,lower+5,resolved,sizeof(resolved));
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
        out(s,s->input); /* history includes the command */
        shell_execute(s,s->input);
        s->used=0; s->input[0]=0;
        return true;
    }
    if (c=='\b' || c==127) {
        if (s->used) { --s->used; s->input[s->used]=0; return true; }
        return false;
    }
    if (c<32 || c>126 || s->used+1>=sizeof(s->input)) return false;
    s->input[s->used++]=c;
    s->input[s->used]=0;
    return true;
}
