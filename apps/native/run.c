#include "oxp.h"
#include "ui_widgets.h"
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
    oxp_draw_cmd_t clear={OXP_DRAW_CLEAR,0,0,0,0,oxp_theme_panel(oxp_ui_theme)};
    oxp_draw(&clear,1);
    oxp_ui_toolbar((oxp_ui_rect){0,0,w,35});
    oxp_ui_text((oxp_ui_rect){14u,0u,w-25u,35u},"Run Program",13u,oxp_ui_ink(),0);
    oxp_ui_panel((oxp_ui_rect){10,40,w-20,43});
    oxp_ui_textbox((oxp_ui_rect){16,47,w-32,28},command,1);
    oxp_ui_button((oxp_ui_rect){run_button_x(),by,87,28},"RUN",OXP_UI_PRIMARY,0);
    if(h>135u){oxp_ui_text((oxp_ui_rect){15u,h-35u,w-30u,20u},status,h-33u,oxp_ui_ink(),0);
        oxp_label(15,h-15u,"EX: NOTEPAD.EXE",1,oxp_theme_ink(oxp_ui_theme));}
}
void oxp_main(void){
    if(oxp_open("Run - native ELF64"))oxp_exit(1);
    oxp_ui_atlas_init_widgets();
    for(;;){render();uint32_t e=oxp_ui_wait_theme();if(e==OXP_CLOSE||e==27)break;
        if(e==10 || (e&OXP_CLICK && oxp_hit(e,run_button_x(),run_button_y(),87,28)))execute();
        else if(e==8){if(n)command[--n]=0;}
        else if(e>=32 && e<127 && n+1<sizeof command){command[n++]=(char)e;command[n]=0;}
    }oxp_exit(0);
}
