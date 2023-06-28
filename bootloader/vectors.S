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
#ifdef BOOTLOADER
    b skip
.space 0x200000-0x8004,0
skip:
#endif
    mov sp,#0x08000000
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
    bx r0

.globl dummy
dummy:
    bx lr

#elif AARCH == 64

//-------------------------------------------------------------------------
// AARCH64

.globl _start
_start:
#ifdef BOOTLOADER
    b skip
.space 0x280000-0x80004,0
skip:
#endif

    mov sp,#0x08000000
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
    ret


.globl dummy
dummy:
    ret

#else

#error AARCH not specified

#endif

