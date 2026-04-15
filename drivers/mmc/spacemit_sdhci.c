// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2026 RISCstar Ltd.
 */

#include <clk.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <linux/delay.h>
#include <malloc.h>
#include <mapmem.h>
#include <sdhci.h>
#include <reset-uclass.h>

#include <power/regulator.h>

#define SPACEMIT_SDHC_MIN_FREQ			200000

#define MMC1_IO_V18EN			0x04
#define AKEY_ASFAR			0xBABA
#define AKEY_ASSAR			0xEB10

#define SDHC_OP_EXT_REG				0x108
#define OVRRD_CLK_OEN				0x0800
#define FORCE_CLK_ON				0x1000

#define SDHC_MMC_CTRL_REG			0x114
#define MISC_INT_EN				0x0002
#define MISC_INT				0x0004
#define ENHANCE_STROBE_EN			0x0100
#define MMC_HS400				0x0200
#define MMC_HS200				0x0400
#define MMC_CARD_MODE				0x1000

#define SDHC_TX_CFG_REG				0x11C
#define TX_INT_CLK_SEL				0x40000000
#define TX_MUX_SEL				0x80000000

#define SDHC_PHY_CTRL_REG			0x160
#define PHY_FUNC_EN				0x0001
#define PHY_PLL_LOCK				0x0002
#define HOST_LEGACY_MODE			0x80000000

#define SDHC_PHY_PADCFG_REG			0x178
#define RX_BIAS_CTRL_SHIFT			0x5
#define PHY_DRIVE_SEL_SHIFT			0x0
#define PHY_DRIVE_SEL_MASK			0x7
#define PHY_DRIVE_SEL_DEFAULT			0x4

#define SDHCI_CTRL_ADMA2_LEN_MODE		BIT(10)
#define SDHCI_CTRL_CMD23_ENABLE			BIT(11)
#define SDHCI_CTRL_HOST_VERSION_4_ENABLE	BIT(12)
#define SDHCI_CTRL_ADDRESSING			BIT(13)
#define SDHCI_CTRL_ASYNC_INT_ENABLE		BIT(14)

#define SDHCI_CLOCK_PLL_EN			BIT(3)

#define CANDIDATE_WIN_NUM	3
#define SELECT_DELAY_NUM 9
#define WINDOW_1ST 0
#define WINDOW_2ND 1
#define WINDOW_3RD 2

#define RX_TUNING_WINDOW_THRESHOLD 80
#define RX_TUNING_DLINE_REG 0x09
#define TX_TUNING_DLINE_REG 0x00
#define TX_TUNING_DELAYCODE 127

enum window_type {
	LEFT_WINDOW = 0,
	MIDDLE_WINDOW = 1,
	RIGHT_WINDOW = 2,
};

struct tuning_window {
	u8 type;
	u8 min_delay;
	u8 max_delay;
};

struct rx_tuning {
	u8 rx_dline_reg;
	u8 select_delay_num;
	/* 0: biggest window, 1: bigger, 2:  small */
	struct tuning_window windows[CANDIDATE_WIN_NUM];
	u8 select_delay[SELECT_DELAY_NUM];

	u8 window_limit;
};

struct spacemit_sdhci_plat {
	struct mmc_config	cfg;
	struct mmc		mmc;

	u32 aib_mmc1_io_reg;
	u32 apbc_asfar_reg;
	u32 apbc_assar_reg;

	u8 tx_dline_reg;
	u8 tx_delaycode;
	struct rx_tuning rxtuning;
};

struct spacemit_sdhci_priv {
	struct sdhci_host host;
	u32 clk_src_freq;
	u32 phy_module;
};

