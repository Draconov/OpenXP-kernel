#include "oxp.h"
static char command[40];static unsigned n;
static const char *status="TYPE APP NAME AND ENTER";
static void execute(void){char path[48]="bin/";
    if(!n){status="ENTER PROGRAM NAME";return;}
    for(unsigned i=0;i<n&&i<sizeof path-5;++i){char c=command[i];
        if(c>='A'&&c<='Z')c+=32;
        if(!((c>='a'&&c<='z')||(c>='0'&&c<='9'))){status="INVALID PROGRAM";return;}
        path[i+4]=c;path[i+5]=0;
    }
    status=oxp_launch(path)==0?"LAUNCH REQUESTED":"UNKNOWN OR BUSY";
}
static void render(void){oxp_draw_cmd_t shapes[]={
    {OXP_DRAW_CLEAR,0,0,0,0,0xe8f2ff},{OXP_DRAW_FILL,12,45,295,38,0xffffff},
    {OXP_DRAW_FILL,218,104,87,28,0x3a91d5}};
    oxp_draw(shapes,3);oxp_label(14,10,"RUN PROGRAM",2,0x174679);
    oxp_label(20,59,command,1,0x143751);oxp_label(243,114,"RUN",1,0xffffff);
    oxp_label(15,144,status,1,0x366080);
    oxp_label(15,162,"EX: NOTEPAD, PAINT, MINES",1,0x366080);
}
void oxp_main(void){if(oxp_open("Run - native ELF64"))oxp_exit(1);
    for(;;){render();uint32_t e=oxp_wait(0);if(e==OXP_CLOSE||e==27)break;
        if(e==10 || (e&OXP_CLICK && oxp_hit(e,218,104,87,28)))execute();
        else if(e==8){if(n)command[--n]=0;}
        else if(e>=32 && e<127 && n+1<sizeof command){command[n++]=(char)e;command[n]=0;}
    }oxp_exit(0);
}
