// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Spacemit
 */

#include <cpu_func.h>
#include <dm.h>
#include <errno.h>
#include <div64.h>
#include <fdtdec.h>
#include <init.h>
#include <log.h>
#include <ram.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <linux/sizes.h>
#include "ddr_init.h"

#define DDR_CHECK_SIZE			(0x2000)
#define DDR_CHECK_STEP			(0x2000)
#define DDR_CHECK_CNT			(0x1000)
#define TOP_DDR_NUM				1

extern int ddr_freq_change(u32 data_rate);
extern void qos_set_default(void);

static int test_pattern(fdt_addr_t base, fdt_size_t size)
{
	fdt_addr_t addr;
	fdt_size_t check_size;
	uint32_t offset;
	uint32_t *ddr_data = NULL;
	uint32_t *save_data;
	int err = 0;

	// use code sram as temp data buffer(16KB), not enough heap memory
	ddr_data = (uint32_t*)0xC08D0000;
	check_size = (DDR_CHECK_SIZE / DDR_CHECK_STEP) * DDR_CHECK_CNT;
	if (check_size > 0x4000) {
		pr_err("test zone malloc fail size 0x%llx\n", check_size);
		return -1;
	}

	save_data = ddr_data;
	/* to avoid overlap important data as image or ramdump  */
	for (addr = base; addr < base + size; addr += DDR_CHECK_STEP) {
		for (offset = 0; offset < DDR_CHECK_CNT; offset += 4) {
			*save_data = readl((void*)addr + offset);
			save_data++;
		}
	}

	for (addr = base; addr < base + size; addr += DDR_CHECK_STEP) {
		for (offset = 0; offset < DDR_CHECK_CNT; offset += 4) {
			writel((uint32_t)(addr + offset), (void*)addr + offset);
		}
	}

	/* writeback and invalid cache */
	flush_dcache_range(base,base+size);
	invalidate_dcache_range(base,base+size);

	for (addr = base; addr < base + size; addr += DDR_CHECK_STEP) {
		for (offset = 0; offset < DDR_CHECK_CNT; offset += 4) {
			if (readl((void*)addr + offset) != (uint32_t)(addr + offset)) {
				pr_err("ddr check error %x vs %x\n", (uint32_t)(addr + offset), readl((void*)addr + offset));
				err++;
				if (err > 10)
					goto ERR_HANDLE;
			}
		}
	}

	for (addr = base; addr < base + size; addr += DDR_CHECK_STEP) {
		for (offset = 0; offset < DDR_CHECK_CNT; offset += 4) {
			writel((~(uint32_t)(addr + offset)), (void*)addr + offset);
		}
	}

	/* writeback and invalid cache */
	flush_dcache_range(base,base+size);
	invalidate_dcache_range(base,base+size);

	for (addr = base; addr < base + size; addr += DDR_CHECK_STEP) {
		for (offset = 0; offset < DDR_CHECK_CNT; offset += 4) {
			if (readl((void*)addr + offset) != (~(uint32_t)(addr + offset))) {
				pr_err("ddr check error %x vs %x\n", (uint32_t)(~(addr + offset)), readl((void*)addr + offset));
				err++;
				if (err > 10)
					goto ERR_HANDLE;

			}
		}
	}

ERR_HANDLE:
	save_data = ddr_data;
	for (addr = base; addr < base + size; addr += DDR_CHECK_STEP) {
		for (offset = 0; offset < DDR_CHECK_CNT; offset += 4) {
			writel(*save_data, (void*)addr + offset);
			save_data++;
		}
	}
	if (err != 0) {
		pr_emerg("dram pattern test failed!\n");
	}

	// free(ddr_data);

	return err;
}

static int spacemit_ddr_probe(struct udevice *dev)
{
	int ret;
	uint32_t data_rate;
	uint64_t start_time, elapsed;
	const char *ddr_type;
	struct ddr_config cfg;


	memset(&cfg, 0, sizeof(struct ddr_config));
	cfg.base = dev_read_addr(dev);
	ret = dev_read_u32u(dev, "spacemit,data-rate", &cfg.data_rate);
	if (ret || cfg.data_rate == 0)
		panic("DDR: fail to get data rate from DTB\n");
	ret = dev_read_u32u(dev, "spacemit,cs-num", &cfg.cs_num);
	if (ret || cfg.cs_num == 0)
		panic("DDR: fail to get cs num from DTB\n");
	ret = dev_read_u32u(dev, "spacemit,tx-odt", &cfg.tx_odt);
	if (ret || cfg.cs_num == 0)
		panic("DDR: fail to get tx odt from DTB\n");
	ddr_type = dev_read_string(dev, "spacemit,ddr-type");
	if (!ddr_type)
		panic("DDR: fail to get type from DTB\n");
	memcpy(&cfg.type[0], ddr_type, strlen(ddr_type));

	printf("DDR type: %s\n", ddr_type);

	/* init dram */
	start_time = get_timer(0);
	data_rate = lpddr4_silicon_init(&cfg);
	elapsed = get_timer(start_time);
	printf("lpddr4_silicon_init consume %ldms\n", elapsed);
	ddr_freq_change(data_rate);

	ret = test_pattern(CFG_SYS_SDRAM_BASE, DDR_CHECK_SIZE);
	if (ret < 0) {
		pr_err("dram init failed!\n");
		return -EIO;
	}
#ifdef CONFIG_DDR_QOS
	qos_set_default();
#endif
	pr_info("dram init done\n");

	return 0;
}

static const struct udevice_id spacemit_ddr_ids[] = {
	{ .compatible = "spacemit,ddr" },
	{}
};

U_BOOT_DRIVER(spacemit_ddr) = {
	.name		= "spacemit_ddr",
	.id		= UCLASS_RAM,
	.of_match	= spacemit_ddr_ids,
	.probe		= spacemit_ddr_probe,
};