#if 0
static void spacemit_set_aib_mmc1_io(struct sdhci_host *host, int vol)
{
	void __iomem *aib_mmc1_io;
	void __iomem *apbc_asfar;
	void __iomem *apbc_assar;
	u32 reg;
	struct mmc *mmc = host->mmc;
	struct spacemit_sdhci_plat *plat = dev_get_plat(mmc->dev);

	if (!plat->aib_mmc1_io_reg ||
		!plat->apbc_asfar_reg ||
		!plat->apbc_assar_reg)
		return;

	aib_mmc1_io = map_sysmem((uintptr_t)plat->aib_mmc1_io_reg, sizeof(uintptr_t));
	apbc_asfar = map_sysmem((uintptr_t)plat->apbc_asfar_reg, sizeof(uintptr_t));
	apbc_assar = map_sysmem((uintptr_t)plat->apbc_assar_reg, sizeof(uintptr_t));

	writel(AKEY_ASFAR, apbc_asfar);
	writel(AKEY_ASSAR, apbc_assar);
	reg = readl(aib_mmc1_io);

	switch (vol) {
	case MMC_SIGNAL_VOLTAGE_180:
		reg |= MMC1_IO_V18EN;
		break;
	default:
		reg &= ~MMC1_IO_V18EN;
		break;
	}
	writel(AKEY_ASFAR, apbc_asfar);
	writel(AKEY_ASSAR, apbc_assar);
	writel(reg, aib_mmc1_io);
}
#endif

static void spacemit_sdhci_set_clk_gate(struct sdhci_host *host, int auto_gate)
{
	unsigned int reg;

	reg = sdhci_readl(host, SDHC_OP_EXT_REG);
	if (auto_gate)
		reg &= ~(OVRRD_CLK_OEN | FORCE_CLK_ON);
	else
		reg |= (OVRRD_CLK_OEN | FORCE_CLK_ON);
	sdhci_writel(host, reg, SDHC_OP_EXT_REG);
}

static int spacemit_sdhci_wait_dat0(struct udevice *dev, int state,
				    int timeout_us)
{
	struct mmc *mmc = mmc_get_mmc_dev(dev);
	struct sdhci_host *host = mmc->priv;
	unsigned long timeout = timer_get_us() + timeout_us;
	u32 tmp;
	u32 cmd;

	printf("##%s, %d\n", __func__, __LINE__);
	// readx_poll_timeout is unsuitable because sdhci_readl accepts
	// two arguments
	do {
		tmp = sdhci_readl(host, SDHCI_PRESENT_STATE);
		if (!!(tmp & SDHCI_DATA_0_LVL_MASK) == !!state){
			if (IS_SD(mmc)) {
				cmd = SDHCI_GET_CMD(sdhci_readw(host, SDHCI_COMMAND));
				if ((cmd == SD_CMD_SWITCH_UHS18V) && (mmc->signal_voltage == MMC_SIGNAL_VOLTAGE_180)) {
					/* recover the auto clock */
					spacemit_sdhci_set_clk_gate(host, 1);
				}
			}
			return 0;
		}
	} while (!timeout_us || !time_after(timer_get_us(), timeout));

	return -ETIMEDOUT;
}

