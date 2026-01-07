# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (c) 2024, Kongyang Liu <seashell11234455@gmail.com>
# Copyright (c) 2025-2026, RISCStar Ltd.

obj-y := board.o
obj-$(CONFIG_SPL_BUILD) += spl.o

DDR_FW_SRC ?= $(DDR_FW_FILE)
FW_TARGET = $(CURDIR)/ddr_fw.bin

DDR_FW_HEADER = $(srctree)/include/generated/ddr_fw_info.h

$(obj)/spl.o: $(DDR_FW_HEADER)

$(FW_TARGET):
	@echo "Preparing DDR firmware..."
	@if [ -n "$(DDR_FW_SRC)" ] && [ -f "$(DDR_FW_SRC)" ]; then \
		echo "  Copying from: $(DDR_FW_SRC)"; \
		cp "$(DDR_FW_SRC)" $@; \
	elif [ -f $@ ]; then \
		echo "  Using existing $@"; \
	else \
		echo "  Note: No firmware found, creating empty file"; \
		echo "  (Set DDR_FW_FILE to specify firmware location)"; \
		touch $@; \
	fi
	@if [ -f $@ ]; then \
		SIZE=$$(stat -c%s $@ 2>/dev/null || echo 0); \
		echo "  Final: $@ ($$SIZE bytes)"; \
	fi

$(DDR_FW_HEADER): $(FW_TARGET)
	@mkdir -p $(dir $@)
	@echo "/* DDR firmware info - $$(date) */" > $@
	@if [ -f "$(FW_TARGET)" ]; then \
		SIZE=$$(stat -c%s "$(FW_TARGET)" 2>/dev/null || echo 0); \
	else \
		SIZE=0; \
	fi; \
	echo "#define DDR_FW_FILE_SIZE	$$SIZE" >> $@
	@if grep -q "CONFIG_SPL_DDR_FIRMWARE_OFFSET=0x" .config 2>/dev/null; then \
		OFFSET=$$(grep "CONFIG_SPL_DDR_FIRMWARE_OFFSET=0x" .config | cut -d= -f2); \
	fi; \
	echo "#define DDR_FW_OFFSET	$$OFFSET" >> $@
	@echo "/* Note: Update ADDR if binman layout changes */" >> $@

clean-files += $(FW_TARGET) $(DDR_FW_HEADER)
