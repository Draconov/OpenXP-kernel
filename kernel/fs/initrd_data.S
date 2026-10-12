#ifndef OPENXP_INITRD_PATH
#define OPENXP_INITRD_PATH "build/initrd.img"
#endif
.section .rodata.initrd,"a",@progbits
.balign 16
.global openxp_initrd_start
.global openxp_initrd_end
openxp_initrd_start:
.incbin OPENXP_INITRD_PATH
openxp_initrd_end:
.section .note.GNU-stack,"",@progbits