#if 0
static void spacemit_sdhci_set_voltage(struct sdhci_host *host)
{
	if (IS_ENABLED(CONFIG_MMC_IO_VOLTAGE)) {
		struct mmc *mmc = (struct mmc *)host->mmc;
		u32 ctrl;

		ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);

		switch (mmc->signal_voltage) {
		case MMC_SIGNAL_VOLTAGE_330:
#if CONFIG_IS_ENABLED(DM_REGULATOR)
			if (mmc->vqmmc_supply) {
				if (regulator_set_value(mmc->vqmmc_supply, 3300000)) {
					pr_err("failed to set vqmmc-voltage to 3.3V\n");
					return;
				}

				if (regulator_set_enable_if_allowed(mmc->vqmmc_supply, true)) {
					pr_err("failed to enable vqmmc-supply\n");
					return;
				}
			}
#endif
			if (IS_SD(mmc)) {
				ctrl &= ~SDHCI_CTRL_VDD_180;
				sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);
			}

			/* Wait for 5ms */
			mdelay(5);

			/* 3.3V regulator output should be stable within 5 ms */
			if (IS_SD(mmc)) {
				if (ctrl & SDHCI_CTRL_VDD_180) {
					pr_err("3.3V regulator output did not become stable\n");
					return;
				}
			}

			break;
		case MMC_SIGNAL_VOLTAGE_180:
#if CONFIG_IS_ENABLED(DM_REGULATOR)
			if (mmc->vqmmc_supply) {
				if (regulator_set_value(mmc->vqmmc_supply, 1800000)) {
					pr_err("failed to set vqmmc-voltage to 1.8V\n");
					return;
				}

				if (regulator_set_enable_if_allowed(mmc->vqmmc_supply, true)) {
					pr_err("failed to enable vqmmc-supply\n");
					return;
				}
			}
#endif
			if (IS_SD(mmc)) {
				ctrl |= SDHCI_CTRL_VDD_180;
				sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);
			}

			/* Wait for 5 ms */
			mdelay(5);

			/* 1.8V regulator output has to be stable within 5 ms */
			if (IS_SD(mmc)) {
				if (!(ctrl & SDHCI_CTRL_VDD_180)) {
					pr_err("1.8V regulator output did not become stable\n");
					return;
				}
			}

			break;
		default:
			/* No signal voltage switch required */
			return;
		}
	}
}
#endif

#if 0
static void spacemit_sdhci_set_control_reg(struct sdhci_host *host)
{
	struct mmc *mmc = host->mmc;
	u32 reg;
	u32 cmd;

	pr_debug("%s: select mode: %s, io voltage: %d\n", host->name,
			mmc_mode_name(mmc->selected_mode), mmc->signal_voltage);

	spacemit_sdhci_set_voltage(host);
	spacemit_set_aib_mmc1_io(host, mmc->signal_voltage);

	if (IS_SD(mmc)) {
		cmd = SDHCI_GET_CMD(sdhci_readw(host, SDHCI_COMMAND));
		if ((cmd == SD_CMD_SWITCH_UHS18V) && (mmc->signal_voltage == MMC_SIGNAL_VOLTAGE_180)) {
			/* disable auto clock */
			spacemit_sdhci_set_clk_gate(host, 0);
		}
	}

	/* according to the SDHC_TX_CFG_REG(0x11c<bit>),
	 * set TX_INT_CLK_SEL to gurantee the hold time
	 * at default speed mode or HS/SDR12/SDR25/SDR50 mode.
	 */
	reg = sdhci_readl(host, SDHC_TX_CFG_REG);
	if ((mmc->selected_mode == MMC_LEGACY) ||
	    (mmc->selected_mode == MMC_HS) ||
	    (mmc->selected_mode == SD_HS) ||
	    (mmc->selected_mode == UHS_SDR12) ||
	    (mmc->selected_mode == UHS_SDR25) ||
	    (mmc->selected_mode == UHS_SDR50)) {
		reg |= TX_INT_CLK_SEL;
	} else {
		reg &= ~TX_INT_CLK_SEL;
	}
	sdhci_writel(host, reg, SDHC_TX_CFG_REG);

	/* set pinctrl state */
//#ifdef CONFIG_PINCTRL
#if 0
	if (mmc->clock >= 200000000)
		pinctrl_select_state(mmc->dev, "fast");
	else if (mmc->bus_width < 4)
		pinctrl_select_state(mmc->dev, "debug");
	else
		pinctrl_select_state(mmc->dev, "default");
#endif

	if ((mmc->selected_mode == MMC_HS_200) ||
	    (mmc->selected_mode == MMC_HS_400) ||
	    (mmc->selected_mode == MMC_HS_400_ES)) {
		reg = sdhci_readw(host, SDHC_MMC_CTRL_REG);
		reg |= (mmc->selected_mode == MMC_HS_200) ? MMC_HS200 : MMC_HS400;
		sdhci_writew(host, reg, SDHC_MMC_CTRL_REG);
	} else {
		reg = sdhci_readw(host, SDHC_MMC_CTRL_REG);
		reg &= ~(MMC_HS200 | MMC_HS400 | ENHANCE_STROBE_EN);
		sdhci_writew(host, reg, SDHC_MMC_CTRL_REG);
	}

	sdhci_set_uhs_timing(host);
	return;
}
static const struct sdhci_ops spacemit_ops = {
	.set_control_reg = &sdhci_set_control_reg,
};
#endif

