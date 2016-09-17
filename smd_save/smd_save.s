.nds

.create "build/game_header",0x0

save_start:

.include "smd_save/smd_constants.s"
.include "smd_save/smd_macros.s"

.orga 0x0
    .incbin "build/game_header.comp"
    .ascii "P2FT"
    .word 0x0
    .word 0x0
    .word 0x0

.fill (0x8000 - .), 0x72

.orga 0x8000
	.include "smd_save/smd_rop.s"

.fill (0x20000-0x4 - .), 0x72

.orga 0x20000-0x4
.word 0x0 ; size, payload_end - payload_start

;payload_start:
;.orga 0x20000
;	.incbin "payload.bin"
;payload_end:

.fill (0x32000 - .), 0x72

.orga 0x32000
.fill (0x32070 - .), 0x73

.orga 0x32070
    .word 0x70005544
    .word 0x30000
    .word 0x4b65cc ; old magic rop gadget, this value doesn't actually matter any more though
    .word SMD_COMP_LIMIT-0x8

.close
