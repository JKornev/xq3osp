# QVM build for every module (modelled on ec-/baseq3a build/linux-qvm/Makefile).
# Run inside the xq3osp-build Docker image from the repo root. MODULE selects the module
# (qagame = default, ui, cgame); its settings are in build/<module>.mk, the link orders in
# build/srcs.mk.
#   make -C build [MODULE=m]               # sources -> build/vm/<m>.qvm (+ <m>.map),
#                                          # q3lcc output in build/vm/<m>/*.asm
#   make -C build info [MODULE=m]          # build, then print the QVM's size and SHA-256
#   make -C build verify [MODULE=m]        # clean build, SHA-256 vs EXPECTED_SHA256 (OK/FAIL)
#   make -C build clean [MODULE=m]         # removes that module's outputs (clean-all: all)

include config.mk

# Absolute paths so q3lcc can run from inside $(ASMDIR).
SRCDIR_ABS  := $(abspath $(SRCDIR))
GAMEDIR_ABS := $(abspath $(GAMEDIR))
OUT_ABS     := $(abspath $(OUT))
ASMDIR      := $(OUT_ABS)/$(MODULE)
SHIM        := $(OUT_ABS)/inc
QVM         := $(OUT)/$(MODULE).qvm
MAP         := $(OUT)/$(MODULE).map

# A link-order entry (build/srcs.mk) is a file stem, compiled from <stem>.c, or a file
# ending in .asm, passed to q3asm as-is. Both are looked up in SRCDIR first and then in
# GAMEDIR, where the ui and cgame builds find the shared bg_* / q_* files (as id's
# ui.q3asm / Conscript take them from ../game). A missing .asm entry is passed as
# $(SRCDIR)/<entry>, so q3asm reports it.
find_src = $(firstword $(wildcard $(SRCDIR_ABS)/$(1) $(GAMEDIR_ABS)/$(1)))
mod_asm  = $(foreach f,$(MOD_SRC),$(if $(filter %.asm,$(f)),$(or $(call find_src,$(f)),$(SRCDIR_ABS)/$(f)),$(ASMDIR)/$(notdir $(f)).asm))
mod_hdr  = $(sort $(wildcard $(SRCDIR)/*.h $(GAMEDIR)/*.h))

# "../../ui/menudef.h" (ai_*.c, g_cmds.c) is searched relative to every -I dir;
# $(SHIM)/a/b/../../ui == $(SHIM)/ui
INCLUDES = -I$(SRCDIR_ABS) $(if $(filter-out $(SRCDIR_ABS),$(GAMEDIR_ABS)),-I$(GAMEDIR_ABS)) \
	$(MOD_INCLUDES) -I$(SHIM)/a/b

all: $(QVM)

$(QVM): $(mod_asm) srcs.mk
	$(Q3ASM) $(ASMFLAGS) -o $(OUT_ABS)/$(MODULE) $(mod_asm)
	@test -f $@

define compile
	@mkdir -p $(ASMDIR)
	cd $(ASMDIR) && Q3CPP_DATE='$(Q3CPP_DATE)' Q3CPP_TIME='$(Q3CPP_TIME)' LD_PRELOAD=$(CPPDATE_SO) $(Q3LCC) $(LCCFLAGS) $(MOD_CFLAGS) $(INCLUDES) -o $*.asm.tmp $(abspath $<) \
		&& mv $*.asm.tmp $*.asm
endef

$(ASMDIR)/%.asm: $(SRCDIR_ABS)/%.c $(mod_hdr) config.mk $(MODULE).mk | $(SHIM)/ui/menudef.h
	$(compile)

$(ASMDIR)/%.asm: $(GAMEDIR_ABS)/%.c $(mod_hdr) config.mk $(MODULE).mk | $(SHIM)/ui/menudef.h
	$(compile)

$(SHIM)/ui/menudef.h: $(UIDIR)/menudef.h
	mkdir -p $(SHIM)/a/b $(SHIM)/ui
	cp $< $@

info: $(QVM)
	@echo "$(QVM): $$(wc -c < $(QVM)) bytes"
	@echo "sha256: $$(sha256sum $(QVM) | cut -d' ' -f1)"

# Clean build, then the SHA-256 of the QVM against the release checksum of the module
# (EXPECTED_SHA256 in build/<module>.mk); exits non-zero on mismatch.
verify:
	$(MAKE) clean
	$(MAKE) all
	@ours=$$(sha256sum $(QVM) | cut -d' ' -f1); \
	echo "ours:     $$ours  $(QVM)"; \
	echo "expected: $(EXPECTED_SHA256)  $(MODULE)"; \
	if [ -n "$$ours" ] && [ "$$ours" = "$(EXPECTED_SHA256)" ]; then \
		echo "verify: OK, $(QVM) matches the OSP 1.03a release"; \
	else \
		echo "verify: FAIL, SHA-256 differs from the OSP 1.03a release"; exit 1; \
	fi

clean:
	rm -rf $(ASMDIR) $(QVM) $(MAP)

clean-all:
	rm -rf $(OUT)

.PHONY: all info verify clean clean-all
