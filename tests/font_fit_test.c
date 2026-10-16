#include "../apps/native/font_engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
    /* Proportional advances exercise pixel fitting, not character counts. */
    static uint8_t advances[OXP_FONT_COUNT*OXP_FONT_STYLES];
    for(unsigned i=0;i<sizeof advances;++i)advances[i]=6;
    advances['i'-OXP_FONT_FIRST]=2;
    advances['W'-OXP_FONT_FIRST]=9;
    advances['.'-OXP_FONT_FIRST]=3;
    oxp_font_t font={advances,0};
    oxp_font_fit_t fit=oxp_font_fit(&font,"iiWW",0,22,92);
    assert(fit.chars==4 && fit.width==22 && fit.dots==0);
    fit=oxp_font_fit(&font,"iiWWx",0,22,92);
    assert(fit.chars==3 && fit.dots==3 && fit.width==22);
    fit=oxp_font_fit(&font,"WWWWW",0,8,92);
    assert(fit.chars==0 && fit.dots==0 && fit.width==0);
    fit=oxp_font_fit(&font,"WWWWW",0,9,92);
    assert(fit.chars==0 && fit.dots==3 && fit.width==9);
    fit=oxp_font_fit(&font,"",0,200,92);
    assert(fit.chars==0 && fit.dots==0 && fit.width==0);
    fit=oxp_font_fit(&font,"WWWW",0,100,2);
    assert(fit.chars==2 && fit.dots==3 && fit.width==27);
    fit=oxp_font_fit(&font,"WWWW",0,0,92);
    assert(fit.chars==0 && fit.dots==0);
    fit=oxp_font_fit(&font,0,0,10,92);
    assert(fit.chars==0 && fit.dots==0);
    fit=oxp_font_fit(&font,"WWWW",0,100,0);
    assert(fit.chars==0 && fit.dots==0);
    char very_long[600];memset(very_long,'W',sizeof very_long-1);
    very_long[sizeof very_long-1]=0;
    fit=oxp_font_fit(&font,very_long,0,200,999);
    assert(fit.chars<=256u && fit.width<=200u && fit.dots==3u);
    /* An unterminated buffer is safe with an explicit byte cap. */
    char raw[2]={'W','W'};
    fit=oxp_font_fit(&font,raw,0,100,2);
    assert(fit.chars==2 && fit.dots==3 && fit.width==27);
    puts("PASS: proportional clipping, ellipses, caps and tiny viewports");
    return 0;
}
