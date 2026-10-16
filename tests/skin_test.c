#include "../apps/native/skin.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char blob[OXP_SKIN_MAX_BYTES];
static size_t size;
static int memreader(void *unused,uint32_t off,void *dst,uint32_t n){
    (void)unused;
    if(off>size||n>size-off)return 0;
    memcpy(dst,blob+off,n);return 1;
}
static void put32(unsigned char *p,unsigned n){
    p[0]=(unsigned char)n;p[1]=(unsigned char)(n>>8);
    p[2]=(unsigned char)(n>>16);p[3]=(unsigned char)(n>>24);
}
int main(int argc,char **argv){
    assert(argc==2);
    FILE *f=fopen(argv[1],"rb");assert(f);
    size=fread(blob,1,sizeof(blob),f);
    assert(!ferror(f)&&feof(f));fclose(f);
    assert(size>=16u);
    oxp_skin_t skin;
    assert(oxp_skin_parse(&skin,blob,size));
    assert(skin.count==5u);
    for(unsigned i=0;i<skin.count;++i){
        assert(skin.sprites[i].id==i);
        assert(skin.sprites[i].offset<size);
    }
    unsigned char copy[OXP_SKIN_MAX_BYTES];
    oxp_skin_t loaded;
    assert(oxp_skin_load(&loaded,copy,memreader,0));
    uint16_t pixels[1024];
    for(unsigned i=0;i<1024;++i)pixels[i]=0xffffu;
    assert(oxp_skin_nine_row(&loaded,OXP_SKIN_FRAME_ACTIVE,20u,10u,
                             470u,300u,10u,pixels,1024u));
    assert(pixels[20u]==0xffffu); /* transparent curved corner */
    assert(pixels[27u]!=0xffffu); /* painted top highlight */
    assert(pixels[0u]==0xffffu); /* correct scissor */
    assert(!oxp_skin_nine_row(&loaded,0u,20u,10u,470u,300u,310u,pixels,1024u));
    assert(oxp_skin_nine_row(&loaded,OXP_SKIN_FRAME_INACTIVE,1019u,0u,
                             470u,300u,40u,pixels,1024u)); /* clip to last five */
    assert(oxp_skin_nine_row(&loaded,OXP_SKIN_FRAME_ACTIVE,2u,2u,
                             6u,4u,3u,pixels,1024u)); /* tiny 9-slice */
    assert(oxp_skin_region_row(&loaded,OXP_SKIN_CAPTION_ATLAS,48u,48u,24u,24u,
                               100u,10u,21u,21u,13u,pixels,1024u));
    assert(!oxp_skin_region_row(&loaded,OXP_SKIN_CAPTION_ATLAS,70u,70u,24u,24u,
                                100u,10u,21u,21u,13u,pixels,1024u));
    assert(!oxp_skin_nine_row(&loaded,6u,2u,2u,300u,200u,42u,pixels,1024u));
    unsigned char header_backup[16];memcpy(header_backup,blob,16);
    blob[0]='?';assert(!oxp_skin_parse(&skin,blob,size));blob[0]='O';
    put32(blob+8,OXP_SKIN_MAX_BYTES+1u);
    assert(!oxp_skin_load(&skin,copy,memreader,0));
    memcpy(blob,header_backup,16);
    /* Reject pixel ranges outside the resource and duplicate IDs. */
    unsigned char row_backup[20];memcpy(row_backup,blob+16,20);
    put32(blob+16+16,(unsigned)size-2u);
    assert(!oxp_skin_parse(&skin,blob,size));
    memcpy(blob+16,row_backup,20);
    blob[16+20]=0;blob[16+21]=0;
    assert(!oxp_skin_parse(&skin,blob,size));
    puts("PASS: OXSK parser, streamed load, 9-slice, atlas, bounds and fallback");
    return 0;
}
