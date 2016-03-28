/*
Copyright (c) 2012, Vinayak Kariappa Chettimada
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
	.thumb

	.section .irq_vectors
	.type vector, %object
vector:
	.word __ram_end
	.word reset_handler
	.word nmi_handler
	.word hardfault_handler
	.word memory_management_handler
	.word bus_fault_handler
	.word usage_fault_handler
	.word 0

	.word 0
	.word 0
	.word 0
	.word svc_handler
	.word 0
	.word 0
	.word pendsv_handler
	.word systick_handler

	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0

	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0

	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0

	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0
	.word 0

	.section .text
	.thumb_func
	.type reset_handler, %function
reset_handler:
	ldr r1, =__text_end
	ldr r2, =__data_start
	ldr r3, =__data_end

	sub r3, r2
	ble no_data

	mov r4, #0x00
data_loop:
	ldr r0, [r1,r4]
	str r0, [r2,r4]
	add r4, #0x04
	cmp r4, r3
	blt data_loop
no_data:

	ldr r2, =__bss_start
	ldr r3, =__bss_end

	sub r3, r2
	ble no_bss

	mov r4, #0x00
	mov r0, r4
bss_loop:
	str r0, [r2,r4]
	add r4, #0x04
	cmp r4, r3
	blt bss_loop
no_bss:

	ldr r0,=main
	bx r0

  .section .text
  .thumb_func
  .weak hardfault_handler
  .type hardfault_handler, %function
hardfault_handler:
  mov   r0, #0x04
  mov   r1, lr
  tst   r0, r1
  beq   use_msp_hardfault
  mrs   r0, psp
  b     call_c_hardfault_handler
use_msp_hardfault:
  mrs   r0, msp
call_c_hardfault_handler:
  ldr   r1, =C_Hardfault_Handler
  bx    r1

  .section .text
  .thumb_func
  .weak svc_handler
  .type svc_handler, %function
svc_handler:
  mov   r0, #0x04
  mov   r1, lr
  tst   r0, r1
  beq   use_msp_svc
  mrs   r1, psp
  b     call_c_svc_handler
use_msp_svc:
  mrs   r1, msp
call_c_svc_handler:
  ldr   r0, [r1, #24]
  sub   r0, #0x02
  ldrb  r0, [r0]
  ldr   r2, =C_SVC_Handler
  bx    r2

	.macro default_handler handler
	.thumb_func
	.weak \handler
	.type \handler, %function
\handler:
	b .
	.endm

	default_handler nmi_handler
	default_handler C_Hardfault_Handler
	default_handler memory_management_handler
	default_handler bus_fault_handler
	default_handler usage_fault_handler
	default_handler C_SVC_Handler
	default_handler pendsv_handler
	default_handler systick_handler

	.end

