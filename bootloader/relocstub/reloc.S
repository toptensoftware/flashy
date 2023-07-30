#define STRINGIFY(s) XSTRINGIFY(s)
#define XSTRINGIFY(s) #s

#define load_address 0x07C00000

#if AARCH == 32

.globl _start
_start:

    // Move kernel image to himem    
    ldr r4,=#load_address           /* pDest */
    ldr r5,=#kernel_image           /* pSource */
    ldr r6,=#kernel_image_end       /* pEnd */
memcpy_1:
    ldr r7, [r5], #4
    cmp r6, r5
    str r7, [r4], #4
    bne memcpy_1

    // Jump to start
    mov r4,#load_address
    bx r4

#else

.globl _start
_start:

    mov x4,#load_address                    // pDest 
    adrp x5,kernel_image
    add x5,x5,#kernel_image
    movz x8,#(kernel_image_end-kernel_image) & 0xFFFF   // count
    movk x8,#(kernel_image_end-kernel_image) >> 16, lsl #16
    mov	x6, #0x0                   	        // offset
memcpy_1:
    ldr	x7, [x5, x6]
    str	x7, [x4, x6]
    add	x6, x6, #0x8
    cmp	x6, x8
    b.ne	memcpy_1

    mov x4,#load_address
    br x4

#endif


.ltorg

.align 4
kernel_image:
    .incbin STRINGIFY(KERNELIMAGEFILE)
.align 4
kernel_image_end:
