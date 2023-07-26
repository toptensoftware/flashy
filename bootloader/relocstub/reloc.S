#define STRINGIFY(s) XSTRINGIFY(s)
#define XSTRINGIFY(s) #s

#define load_address 0x07C00000

#if AARCH == 32

.globl _start
_start:

    // Move kernel image to himem    
    ldr r0,=#load_address           /* pDest */
    ldr r1,=#kernel_image           /* pSource */
    ldr r2,=#kernel_image_end       /* pEnd */
memcpy_1:
    ldr r3, [r1], #4
    cmp r2, r1
    str r3, [r0], #4
    bne memcpy_1

    // Jump to start
    mov r0,#load_address
    bx r0

#else

.globl _start
_start:

    mov x0,#load_address                    // pDest 
    adrp x1,kernel_image
    add x1,x1,#kernel_image
    movz x4,#(kernel_image_end-kernel_image) & 0xFFFF   // count
    movk x4,#(kernel_image_end-kernel_image) >> 16, lsl #16
    mov	x2, #0x0                   	        // offset
memcpy_1:
    ldr	x3, [x1, x2]
    str	x3, [x0, x2]
    add	x2, x2, #0x8
    cmp	x2, x4
    b.ne	memcpy_1

    mov x0,#load_address
    br x0

#endif


.ltorg

.align 4
kernel_image:
    .incbin STRINGIFY(KERNELIMAGEFILE)
.align 4
kernel_image_end:
