#include "shell/shell.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char lines[128][128]; static int count;
static void output(void *p,const char *line) { (void)p; assert(count < 128); snprintf(lines[count++],128,"%s",line); }
static uint64_t ticks(void *p) { (void)p; return 123; }
static uint64_t freepages(void *p) { (void)p; return 48; }
static size_t filecount(void *p) { (void)p; return 1; }
static bool filename(void *p,size_t idx,char *dst,size_t cap) { (void)p; assert(idx==0); assert(cap>10); strcpy(dst,"bin/hello"); return true; }
static size_t readtext(void *p,const char *path,char *dst,size_t cap) {
    (void)p;(void)path;assert(cap>6);strcpy(dst,"hello\n");return 6;
}
static bool run(void *p,const char *path) { (void)p; return strcmp(path,"bin/hello")==0; }
int main(void) {
    shell_t shell; shell_services_t svc={ticks,freepages,filecount,filename,run,readtext,NULL};
    shell_init(&shell,output,NULL,svc); assert(count==2);
    shell_execute(&shell,"dir"); assert(!strcmp(lines[3],"bin/hello"));
    shell_execute(&shell,"mem"); assert(!strcmp(lines[5],"48"));
    shell_execute(&shell,"uptime"); assert(!strcmp(lines[7],"123"));
    shell_execute(&shell,"run bin/hello"); assert(strstr(lines[8],"successfully"));
    shell_execute(&shell,"run missing"); assert(strstr(lines[9],"Could not"));
    assert(shell_key(&shell,'A') && shell_key(&shell,'B'));
    assert(shell.used==2 && !strcmp(shell.input,"AB"));
    assert(shell_key(&shell,'\b') && shell.used==1);
    assert(shell_key(&shell,'\n') && shell.used==0);
    shell_execute(&shell,"type docs/readme.txt");
    assert(!strcmp(lines[count-1],"hello"));
    shell_execute(&shell,"cd bin");assert(!strcmp(shell.cwd,"bin/"));
    shell_execute(&shell,"cd ..");assert(!shell.cwd[0]);
    shell_execute(&shell,"exit"); assert(!shell.visible);
    shell_execute(&shell,"pwd");assert(!strcmp(lines[count-1],"C:\\"));
    shell_execute(&shell,"ls");assert(!strcmp(lines[count-1],"bin/hello"));
    shell_execute(&shell,"calc 8 * 7");assert(!strcmp(lines[count-1],"56"));
    shell_execute(&shell,"calc 1 - 3");assert(!strcmp(lines[count-1],"-2"));
    shell_execute(&shell,"calc 2 / 0");assert(strstr(lines[count-1],"zero"));
    shell_execute(&shell,"calc 2000000000 * 2");assert(strstr(lines[count-1],"Usage"));
    shell_execute(&shell,"sysinfo");assert(strstr(lines[count-1],"PS/2"));
    shell_execute(&shell,"apps");assert(strstr(lines[count-1],"Calculator"));
    shell_execute(&shell,"tree");assert(strstr(lines[count-1],"DIR"));
    shell.visible=true;
    assert(shell_key(&shell,'h')&&shell_key(&shell,'e')&&shell_key(&shell,'l'));
    assert(shell.cursor==3 && shell.used==3);
    assert(shell_home(&shell) && shell.cursor==0);
    assert(shell_delete(&shell) && !strcmp(shell.input,"el"));
    assert(shell_key(&shell,'H') && !strcmp(shell.input,"Hel"));
    assert(shell_end(&shell) && shell.cursor==3);
    assert(shell_left(&shell) && shell.cursor==2);
    assert(shell_right(&shell) && shell.cursor==3);
    assert(shell_key(&shell,'p') && !strcmp(shell.input,"Help"));
    assert(shell_key(&shell,'\n'));
    assert(shell_history(&shell,-1) && !strcmp(shell.input,"Help"));
    assert(shell_history(&shell,1) && !shell.input[0]);
    puts("PASS: shell commands, arithmetic, history and line editing");
}
