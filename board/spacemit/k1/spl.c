// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2025-2026, RISCStar Ltd.
 */

#include <asm/io.h>
#include <binman_sym.h>
#include <clk.h>
#include <clk-uclass.h>
#include <configs/k1.h>
#include <cpu_func.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <dt-bindings/pinctrl/k1-pinctrl.h>
#include <generated/ddr_fw_info.h>
#include <i2c.h>
#include <linux/delay.h>
#include <power/regulator.h>
#include <spl.h>
#include <tlv_eeprom.h>
#include "tlv_codes.h"

#define I2C_PIN_CONFIG(x)       ((x) | EDGE_NONE | PULL_UP | PAD_1V8_DS2)
#define I2C_BUF_SIZE		64

#define MFP_GPIO_84		0xd401e154
#define MFP_GPIO_85		0xd401e158

#define DDR_DEFAULT_CS_NUM	2
#define DDR_DEFAULT_TYPE	"LPDDR4X"
#define DDR_DEFAULT_TX_ODT	80
#define DDR_DEFAULT_DATA_RATE	2400

struct ddr_cfg {
	u32	data_rate;
	u32	cs_num;
	u32	tx_odt;
	u8	type[I2C_BUF_SIZE];
};

static void reset_early_init(void)
{
	struct udevice *dev;
	int ret;

	ret = uclass_get_device(UCLASS_RESET, 0, &dev);
	if (ret)
		panic("Fail to detect reset controller.\n");
}

static void i2c_scan_bus(struct udevice *bus)
{
	struct udevice *dev;
	uint chip;
	int ret;

	printf("Probe i2c devices on bus %s\n", bus->name);
	for (chip = 0x40; chip < 0x51; chip++) {
		ret = dm_i2c_probe(bus, chip, 0, &dev);
		if (ret == 0) {
			printf("Found i2c (0x%x) on bus %s\n",
				chip, bus->name);
		}
	}
}

static void i2c_early_init(void)
{
	struct udevice *bus;

	// eeprom: I2C2, pin group(GPIO_84, GPIO_85)
	writel(I2C_PIN_CONFIG(MUX_MODE4), (void __iomem *)MFP_GPIO_84);
	writel(I2C_PIN_CONFIG(MUX_MODE4), (void __iomem *)MFP_GPIO_85);
	udelay(100);
	uclass_first_device(UCLASS_I2C, &bus);
	while (bus) {
		i2c_scan_bus(bus);
		uclass_next_device(&bus);
		if (!bus)
			break;
	}
}

int read_product_name(char *name, int size)
{
	u8 eeprom_data[TLV_TOTAL_LEN_MAX], *p;
	struct tlvinfo_header *tlv_hdr;
	struct tlvinfo_tlv *tlv_entry;
	int ret, i;
	u32 entry_size;

	if (!name || size < 0)
		return -EINVAL;
	ret = read_tlvinfo_tlv_eeprom(eeprom_data, &tlv_hdr,
				      &tlv_entry, i);
	if (ret)
		return ret;
	p = (u8 *)tlv_entry;
	for (i = 0; i < tlv_hdr->totallen; ) {
		if (tlv_entry->type == TLV_CODE_PRODUCT_NAME) {
			if (tlv_entry->length < size)
				size = tlv_entry->length;
			memset(name, 0, size);
			memcpy(name, &tlv_entry->value[0], size);
			return 0;
		}
		if (tlv_entry->type == TLV_CODE_CRC_32)
			return -ENOENT;
		entry_size = tlv_entry->length + sizeof(struct tlvinfo_tlv);
		i += entry_size;
		p += entry_size;
		tlv_entry = (struct tlvinfo_tlv *)p;
	}
	return -ENOENT;
}

/* Set default value for DDR chips */
static void ddr_cfg_init(struct ddr_cfg *cfg)
{
	memset(cfg, 0, sizeof(struct ddr_cfg));
	cfg->data_rate = DDR_DEFAULT_DATA_RATE;
	cfg->cs_num = DDR_DEFAULT_CS_NUM;
	cfg->tx_odt = DDR_DEFAULT_TX_ODT;
	strcpy(cfg->type, DDR_DEFAULT_TYPE);
}

