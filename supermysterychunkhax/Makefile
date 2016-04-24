all: save

clean:
	@rm -f build/* constants/* smd_code/smd_code.bin smd_code/smd_code.elf smd_code/build/*

save: smd_save/smd_constants.s smd_save/smd_macros.s smd_save/smd_rop.s smd_save/smd_save.s smd_save/smd_decomp.s smd_ropdb/ropdb_$(REGION).txt
	@mkdir -p build constants
	@python scripts/makeHeaders.py constants/constants "FIRM_VERSION=$(FIRM_VERSION)" smd_ropdb/ropdb_"$(REGION)".txt
	@cd smd_code && $(MAKE)
	armips smd_save/smd_decomp.s
	@python scripts/comp.py
	armips smd_save/smd_save.s
	
