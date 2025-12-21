// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025-2026 RISCStar Ltd.
 */

#include <dm.h>
#include <power/pmic.h>
#include <power/spm8821.h>

static int pmic_spm8821_reg_count(struct udevice *dev)
{
	return SPM8821_MAX_REGS;
}

static int pmic_spm8821_write(struct udevice *dev, uint reg, const u8 *buffer,
			       int len)
{
	int ret;

	ret = dm_i2c_write(dev, reg, buffer, len);
	if (ret)
		pr_err("%s write error on register %02x\n", dev->name, reg);

	return ret;
}

static int pmic_spm8821_read(struct udevice *dev, uint reg, u8 *buffer,
			      int len)
{
	int ret;

	ret = dm_i2c_read(dev, reg, buffer, len);
	if (ret)
		pr_err("%s read error on register %02x\n", dev->name, reg);

	return ret;
}

static const struct pmic_child_info spm8821_children_info[] = {
	{ .prefix = "DCDC_REG",		.driver = SPM8821_BUCK_DRIVER },
	{ .prefix = "EDCDC_REG",	.driver = SPM8821_BUCK_DRIVER },
	//{ .prefix = "LDO_REG",		.driver = SPM8821_LDO_DRIVER },
	{ .prefix = "ALDO_REG",		.driver = SPM8821_LDO_DRIVER },
	{ .prefix = "DLDO_REG",		.driver = SPM8821_LDO_DRIVER },
	{ .prefix = "SWITCH_REG",	.driver = SPM8821_SWITCH_DRIVER },
	{ .prefix = "PMIC_WDT",		.driver = SPM8821_WDT_DRIVER },
	{ },
};

static int pmic_spm8821_bind(struct udevice *dev)
{
	const struct pmic_child_info *spm8821_children_info =
			(struct pmic_child_info *)dev_get_driver_data(dev);
	ofnode regulators_node;
	int children, ret;

	/*
	   // TODO: append watchdog driver
	if (IS_ENABLED(CONFIG_SYSRESET_TPS65910)) {
		ret = device_bind_driver(dev, TPS65910_RST_DRIVER,
					 "sysreset", NULL);
		if (ret) {
			log_err("cannot bind SYSRESET (ret = %d)\n", ret);
			return ret;
		}
	}
	*/

	regulators_node = dev_read_subnode(dev, "regulators");
	if (!ofnode_valid(regulators_node)) {
		debug("%s regulators subnode not found\n", dev->name);
		return -EINVAL;
	}

	children = pmic_bind_children(dev, regulators_node,
				      spm8821_children_info);
	if (!children)
		debug("%s has no children (regulators)\n", dev->name);

	return 0;
}

static int pmic_spm8821_probe(struct udevice *dev)
{
	printf("%s, %d\n", __func__, __LINE__);
	return 0;
}

static struct dm_pmic_ops pmic_spm8821_ops = {
	.reg_count	= pmic_spm8821_reg_count,
	.read		= pmic_spm8821_read,
	.write		= pmic_spm8821_write,
};

static const struct udevice_id pmic_spm8821_match[] = {
	{
		.compatible = "spacemit,spm8821",
		.data = (ulong)&spm8821_children_info,
	}, {
		/* sentinel */
	}
};

U_BOOT_DRIVER(pmic_spm8821) = {
	.name		= "pmic_spm8821",
	.id		= UCLASS_PMIC,
	.of_match	= pmic_spm8821_match,
	.bind		= pmic_spm8821_bind,
	.probe		= pmic_spm8821_probe,
	.ops		= &pmic_spm8821_ops,
	//.priv_auto	= sizeof(struct spm8821_pdata),
};
