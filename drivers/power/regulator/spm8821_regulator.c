// SPDX-License-Identifier: GPL-2.0+

//#include <common.h>
#include <dm.h>
#include <dm/lists.h>
#include <errno.h>
#include <log.h>
//#include <power/spacemit/spacemit_pmic.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <power/spm8821.h>

DECLEAR_SPM8821_REGULATOR_MATCH_DATA;

struct spm8821_reg_info {
	uint min_uv;
	uint step_uv;
	u8 vsel_reg;
	u8 vsel_sleep_reg;
	u8 config_reg;
	u8 vsel_mask;
	u8 min_sel;
	u8 max_sel;
};

static const struct spm8821_reg_info spm8821_bucks[] = {
	/* BUCK 1 */
	{  500000,  5000, SPM8821_BUCK_VSEL(1), SPM8821_BUCK_SVSEL(1), SPM8821_BUCK_CTRL(1), BUCK_VSEL_MASK, 0x00, 0xaa },
	{ 1375000, 25000, SPM8821_BUCK_VSEL(1), SPM8821_BUCK_SVSEL(1), SPM8821_BUCK_CTRL(1), BUCK_VSEL_MASK, 0xab, 0xfe },
	/* BUCK 2 */
	{  500000,  5000, SPM8821_BUCK_VSEL(2), SPM8821_BUCK_SVSEL(2), SPM8821_BUCK_CTRL(2), BUCK_VSEL_MASK, 0x00, 0xaa },
	{ 1375000, 25000, SPM8821_BUCK_VSEL(2), SPM8821_BUCK_SVSEL(2), SPM8821_BUCK_CTRL(2), BUCK_VSEL_MASK, 0xab, 0xfe },
	/* BUCK 3 */
	{  500000,  5000, SPM8821_BUCK_VSEL(3), SPM8821_BUCK_SVSEL(3), SPM8821_BUCK_CTRL(3), BUCK_VSEL_MASK, 0x00, 0xaa },
	{ 1375000, 25000, SPM8821_BUCK_VSEL(3), SPM8821_BUCK_SVSEL(3), SPM8821_BUCK_CTRL(3), BUCK_VSEL_MASK, 0xab, 0xfe },
	/* BUCK 4 */
	{  500000,  5000, SPM8821_BUCK_VSEL(4), SPM8821_BUCK_SVSEL(4), SPM8821_BUCK_CTRL(4), BUCK_VSEL_MASK, 0x00, 0xaa },
	{ 1375000, 25000, SPM8821_BUCK_VSEL(4), SPM8821_BUCK_SVSEL(4), SPM8821_BUCK_CTRL(4), BUCK_VSEL_MASK, 0xab, 0xfe },
	/* BUCK 5 */
	{  500000,  5000, SPM8821_BUCK_VSEL(5), SPM8821_BUCK_SVSEL(5), SPM8821_BUCK_CTRL(5), BUCK_VSEL_MASK, 0x00, 0xaa },
	{ 1375000, 25000, SPM8821_BUCK_VSEL(5), SPM8821_BUCK_SVSEL(5), SPM8821_BUCK_CTRL(5), BUCK_VSEL_MASK, 0xab, 0xfe },
	/* BUCK 6 */
	{  500000,  5000, SPM8821_BUCK_VSEL(6), SPM8821_BUCK_SVSEL(6), SPM8821_BUCK_CTRL(6), BUCK_VSEL_MASK, 0x00, 0xaa },
	{ 1375000, 25000, SPM8821_BUCK_VSEL(6), SPM8821_BUCK_SVSEL(6), SPM8821_BUCK_CTRL(6), BUCK_VSEL_MASK, 0xab, 0xfe },
};