int read_ddr_info(struct ddr_cfg *cfg)
{
	u8 eeprom_data[TLV_TOTAL_LEN_MAX], *p;
	struct tlvinfo_header *tlv_hdr;
	struct tlvinfo_tlv *tlv_entry;
	u32 size, entry_size;
	int ret, i;
	bool found = false;

	if (!cfg)
		return -EINVAL;
	ddr_cfg_init(cfg);
	ret = read_tlvinfo_tlv_eeprom(eeprom_data, &tlv_hdr,
				      &tlv_entry, i);
	if (ret)
		return ret;
	p = (u8 *)tlv_entry;
	for (i = 0; i < tlv_hdr->totallen; ) {
		switch (tlv_entry->type) {
		case TLV_CODE_DDR_CSNUM:
			memcpy(&cfg->cs_num, &tlv_entry->value[0], 1);
			found = true;
			break;
		case TLV_CODE_DDR_TYPE:
			size = min((u32)tlv_entry->length, (u32)I2C_BUF_SIZE);
			memcpy(&cfg->type[0], &tlv_entry->value[0], size);
			found = true;
			break;
		case TLV_CODE_DDR_DATARATE:
			memcpy(&cfg->data_rate, &tlv_entry->value[0], 2);
			found = true;
			break;
		case TLV_CODE_DDR_TX_ODT:
			memcpy(&cfg->tx_odt, &tlv_entry->value[0], 1);
			found = true;
			break;
		case TLV_CODE_CRC_32:
			if (!found)
				return -ENOENT;
			return 0;
		}
		entry_size = tlv_entry->length + sizeof(struct tlvinfo_tlv);
		i += entry_size;
		p += entry_size;
		tlv_entry = (struct tlvinfo_tlv *)p;
	}
	if (!found)
		return -ENOENT;
	return 0;
}

static void clk_early_init(void)
{
	struct udevice *dev;
	int ret;

	ret = uclass_get_device_by_name(UCLASS_CLK, "clock-controller@d4090000", &dev);
	if (ret)
		panic("Fail to detect clock-controller@d4090000\n");
	ret = uclass_get_device_by_name(UCLASS_CLK, "system-controller@d4050000", &dev);
	if (ret)
		panic("Fail to detect system-controller@d4005000\n");
	ret = uclass_get_device_by_name(UCLASS_CLK, "system-controller@d4282800", &dev);
	if (ret)
		panic("Fail to detect system-controller@d4282800\n");
	ret = uclass_get_device_by_name(UCLASS_CLK, "system-controller@d4015000", &dev);
	if (ret)
		panic("Fail to detect system-controller@d4015000\n");

	if (device_active(dev))
		printf("clk: device is active\n");
	else
		printf("clk: device not active, probing...\n");
}

void serial_early_init(void)
{
	struct udevice *dev;
	int ret;

	ret = uclass_get_device(UCLASS_SERIAL, 0, &dev);
	if (ret)
		panic("Serial uclass init failed: %d\n", ret);
}

static void set_vdd_core(void)
{
	struct udevice *dev;
	int ret;

	ret = regulator_get_by_platname("vdd_core", &dev);
	if (ret)
		printf("Fail to detect vdd_core (%d)\n", ret);
	ret = regulator_set_enable(dev, true);
	if (ret)
		printf("Fail to enable vdd_core (%d)\n", ret);
	ret = regulator_get_value(dev);
	if (ret < 0)
		printf("Fail to read vdd_core (%d)\n", ret);
	printf("vdd_core, value:%d\n", ret);
}

static void set_vdd_1v8(void)
{
	struct udevice *dev;
	int ret;

	ret = regulator_get_by_platname("vdd_1v8", &dev);
	if (ret)
		printf("Fail to detect vdd_1v8 (%d)\n", ret);
	ret = regulator_set_value(dev, 1800000);
	if (ret)
		printf("Fail to set vdd_1v8 as 1800000 (%d)\n", ret);
	ret = regulator_set_enable(dev, true);
	if (ret)
		printf("Fail to enable vdd_1v8 (%d)\n", ret);
	ret = regulator_get_value(dev);
	if (ret < 0)
		printf("Fail to read vdd_1v8 (%d)\n", ret);
	printf("vdd_1v8, value:%d\n", ret);
}

static void set_vdd_mmc(void)
{
	struct udevice *dev;
	int ret;

	ret = regulator_get_by_platname("vdd_1v8_mmc", &dev);
	if (ret)
		printf("Fail to detect vdd_1v8_mmc (%d)\n", ret);
	ret = regulator_set_enable(dev, true);
	if (ret)
		printf("Fail to enable vdd_1v8_mmc (%d)\n", ret);
	ret = regulator_get_value(dev);
	if (ret < 0)
		printf("Fail to read vdd_1v8_mmc (%d)\n", ret);
	printf("vdd_1v8_mmc, value:%d\n", ret);
}

