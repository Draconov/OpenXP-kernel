#include "oxp.h"
/* Windows-like Run dialog: paths resolve to allowlisted OpenXP ELF64 apps.
 * '.exe' is an OS-visible alias, not Windows PE compatibility. */
static char command[72];static unsigned n;
static const char *status="TYPE APP OR C:\\ PATH";
static int starts(const char *s,const char *prefix){
    while(*prefix){char a=*s++,b=*prefix++;
        if(a>='A'&&a<='Z')a+=32;
        if(b>='A'&&b<='Z')b+=32;
        if(a!=b)return 0;
    }return 1;
}
static int ends_exe(const char *s){
    unsigned n=0;while(s[n])++n;
    return n>=4 && starts(s+n-4,".exe");
}
static void execute(void){
    char path[96]={0};
    if(!n){status="ENTER PROGRAM NAME";return;}
    const char *source=command;
    if(starts(source,"c:"))source+=2;
    while(*source=='\\' || *source=='/')++source;
    if(starts(source,"windows/")||starts(source,"windows\\")||
       starts(source,"system apps/")||starts(source,"system apps\\")||
       starts(source,"program files/")||starts(source,"program files\\")||
       starts(source,"bin/")||starts(source,"bin\\")){
        unsigned i=0;
        while(source[i]&&i+1<sizeof path){char c=source[i];
            if(c>='A'&&c<='Z')c+=32;
            path[i]=c=='\\'?'/':c;++i;
        }
        path[i]=0;
    }else{
        /* .exe invokes the system-app namespace, bare names the old BIN path. */
        oxp_copy(path,ends_exe(source)?"system apps/":"bin/",sizeof path);
        unsigned j=0;while(path[j])++j;
        unsigned i=0;while(source[i]&&j+1<sizeof path){char c=source[i++];
            if(c>='A'&&c<='Z')c+=32;
            path[j++]=c;
        }path[j]=0;
    }
    status=oxp_launch(path)==0?"LAUNCH REQUESTED":"UNKNOWN PROGRAM / BUSY";
}
static unsigned run_button_x(void){return oxp_canvas_width()-99u;}
static unsigned run_button_y(void){
    unsigned h=oxp_canvas_height();return h>=140u?104u:h>75u?h-35u:38u;
}
static void render(void){
    unsigned w=oxp_canvas_width(),h=oxp_canvas_height();
    unsigned by=run_button_y();
    oxp_draw_cmd_t shapes[]={
        {OXP_DRAW_CLEAR,0,0,0,0,0xe8f2ff},
        {OXP_DRAW_FILL,12,45,w-24,30,0xffffff},
        {OXP_DRAW_FILL,run_button_x(),by,87,28,0x3a91d5}};
    oxp_draw(shapes,3);oxp_label(14,10,"RUN PROGRAM",2,0x174679);
    oxp_label(20,57,command,1,0x143751);
    oxp_label(run_button_x()+24u,by+10u,"RUN",1,0xffffff);
    if(h>135u){oxp_label(15,h-33u,status,1,0x366080);
        oxp_label(15,h-15u,"EX: NOTEPAD.EXE",1,0x366080);}
}
void oxp_main(void){
    if(oxp_open("Run - native ELF64"))oxp_exit(1);
    for(;;){render();uint32_t e=oxp_wait(0);if(e==OXP_CLOSE||e==27)break;
        if(e==10 || (e&OXP_CLICK && oxp_hit(e,run_button_x(),run_button_y(),87,28)))execute();
        else if(e==8){if(n)command[--n]=0;}
        else if(e>=32 && e<127 && n+1<sizeof command){command[n++]=(char)e;command[n]=0;}
    }oxp_exit(0);
}