static const struct spm8821_reg_info spm8821_aldos[] = {
	/* ALDO 1 */
	{ 500000, 25000, SPM8821_ALDO_VOLT(1), SPM8821_ALDO_SVOLT(1), SPM8821_ALDO_CTRL(1), ALDO_VSEL_MASK, 0x0b, 0x7f },
	/* ALDO 2 */
	{ 500000, 25000, SPM8821_ALDO_VOLT(2), SPM8821_ALDO_SVOLT(2), SPM8821_ALDO_CTRL(2), ALDO_VSEL_MASK, 0x0b, 0x7f },
	/* ALDO 3 */
	{ 500000, 25000, SPM8821_ALDO_VOLT(3), SPM8821_ALDO_SVOLT(3), SPM8821_ALDO_CTRL(3), ALDO_VSEL_MASK, 0x0b, 0x7f },
	/* ALDO 4 */
	{ 500000, 25000, SPM8821_ALDO_VOLT(4), SPM8821_ALDO_SVOLT(4), SPM8821_ALDO_CTRL(4), ALDO_VSEL_MASK, 0x0b, 0x7f },
};

static const struct spm8821_reg_info spm8821_dldos[] = {
	/* DLDO 1 */
	{ 500000, 25000, SPM8821_DLDO_VOLT(1), SPM8821_DLDO_SVOLT(1), SPM8821_DLDO_CTRL(1), ALDO_VSEL_MASK, 0x0b, 0x7f },
	/* DLDO 2 */
	{ 500000, 25000, SPM8821_DLDO_VOLT(2), SPM8821_DLDO_SVOLT(2), SPM8821_DLDO_CTRL(2), ALDO_VSEL_MASK, 0x0b, 0x7f },
	/* DLDO 3 */
	{ 500000, 25000, SPM8821_DLDO_VOLT(3), SPM8821_DLDO_SVOLT(3), SPM8821_DLDO_CTRL(3), ALDO_VSEL_MASK, 0x0b, 0x7f },
	/* DLDO 4 */
	{ 500000, 25000, SPM8821_DLDO_VOLT(4), SPM8821_DLDO_SVOLT(4), SPM8821_DLDO_CTRL(4), ALDO_VSEL_MASK, 0x0b, 0x7f },
	/* DLDO 5 */
	{ 500000, 25000, SPM8821_DLDO_VOLT(5), SPM8821_DLDO_SVOLT(5), SPM8821_DLDO_CTRL(5), ALDO_VSEL_MASK, 0x0b, 0x7f },
	/* DLDO 6 */
	{ 500000, 25000, SPM8821_DLDO_VOLT(6), SPM8821_DLDO_SVOLT(6), SPM8821_DLDO_CTRL(6), ALDO_VSEL_MASK, 0x0b, 0x7f },
	/* DLDO 7 */
	{ 500000, 25000, SPM8821_DLDO_VOLT(7), SPM8821_DLDO_SVOLT(7), SPM8821_DLDO_CTRL(7), ALDO_VSEL_MASK, 0x0b, 0x7f },
};

static const struct spm8821_reg_info *get_buck_reg(struct udevice *pmic,
						   int idx, int uvolt)
{
	if (idx < 0)
		return NULL;
	if (uvolt < 1375000)
		return &spm8821_bucks[(idx - 1) * 2 + 0];
	return &spm8821_bucks[(idx - 1) * 2 + 1];
}

static const struct spm8821_reg_info *get_aldo_reg(struct udevice *pmic,
						   int idx, int uvolt)
{
	return &spm8821_aldos[idx];
}

static const struct spm8821_reg_info *get_dldo_reg(struct udevice *pmic,
						   int idx, int uvolt)
{
	return &spm8821_dldos[idx];
}

static int buck_get_value(struct udevice *dev)
{
	struct spm8821_regulator_pdata *priv = dev_get_priv(dev);
	const struct dm_pmic_ops *ops = device_get_ops(dev->parent);
	const struct spm8821_reg_info *info;
	uint val;
	int ret;

	if (!ops || !ops->read)
		return -ENOSYS;

	info = get_buck_reg(dev->parent, dev->driver_data, 0);
	if (!info)
		return -ENOENT;
	ret = pmic_reg_read(dev->parent, info->vsel_reg);
	if (ret < 0)
		return ret;
	val = ret & info->vsel_mask;
	while (val > info->max_sel)
		info++;

	return info->min_uv + (val - info->min_sel) * info->step_uv;
}

