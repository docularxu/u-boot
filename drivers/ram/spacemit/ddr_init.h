#ifndef __DDR_INIT_H_
#define __DDR_INIT_H_

#define MC_CH0_BASE			0x200
#define	MC_CH0_PHY_BASE			0x1000
#define	ANALOG_CONTROL_OFFSET		0x0
#define	OTHER_CONTROL_OFFSET		0x10000
#define	TRAINING_CONTROL_OFFSET		0x18000

#define	subPHY_A_OFFSET			0x0
#define	subPHY_B_OFFSET			0x200
#define	subPHY_C_OFFSET			0x400
#define	subPHY_D_OFFSET			0x600

#define	CS0_OFFSET			0x0
#define	CS1_OFFSET			0x100

#define	CAPHY_OFFSET			0x2000
#define	DATAPHY0_OFFSET			0x0
#define	DATAPHY1_OFFSET			0x1000
#define	COMMON_OFFSET			0x3000
#define	FREQ_POINT_OFFSET		0x4000

#define TOP_EXT_CLK_DIV_OFFSET		0
#define TOP_EXT_CLK_SEL_OFFSET		1
#define TOP_PLL2_DIV_OFFSET		2
#define TOP_PLL1_DIV_OFFSET		4
#define TOP_PLL1_2_SEL_OFFSET		6
#define TOP_PLL2_EN_OFFSET		8
#define TOP_PLL1_EN_OFFSET		9
#define TOP_DDRPHY1_EN_OFFSET		31
#define TOP_DDRPHY0_EN_OFFSET		30
#define TOP_DCLK_BYPASS_FC_REQ_OFFSET	23
#define TOP_DCLK_BYPASS_CLK_EN_OFFSET	22
#define TOP_DCLK_BYPASS_RST_OFFSET	21
#define TOP_DCLK_BYPASS_SEL_OFFSET	19
#define TOP_DCLK_BYPASS_DIV_OFFSET	16
#define TOP_MC_REG_TABLE_EN_OFFSET	10
#define TOP_FREQ_PLL_CHG_MODE_OFFSET	9
#define TOP_MC_REQ_TABLE_NUM_OFFSET	3
#define TOP_AP_ALLOW_SPD_CHG		18
#define TOP_DDR_FREQ_CHG_REQ		22

#define DDR_TYPE_SIZE			64

// emu for device density
typedef enum {
	BANK_2 = 0,
	BANK_4,
	BANK_8,
	BANK_RESERVED,
} bank_num;

typedef enum {
	ROW_11 = 1,
	ROW_12,
	ROW_13,
	ROW_14,
	ROW_15,
	ROW_16,
	ROW_17,
	ROW_18,
} row_num;

typedef enum {
	COL_8 = 1,
	COL_9,
	COL_10,
	COL_11,
	COL_12,
} col_num;

typedef enum {
	IO_X16 = 0,
	IO_X8,
} io_width;

// emu for IO parameter
typedef enum {
	SAMSUNG = 0x1,
	SK_HYNIX = 0x6,
	MICRON = 0xff,
} manufacturer_id;

typedef enum {
	R_240 = 1,
	R_120,
	R_80,
	R_60,
	R_48,
	R_40,
} tx_ds_odt_rx_odt;

typedef enum {
	VOH_0P6 = 0x0,
	VOH_0P5,
} pu_cal;

typedef enum {
	LPDDR4X = 0x0,
	LPDDR4,
} device_type;

struct ddr_config {
	void __iomem	*base;
	u32		data_rate;
	u32		cs_num;
	u32		tx_odt;
	u8		type[DDR_TYPE_SIZE];
};

extern uint32_t lpddr4_silicon_init(struct ddr_config *cfg);

#endif /* __DDR_INIT_H_ */
