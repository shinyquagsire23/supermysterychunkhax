.section ".init"
.arm
.align 4
.global _start

_start:
	mov sp, #0x10000000
	blx _main