static int buck_set_value(struct udevice *dev, int uvolt)
{
	struct spm8821_regulator_pdata *priv = dev_get_priv(dev);
	const struct dm_pmic_ops *ops = device_get_ops(dev->parent);
	const struct spm8821_reg_info *info;
	uint val;
	int ret;

	if (!ops || !ops->write)
		return -ENOSYS;

	info = get_buck_reg(dev->parent, dev->driver_data, uvolt);
	if (!info)
		return -ENOENT;
	val = (uvolt - info->min_uv);
	val = val / info->step_uv;
	val += info->min_sel;
	ret = pmic_reg_write(dev->parent, info->vsel_reg, val);
	if (ret < 0)
		return ret;
	return 0;
}

static int buck_set_enable(struct udevice *dev, bool enable)
{
	struct spm8821_regulator_pdata *priv = dev_get_priv(dev);
	const struct dm_pmic_ops *ops = device_get_ops(dev->parent);
	const struct spm8821_reg_info *info;
	uint val;
	int ret;

	info = get_buck_reg(dev->parent, dev->driver_data, 0);
	if (!info)
		return -ENOENT;

	val = (unsigned int)ret;
	val &= 1;

	if (enable == val)
		return 0;

	val = enable;

	ret = pmic_clrsetbits(dev->parent, info->config_reg, 1, val);
	if (ret < 0)
		return ret;

	return 0;
}

static int spm8821_buck_probe(struct udevice *dev)
{
	struct dm_regulator_uclass_plat *uc_pdata;

	printf("%s: name:%s\n", __func__, dev->name);
	uc_pdata = dev_get_uclass_plat(dev);

	uc_pdata->type = REGULATOR_TYPE_BUCK;
	uc_pdata->mode_count = 0;

	return 0;
}

static const struct dm_regulator_ops spm8821_buck_ops = {
	.get_value  = buck_get_value,
	.set_value  = buck_set_value,
	.set_enable = buck_set_enable,
	/*
	.set_suspend_value = buck_set_suspend_value,
	.get_suspend_value = buck_get_suspend_value,
	.get_enable = buck_get_enable,
	.set_suspend_enable = buck_set_suspend_enable,
	.get_suspend_enable = buck_get_suspend_enable,
	*/
};

U_BOOT_DRIVER(spm8821_buck) = {
	.name	= "spm8821_buck",
	.id	= UCLASS_REGULATOR,
	.ops	= &spm8821_buck_ops,
	.probe	= spm8821_buck_probe,
};

#if 0
static int spm8821_ldo_get_value(struct udevice *dev)
{
	int buck = dev->driver_data - 1;
	const struct pm8xx_buck_desc *info = get_ldo_reg(dev->parent, buck);
	int mask = info->vsel_msk;
	int ret;
	unsigned int val;

	if (info == NULL)
		return -ENOSYS;

	ret = pmic_reg_read(dev->parent, info->vsel_reg);
	if (ret < 0)
		return ret;

	val = ret & mask;

	val >>= ffs(mask) - 1;

	return regulator_desc_list_voltage_linear_range(info, val);
}

static const struct dm_regulator_ops spm8821_ldo_ops = {
	.get_value	= spm8821_ldo_get_value,
	.set_value	= spm8821_ldo_set_value,
	.get_enable	= spm8821_get_enable,
	.set_enable	= spm8821_set_enable,
};
#endif

U_BOOT_DRIVER(spm8821_ldo) = {
	.name		= SPM8821_LDO_DRIVER,
	.id		= UCLASS_REGULATOR,
	//.ops		= &spm8821_ldo_ops,
	//.plat_auto	= sizeof(struct spm8821_regulator_pdata),
	//.of_to_plat	= spm8821_regulator_of_to_plat,
	//.priv_auto	= sizeof(struct spm8821_regulator_pdata),
};
/*
U_BOOT_DRIVER(spacemit_spm8821) = {
	.name = "spacemit_spm8821",
	.id = UCLASS_PMIC,
	.of_match = spm8821_ids,
#if CONFIG_IS_ENABLED(PMIC_CHILDREN)
	.bind = spm8821_bind,
#endif
	.priv_auto	  = sizeof(struct spm8821_priv),
	.probe = spm8821_probe,
	.ops = &spm8821_ops,
};
*/
