/* Host-only renderer fixture, not a screenshot of a running QEMU guest. */
#include "gui/desktop.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static uint32_t fb[1024*768];
size_t initrd_count(void){return 3;}
bool initrd_name_at(size_t n,char *dst,size_t cap) {
    const char *names[]={"bin/fault","bin/hello","docs/readme.txt"};
    if(n>=3||cap<17)return false;
    size_t i=0;while(names[n][i]){dst[i]=names[n][i];++i;}dst[i]=0;return true;
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    fb_surface_t surface={(volatile uint8_t*)fb,1024,768,4096,8,8,8,16,8,0};
    static desktop_t desktop;
    desktop_init(&desktop,&surface,(shell_services_t){0});
    desktop_tick(&desktop,67230);
    desktop_draw(&desktop);
    FILE *file=fopen(argv[1],"wb");if(!file)return 3;
    fputs("P6\n1024 768\n255\n",file);
    for(int i=0;i<1024*768;++i) {
        uint8_t pixels[3]={(uint8_t)(fb[i]>>16),(uint8_t)(fb[i]>>8),(uint8_t)fb[i]};
        if(fwrite(pixels,1,3,file)!=3){fclose(file);return 4;}
    }
    fclose(file);return 0;
}
