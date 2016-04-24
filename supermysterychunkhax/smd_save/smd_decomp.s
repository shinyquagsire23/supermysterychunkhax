.nds

.create "build/game_header.decomp",0x0

.include "smd_save/smd_constants.s"
.include "smd_save/smd_macros.s"

; using the memchunks to write has a tendency to also write to the pointer you're writing in.
; either way it's probably good to space this a bit.
.fill (0x4000 - .), 0x73

.orga 0x4000
    .incbin "smd_code/smd_code.bin"

.fill (0x3e800 - .), 0x73

.orga 0x3e800
.fill (0x3e870 - .), 0x74

.orga 0x3e870
    .word 0x70005544
    .word 0x30000 ; bogus length, does nothing
    .word SMD_STACK_COPYTO_ADDR
    .word SMD_STACK_MEMCPY_ADDR-0x8

;It seems this extension into the zlib buffer actually caused the memcpy to happen
.fill (0x3ea90 - .), 0x75

.close
