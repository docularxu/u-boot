// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2025-2026, RISCStar Ltd.
 */

#include <asm/io.h>
#include <binman_sym.h>
#include <clk.h>
#include <clk-uclass.h>
#include <configs/k1.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <dt-bindings/pinctrl/k1-pinctrl.h>
#include <i2c.h>
#include <linux/delay.h>
#include <power/regulator.h>
#include <spl.h>
#include <tlv_eeprom.h>

#define I2C_PIN_CONFIG(x)       ((x) | EDGE_NONE | PULL_UP | PAD_1V8_DS2)
#define I2C_BUF_SIZE		64

#define MFP_GPIO_84		0xd401e154
#define MFP_GPIO_85		0xd401e158

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
	u8 eeprom_data[TLV_TOTAL_LEN_MAX];
	struct tlvinfo_header *tlv_hdr;
	struct tlvinfo_tlv *tlv_entry;
	int ret, i;

	if (!name || size < 0)
		return -EINVAL;
	ret = read_tlvinfo_tlv_eeprom(eeprom_data, &tlv_hdr,
				      &tlv_entry, i);
	if (ret)
		return ret;
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
		i += tlv_entry->length + sizeof(struct tlvinfo_tlv);
	}
	return -ENOENT;
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
		panic("Fail to detect system-controller@d4015000\n");
	ret = uclass_get_device_by_name(UCLASS_CLK, "system-controller@d4015000", &dev);
	if (ret)
		panic("Fail to detect system-controller@d4015000\n");
	ret = uclass_get_device_by_name(UCLASS_CLK, "system-controller@d4282800", &dev);
	if (ret)
		panic("Fail to detect system-controller@d4282800\n");

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
	ret = regulator_get_value(dev);
	printf("vdd_core, value:%d\n", ret);
}

static void set_vdd_1v8(void)
{
	struct udevice *dev;
	int ret;

	ret = regulator_get_by_platname("vdd_1v8", &dev);
	if (ret)
		printf("Fail to detect vdd_1v8 (%d)\n", ret);
	ret = regulator_get_value(dev);
	printf("vdd_1v8, value:%d\n", ret);
}

static void set_vdd_mmc(void)
{
	struct udevice *dev;
	int ret;

	ret = regulator_get_by_platname("vdd_1v8_mmc", &dev);
	if (ret)
		printf("Fail to detect vdd_1v8_mmc (%d)\n", ret);
	ret = regulator_get_value(dev);
	printf("vdd_1v8_mmc, value:%d\n", ret);
}

void pmic_init(void)
{
	struct udevice *pmic_dev;
	int ret;

	ret = uclass_get_device(UCLASS_PMIC, 0, &pmic_dev);
	if (ret)
		panic("Fail to detect PMIC (%d)\n", ret);
	set_vdd_1v8();
	set_vdd_mmc();
	set_vdd_core();
}

void ddr_init(void)
{
	void __iomem *addr;

	addr = (void __iomem *)(CONFIG_SPL_TEXT_BASE);
	addr += CONFIG_SPL_DDR_FIRMWARE_OFFSET;
	// verify DDR firmware header
	printf("[0x%x]:0x%x\n", (uint)(u64)addr, readl(addr));
}

void board_init_f(ulong dummy)
{
	u8 i2c_buf[I2C_BUF_SIZE];
	int ret;

	ret = spl_early_init();
	if (ret)
		panic("spl_early_init() failed:%d\n", ret);

	riscv_cpu_setup();

	reset_early_init();
	clk_early_init();
	serial_early_init();

	preloader_console_init();

	i2c_early_init();
	ddr_init();
	ret = read_product_name(i2c_buf, I2C_BUF_SIZE);
	if (ret)
		printf("Fail to detect board:%d\n", ret);
	else
		printf("Get board name:%s\n", (char *)i2c_buf);
	pmic_init();
}

u32 spl_boot_device(void)
{
	return BOOT_DEVICE_NOR;
}

void spl_board_init(void)
{
}
