#ifndef SDHCI_XENON_H_
#define SDHCI_XENON_H_
#define XENON_SYS_CFG_INFO			0x0104
#define XENON_SLOT_TYPE_SDIO_SHIFT		24
#define XENON_NR_SUPPORTED_SLOT_MASK		0x7
#define XENON_SYS_OP_CTRL			0x0108
#define XENON_AUTO_CLKGATE_DISABLE_MASK		BIT(20)
#define XENON_SDCLK_IDLEOFF_ENABLE_SHIFT	8
#define XENON_SLOT_ENABLE_SHIFT			0
#define XENON_SYS_EXT_OP_CTRL			0x010C
#define XENON_MASK_CMD_CONFLICT_ERR		BIT(8)
#define XENON_SLOT_OP_STATUS_CTRL		0x0128
#define XENON_TUN_CONSECUTIVE_TIMES_SHIFT	16
#define XENON_TUN_CONSECUTIVE_TIMES_MASK	0x7
#define XENON_TUN_CONSECUTIVE_TIMES		0x4
#define XENON_TUNING_STEP_SHIFT			12
#define XENON_TUNING_STEP_MASK			0xF
#define XENON_TUNING_STEP_DIVIDER		BIT(6)
#define XENON_SLOT_EMMC_CTRL			0x0130
#define XENON_ENABLE_RESP_STROBE		BIT(25)
#define XENON_ENABLE_DATA_STROBE		BIT(24)
#define XENON_SLOT_RETUNING_REQ_CTRL		0x0144
#define XENON_RETUNING_COMPATIBLE		0x1
#define XENON_SLOT_EXT_PRESENT_STATE		0x014C
#define XENON_DLL_LOCK_STATE			0x1
#define XENON_SLOT_DLL_CUR_DLY_VAL		0x0150
#define XENON_TMR_RETUN_NO_PRESENT		0xF
#define XENON_DEF_TUNING_COUNT			0x9
#define XENON_DEFAULT_SDCLK_FREQ		400000
#define XENON_LOWEST_SDCLK_FREQ			100000
#define XENON_CTRL_HS200			0x5
#define XENON_CTRL_HS400			0x6
enum xenon_variant {
	XENON_A3700,
	XENON_AP806,
	XENON_AP807,
	XENON_CP110
};
struct xenon_priv {
	unsigned char	tuning_count;
	u8		sdhc_id;
	unsigned int	init_card_type;
	unsigned char	bus_width;
	unsigned char	timing;
	unsigned int	clock;
	struct clk      *axi_clk;
	int		phy_type;
	void		*phy_params;
	struct xenon_emmc_phy_regs *emmc_phy_regs;
	bool restore_needed;
	enum xenon_variant hw_version;
};
int xenon_phy_adj(struct sdhci_host *host, struct mmc_ios *ios);
int xenon_phy_parse_params(struct device *dev,
			   struct sdhci_host *host);
void xenon_soc_pad_ctrl(struct sdhci_host *host,
			unsigned char signal_voltage);
#endif
