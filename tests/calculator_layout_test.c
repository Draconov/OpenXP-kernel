#include "../apps/native/calculator_layout.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    for(unsigned w=100u;w<=640u;w+=45u)for(unsigned h=90u;h<=360u;h+=30u){
        for(unsigned r=0;r<4u;++r)for(unsigned c=0;c<4u;++c){
            oxp_calc_rect b=oxp_calc_key(w,h,c,r);
            assert(b.w>=1u&&b.h>=1u);
            assert(b.x+b.w<=w&&b.y+b.h<=h);
            assert(oxp_calc_key_at(w,h,b.x+b.w/2u,b.y+b.h/2u)==(int)(r*4u+c));
        }
        assert(oxp_calc_key_at(w,h,0u,0u)==-1);
    }
    assert(oxp_calc_key(10,10,0,0).w==0u);
    puts("PASS: Aurora calculator artwork positions and click geometry");
    return 0;
}
