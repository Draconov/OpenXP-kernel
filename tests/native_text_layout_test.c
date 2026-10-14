#include "../apps/native/text_layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void check(const char *text,unsigned cols){
    size_t len=strlen(text);
    for(size_t i=0;i<=len;++i){
        oxp_text_position_t p=oxp_text_position(text,len,i,cols);
        assert(p.col<cols);
        assert(oxp_text_offset_at(text,len,cols,p.line,p.col)==i);
    }
}
int main(void){
    check("",8);check("12345678",8);check("123456789",8);
    check("12345678\n",8);check("\n",8);check("alpha\nbeta\n",4);
    check("The quick brown fox jumps over the lazy dog",7);
    for(unsigned cols=1;cols<=49;++cols){
        check("one long line without any line feeds, so words wrap.",cols);
        check("line one\nline two\n\nfinal line\n",cols);
    }
    puts("PASS: wrapped editor cursor/row coordinates and end-of-line positions");
}
