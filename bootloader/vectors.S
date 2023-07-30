//-------------------------------------------------------------------------
//
// Copyright (c) 2012 David Welch dwelch@dwelch.com
//
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in 
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.
//
//-------------------------------------------------------------------------



#if AARCH == 32

//-------------------------------------------------------------------------
// AARCH32

.globl _start
_start:
    mov sp,#0x08000000
    // Store r0-r4 for chain boot
    push { r0, r1, r2, r3 }
    bl start_main
hang: b hang

.globl PUT32
PUT32:
    str r1,[r0]
    bx lr

.globl PUT16
PUT16:
    strh r1,[r0]
    bx lr

.globl PUT8
PUT8:
    strb r1,[r0]
    bx lr

.globl GET32
GET32:
    ldr r0,[r0]
    bx lr

.globl GETPC
GETPC:
    mov r0,lr
    bx lr

.globl BRANCHTO
BRANCHTO:
    mov r5, r0
    mov sp,#0x08000000
    sub sp,#16
    pop { r0, r1, r2, r3 }
    bx r5


.globl dummy
dummy:
    bx lr

#elif AARCH == 64

//-------------------------------------------------------------------------
// AARCH64

.globl _start
_start:
    mov sp, #0x08000000
    // Store x0-x3 for chain boot
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    bl start_main
hang: b hang

.globl PUT32
PUT32:
  str w1,[x0]
  ret

.globl PUT8
PUT8:
  strb w1,[x0]
  ret

.globl GET32
GET32:
    ldr w0,[x0]
    ret

.globl GETPC
GETPC:
    mov x0,x30
    ret

.globl BRANCHTO
BRANCHTO:
    mov w30,w0
    mov sp, #(0x08000000-32)
    ldp	x2, x3, [sp], #16
    ldp	x0, x1, [sp], #16
    ret


.globl dummy
dummy:
    ret

#else

#error AARCH not specified

#endif