static struct dm_mmc_ops spacemit_mmc_ops __attribute__((section(".data")));

/*
static void sdhci_do_enable_v4_mode(struct udevice *dev)
{
	struct sdhci_host *host = dev_get_priv(dev);
	u16 ctrl2;
	ctrl2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	ctrl2 |= SDHCI_CTRL_ADMA2_LEN_MODE
			| SDHCI_CTRL_CMD23_ENABLE
			| SDHCI_CTRL_HOST_VERSION_4_ENABLE
			| SDHCI_CTRL_ADDRESSING
			| SDHCI_CTRL_ASYNC_INT_ENABLE;

	sdhci_writew(host, ctrl2, SDHCI_HOST_CONTROL2);
}
*/

static int spacemit_sdhci_probe(struct udevice *dev)
{
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct spacemit_sdhci_plat *plat = dev_get_plat(dev);
	struct sdhci_host *host = dev_get_priv(dev);
	struct reset_ctl_bulk reset_bulk;
	struct clk_bulk clks;
	int ret;
	uint32_t value;
	struct dm_mmc_ops *mmc_driver_ops = (struct dm_mmc_ops *)dev->driver->ops;

	ret = reset_get_bulk(dev, &reset_bulk);
	if (!ret)
		reset_deassert_bulk(&reset_bulk);

	ret = clk_get_bulk(dev, &clks);
	if (ret) {
		dev_err(dev, "Fail to get bulk clks\n");
		return ret;
	}
	{
		ulong rate;
		for (int i = 0; i < clks.count; i++) {
			rate = clk_get_rate(&clks.clks[i]);
			printf("%s, %d, i:%d, rate:%ld\n", __func__, __LINE__, i, rate);
		}
	}
	ret = clk_enable_bulk(&clks);
	if (ret) {
		dev_err(dev, "Fail to enable bulk clks\n");
		return ret;
	}

	//host->name = dev->name;
	//host->ioaddr = dev_read_addr_ptr(dev);
	//host->ops = &spacemit_ops;
	host->quirks = SDHCI_QUIRK_WAIT_SEND_CMD;
	spacemit_mmc_ops = sdhci_ops;

	ret = mmc_of_parse(dev, &plat->cfg);
	if (ret)
		return ret;

	host->mmc = &plat->mmc;
	host->mmc->dev = dev;
	host->max_clk = plat->cfg.f_max;

	mmc_driver_ops->wait_dat0 = spacemit_sdhci_wait_dat0;
	ret = sdhci_setup_cfg(&plat->cfg, host, plat->cfg.f_max,
			      SPACEMIT_SDHC_MIN_FREQ);
	if (ret)
		return ret;

	upriv->mmc = host->mmc;
	host->mmc->priv = host;

	ret = sdhci_probe(dev);
	if (ret)
		return ret;
	if (dev_read_bool(dev, "no-sd")) {
		/* mmc card mode */
		value = sdhci_readl(host, SDHC_MMC_CTRL_REG);
		value |= MMC_CARD_MODE;
		sdhci_writel(host, value, SDHC_MMC_CTRL_REG);
		/* use phy func mode */
		value = sdhci_readl(host, SDHC_PHY_CTRL_REG);
		value |= (PHY_FUNC_EN | PHY_PLL_LOCK);
		sdhci_writel(host, value, SDHC_PHY_CTRL_REG);

		value = sdhci_readl(host, SDHC_PHY_PADCFG_REG);
		value |= (1 << RX_BIAS_CTRL_SHIFT);
		sdhci_writel(host, value, SDHC_PHY_PADCFG_REG);
		printf("%s: use mmc phy func.\n", host->name);
	} else {
		pr_debug("%s: not support phy module.\n", host->name);
		value = sdhci_readl (host, SDHC_TX_CFG_REG);
		value |= TX_INT_CLK_SEL;
		sdhci_writel (host, value, SDHC_TX_CFG_REG);
	}
	value = sdhci_readl(host, SDHC_MMC_CTRL_REG);
	value &= ~ENHANCE_STROBE_EN;
	sdhci_writel(host, value, SDHC_MMC_CTRL_REG);

	/*
	enable v4 should execute after sdhci_probe, because sdhci reset would
	clear the SDHCI_HOST_CONTROL2 register.
	*/
	//sdhci_do_enable_v4_mode(dev);
	return 0;
}