void pmic_init(void)
{
	struct udevice *pmic_dev;
	int ret;

	ret = uclass_get_device(UCLASS_PMIC, 0, &pmic_dev);
	if (ret)
		panic("Fail to detect PMIC (%d)\n", ret);
	set_vdd_core();
	set_vdd_1v8();
	set_vdd_mmc();
}

/* Load DDR training firmware */
int init_ddr_firmware(void)
{
	void __iomem *src, *dst;
	unsigned long size;

	src = (void __iomem *)(CONFIG_SPL_TEXT_BASE + DDR_FW_OFFSET);
	dst = (void __iomem *)(DDR_TRAINING_DATA_BASE);
	memcpy(dst, src, DDR_FW_FILE_SIZE);
	size = round_up(DDR_FW_FILE_SIZE, 64);
	flush_dcache_range((u32)(u64)dst, (u32)(u64)dst + size);
	return 0;
}

int update_ddr_info_in_fdt(struct ddr_cfg *cfg)
{
	int node, ret;
	char *compatible, *name;
	void *fdt = gd->fdt_blob;

	if (!fdt || fdt_check_header(fdt) != 0) {
		printf("SPL: invalid FDT pointer\n");
		return -ENOENT;
	}

	node = fdt_path_offset(fdt, "/soc/ddr-controller@c0000000");
	if (node < 0) {
		printf("SPL: fail to find /soc/ddr-controller@c0000000\n");
		return -ENOENT;
	}

	compatible = fdt_getprop(fdt, node, "compatible", NULL);
	if (strcmp(compatible, "spacemit,ddr") != 0) {
		printf("SPL: DDR compatible is not matched\n");
		return -EINVAL;
	}

	name = "spacemit,cs-num";
	ret = fdt_setprop_u32(fdt, node, name, cfg->cs_num);
	if (ret) {
		printf("Fail to set property '%s': %s\n",
		       name, fdt_strerror(ret));
		return ret;
	}
	name = "spacemit,data-rate";
	ret = fdt_setprop_u32(fdt, node, name, cfg->data_rate);
	if (ret) {
		printf("Fail to set property '%s': %s\n",
		       name, fdt_strerror(ret));
		return ret;
	}
	name = "spacemit,tx-odt";
	ret = fdt_setprop_u32(fdt, node, name, cfg->tx_odt);
	if (ret) {
		printf("Fail to set property '%s': %s\n",
		       name, fdt_strerror(ret));
		return ret;
	}
	name = "spacemit,ddr-type";
	ret = fdt_setprop_string(fdt, node, name, cfg->type);
	if (ret) {
		printf("Fail to set property '%s': %s\n",
		       name, fdt_strerror(ret));
		return ret;
	}
	return 0;
}

void ddr_early_init(void)
{
	struct udevice *dev;
	struct ddr_cfg cfg;
	void __iomem *addr;
	int ret;

	init_ddr_firmware();
	read_ddr_info(&cfg);
	update_ddr_info_in_fdt(&cfg);
	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret)
		printf("Fail to detect ddr controller.\n");
	{
		void __iomem *addr = 0;

		printf("[0x%x]: 0x%x 0x%x\n", addr, readl(addr), readl(addr + 4));
		writel(0xa5a5, addr);
		flush_dcache_range(0, 0x1000);
		invalidate_dcache_range(0, 0x1000);
		printf("[0x%x]: 0x%x 0x%x\n", addr, readl(addr), readl(addr + 4));
	}
}

void board_init_f(ulong dummy)
{
	u8 i2c_buf[I2C_BUF_SIZE];
	int ret;

	ret = spl_early_init();
	if (ret)
		printf("spl_early_init() failed:%d\n", ret);

	riscv_cpu_setup();

	/*
	reset_early_init();
	clk_early_init();
	*/
	serial_early_init();

	preloader_console_init();

	ddr_early_init();
	/*
	i2c_early_init();
	ret = read_product_name(i2c_buf, I2C_BUF_SIZE);
	if (ret)
		printf("Fail to detect board:%d\n", ret);
	else
		printf("Get board name:%s\n", (char *)i2c_buf);
	pmic_init();
	*/

}

u32 spl_boot_device(void)
{
	return BOOT_DEVICE_NOR;
}

void spl_board_init(void)
{
}