static int spacemit_sdhci_of_to_plat(struct udevice *dev)
{
	struct spacemit_sdhci_plat *plat = dev_get_plat(dev);
	struct spacemit_sdhci_priv *priv = dev_get_priv(dev);
	struct sdhci_host *host = &priv->host;
	int ret = 0;

	host->name = dev->name;
	host->ioaddr = (void *)devfdt_get_addr(dev);
	priv->phy_module = dev_read_u32_default(dev, "sdh-phy-module", 0);
	//priv->clk_src_freq = dev_read_u32_default(dev, "clk-src-freq", SDHC_DEFAULT_MAX_CLOCK);

	plat->aib_mmc1_io_reg = dev_read_u32_default(dev, "spacemit,aib_mmc1_io_reg", 0);
	plat->apbc_asfar_reg = dev_read_u32_default(dev, "spacemit,apbc_asfar_reg", 0);
	plat->apbc_assar_reg = dev_read_u32_default(dev, "spacemit,apbc_assar_reg", 0);

	/* read rx tuning dline_reg */
	plat->rxtuning.rx_dline_reg = dev_read_u32_default(dev, "spacemit,rx_dline_reg", RX_TUNING_DLINE_REG);
	/* read rx tuning window limit */
	plat->rxtuning.window_limit = dev_read_u32_default(dev, "spacemit,rx_tuning_limit", RX_TUNING_WINDOW_THRESHOLD);

	/* tx tuning dline_reg */
	plat->tx_dline_reg = dev_read_u32_default(dev, "spacemit,tx_dline_reg", TX_TUNING_DLINE_REG);
	/* tx tuning delaycode */
	plat->tx_delaycode = dev_read_u32_default(dev, "spacemit,tx_delaycode", TX_TUNING_DELAYCODE);

	ret = mmc_of_parse(dev, &plat->cfg);

	return ret;
}

static int spacemit_sdhci_bind(struct udevice *dev)
{
	struct spacemit_sdhci_plat *plat = dev_get_plat(dev);

	return sdhci_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id spacemit_sdhci_ids[] = {
	{ .compatible = "spacemit,k1-sdhci" },
	{ }
};

U_BOOT_DRIVER(spacemit_sdhci_drv) = {
	.name		= "spacemit_sdhci",
	.id		= UCLASS_MMC,
	.of_match	= spacemit_sdhci_ids,
	.of_to_plat	= spacemit_sdhci_of_to_plat,
	.ops		= &spacemit_mmc_ops,
	.bind		= spacemit_sdhci_bind,
	.probe		= spacemit_sdhci_probe,
	.priv_auto	= sizeof(struct spacemit_sdhci_priv),
	.plat_auto	= sizeof(struct spacemit_sdhci_plat),
};
