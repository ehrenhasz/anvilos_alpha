
 

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/of_address.h>

#include "sdhci-pltfm.h"
#include "sdhci-xenon.h"

 
#define XENON_EMMC_5_0_PHY_REG_BASE		0x0160
 
#define XENON_EMMC_PHY_REG_BASE			0x0170

#define XENON_EMMC_PHY_TIMING_ADJUST		XENON_EMMC_PHY_REG_BASE
#define XENON_EMMC_5_0_PHY_TIMING_ADJUST	XENON_EMMC_5_0_PHY_REG_BASE
#define XENON_TIMING_ADJUST_SLOW_MODE		BIT(29)
#define XENON_TIMING_ADJUST_SDIO_MODE		BIT(28)
#define XENON_SAMPL_INV_QSP_PHASE_SELECT	BIT(18)
#define XENON_SAMPL_INV_QSP_PHASE_SELECT_SHIFT	18
#define XENON_PHY_INITIALIZAION			BIT(31)
#define XENON_WAIT_CYCLE_BEFORE_USING_MASK	0xF
#define XENON_WAIT_CYCLE_BEFORE_USING_SHIFT	12
#define XENON_FC_SYNC_EN_DURATION_MASK		0xF
#define XENON_FC_SYNC_EN_DURATION_SHIFT		8
#define XENON_FC_SYNC_RST_EN_DURATION_MASK	0xF
#define XENON_FC_SYNC_RST_EN_DURATION_SHIFT	4
#define XENON_FC_SYNC_RST_DURATION_MASK		0xF
#define XENON_FC_SYNC_RST_DURATION_SHIFT	0

#define XENON_EMMC_PHY_FUNC_CONTROL		(XENON_EMMC_PHY_REG_BASE + 0x4)
#define XENON_EMMC_5_0_PHY_FUNC_CONTROL		\
	(XENON_EMMC_5_0_PHY_REG_BASE + 0x4)
#define XENON_ASYNC_DDRMODE_MASK		BIT(23)
#define XENON_ASYNC_DDRMODE_SHIFT		23
#define XENON_CMD_DDR_MODE			BIT(16)
#define XENON_DQ_DDR_MODE_SHIFT			8
#define XENON_DQ_DDR_MODE_MASK			0xFF
#define XENON_DQ_ASYNC_MODE			BIT(4)

#define XENON_EMMC_PHY_PAD_CONTROL		(XENON_EMMC_PHY_REG_BASE + 0x8)
#define XENON_EMMC_5_0_PHY_PAD_CONTROL		\
	(XENON_EMMC_5_0_PHY_REG_BASE + 0x8)
#define XENON_REC_EN_SHIFT			24
#define XENON_REC_EN_MASK			0xF
#define XENON_FC_DQ_RECEN			BIT(24)
#define XENON_FC_CMD_RECEN			BIT(25)
#define XENON_FC_QSP_RECEN			BIT(26)
#define XENON_FC_QSN_RECEN			BIT(27)
#define XENON_OEN_QSN				BIT(28)
#define XENON_AUTO_RECEN_CTRL			BIT(30)
#define XENON_FC_ALL_CMOS_RECEIVER		0xF000

#define XENON_EMMC5_FC_QSP_PD			BIT(18)
#define XENON_EMMC5_FC_QSP_PU			BIT(22)
#define XENON_EMMC5_FC_CMD_PD			BIT(17)
#define XENON_EMMC5_FC_CMD_PU			BIT(21)
#define XENON_EMMC5_FC_DQ_PD			BIT(16)
#define XENON_EMMC5_FC_DQ_PU			BIT(20)

#define XENON_EMMC_PHY_PAD_CONTROL1		(XENON_EMMC_PHY_REG_BASE + 0xC)
#define XENON_EMMC5_1_FC_QSP_PD			BIT(9)
#define XENON_EMMC5_1_FC_QSP_PU			BIT(25)
#define XENON_EMMC5_1_FC_CMD_PD			BIT(8)
#define XENON_EMMC5_1_FC_CMD_PU			BIT(24)
#define XENON_EMMC5_1_FC_DQ_PD			0xFF
#define XENON_EMMC5_1_FC_DQ_PU			(0xFF << 16)

#define XENON_EMMC_PHY_PAD_CONTROL2		(XENON_EMMC_PHY_REG_BASE + 0x10)
#define XENON_EMMC_5_0_PHY_PAD_CONTROL2		\
	(XENON_EMMC_5_0_PHY_REG_BASE + 0xC)
#define XENON_ZNR_MASK				0x1F
#define XENON_ZNR_SHIFT				8
#define XENON_ZPR_MASK				0x1F
 
#define XENON_ZNR_DEF_VALUE			0xF
#define XENON_ZPR_DEF_VALUE			0xF

#define XENON_EMMC_PHY_DLL_CONTROL		(XENON_EMMC_PHY_REG_BASE + 0x14)
#define XENON_EMMC_5_0_PHY_DLL_CONTROL		\
	(XENON_EMMC_5_0_PHY_REG_BASE + 0x10)
#define XENON_DLL_ENABLE			BIT(31)
#define XENON_DLL_UPDATE_STROBE_5_0		BIT(30)
#define XENON_DLL_REFCLK_SEL			BIT(30)
#define XENON_DLL_UPDATE			BIT(23)
#define XENON_DLL_PHSEL1_SHIFT			24
#define XENON_DLL_PHSEL0_SHIFT			16
#define XENON_DLL_PHASE_MASK			0x3F
#define XENON_DLL_PHASE_90_DEGREE		0x1F
#define XENON_DLL_FAST_LOCK			BIT(5)
#define XENON_DLL_GAIN2X			BIT(3)
#define XENON_DLL_BYPASS_EN			BIT(0)

#define XENON_EMMC_5_0_PHY_LOGIC_TIMING_ADJUST	\
	(XENON_EMMC_5_0_PHY_REG_BASE + 0x14)
#define XENON_EMMC_5_0_PHY_LOGIC_TIMING_VALUE	0x5A54
#define XENON_EMMC_PHY_LOGIC_TIMING_ADJUST	(XENON_EMMC_PHY_REG_BASE + 0x18)
#define XENON_LOGIC_TIMING_VALUE		0x00AA8977

 
struct xenon_emmc_phy_regs {
	 
	u16 timing_adj;
	 
	u16 func_ctrl;
	 
	u16 pad_ctrl;
	 
	u16 pad_ctrl2;
	 
	u16 dll_ctrl;
	 
	u16 logic_timing_adj;
	 
	u32 dll_update;
	 
	u32 logic_timing_val;
};

static const char * const phy_types[] = {
	"emmc 5.0 phy",
	"emmc 5.1 phy"
};

enum xenon_phy_type_enum {
	EMMC_5_0_PHY,
	EMMC_5_1_PHY,
	NR_PHY_TYPES
};

enum soc_pad_ctrl_type {
	SOC_PAD_SD,
	SOC_PAD_FIXED_1_8V,
};

struct soc_pad_ctrl {
	 
	void __iomem	*reg;
	 
	enum soc_pad_ctrl_type pad_type;
	 
	void (*set_soc_pad)(struct sdhci_host *host,
			    unsigned char signal_voltage);
};

static struct xenon_emmc_phy_regs xenon_emmc_5_0_phy_regs = {
	.timing_adj	= XENON_EMMC_5_0_PHY_TIMING_ADJUST,
	.func_ctrl	= XENON_EMMC_5_0_PHY_FUNC_CONTROL,
	.pad_ctrl	= XENON_EMMC_5_0_PHY_PAD_CONTROL,
	.pad_ctrl2	= XENON_EMMC_5_0_PHY_PAD_CONTROL2,
	.dll_ctrl	= XENON_EMMC_5_0_PHY_DLL_CONTROL,
	.logic_timing_adj = XENON_EMMC_5_0_PHY_LOGIC_TIMING_ADJUST,
	.dll_update	= XENON_DLL_UPDATE_STROBE_5_0,
	.logic_timing_val = XENON_EMMC_5_0_PHY_LOGIC_TIMING_VALUE,
};

static struct xenon_emmc_phy_regs xenon_emmc_5_1_phy_regs = {
	.timing_adj	= XENON_EMMC_PHY_TIMING_ADJUST,
	.func_ctrl	= XENON_EMMC_PHY_FUNC_CONTROL,
	.pad_ctrl	= XENON_EMMC_PHY_PAD_CONTROL,
	.pad_ctrl2	= XENON_EMMC_PHY_PAD_CONTROL2,
	.dll_ctrl	= XENON_EMMC_PHY_DLL_CONTROL,
	.logic_timing_adj = XENON_EMMC_PHY_LOGIC_TIMING_ADJUST,
	.dll_update	= XENON_DLL_UPDATE,
	.logic_timing_val = XENON_LOGIC_TIMING_VALUE,
};

 
struct xenon_emmc_phy_params {
	bool	slow_mode;

	u8	znr;
	u8	zpr;

	 
	u8	nr_tun_times;
	 
	u8	tun_step_divider;

	struct soc_pad_ctrl pad_ctrl;
};

static int xenon_alloc_emmc_phy(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct xenon_emmc_phy_params *params;

	params = devm_kzalloc(mmc_dev(host->mmc), sizeof(*params), GFP_KERNEL);
	if (!params)
		return -ENOMEM;

	priv->phy_params = params;
	if (priv->phy_type == EMMC_5_0_PHY)
		priv->emmc_phy_regs = &xenon_emmc_5_0_phy_regs;
	else
		priv->emmc_phy_regs = &xenon_emmc_5_1_phy_regs;

	return 0;
}

 
static int xenon_emmc_phy_init(struct sdhci_host *host)
{
	u32 reg;
	u32 wait, clock;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct xenon_emmc_phy_regs *phy_regs = priv->emmc_phy_regs;

	reg = sdhci_readl(host, phy_regs->timing_adj);
	reg |= XENON_PHY_INITIALIZAION;
	sdhci_writel(host, reg, phy_regs->timing_adj);

	 
	wait = ((reg >> XENON_FC_SYNC_RST_DURATION_SHIFT) &
			XENON_FC_SYNC_RST_DURATION_MASK);
	 
	wait += ((reg >> XENON_FC_SYNC_RST_EN_DURATION_SHIFT) &
			XENON_FC_SYNC_RST_EN_DURATION_MASK);
	 
	wait += ((reg >> XENON_FC_SYNC_EN_DURATION_SHIFT) &
			XENON_FC_SYNC_EN_DURATION_MASK);
	 
	wait += ((reg >> XENON_WAIT_CYCLE_BEFORE_USING_SHIFT) &
			XENON_WAIT_CYCLE_BEFORE_USING_MASK);
	 
	wait += 8;
	wait <<= 20;

	clock = host->clock;
	if (!clock)
		 
		clock = XENON_LOWEST_SDCLK_FREQ;
	 
	wait /= clock;
	wait++;
	 
	udelay(wait);

	reg = sdhci_readl(host, phy_regs->timing_adj);
	reg &= XENON_PHY_INITIALIZAION;
	if (reg) {
		dev_err(mmc_dev(host->mmc), "eMMC PHY init cannot complete after %d us\n",
			wait);
		return -ETIMEDOUT;
	}

	return 0;
}

#define ARMADA_3700_SOC_PAD_1_8V	0x1
#define ARMADA_3700_SOC_PAD_3_3V	0x0

static void armada_3700_soc_pad_voltage_set(struct sdhci_host *host,
					    unsigned char signal_voltage)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct xenon_emmc_phy_params *params = priv->phy_params;

	if (params->pad_ctrl.pad_type == SOC_PAD_FIXED_1_8V) {
		writel(ARMADA_3700_SOC_PAD_1_8V, params->pad_ctrl.reg);
	} else if (params->pad_ctrl.pad_type == SOC_PAD_SD) {
		if (signal_voltage == MMC_SIGNAL_VOLTAGE_180)
			writel(ARMADA_3700_SOC_PAD_1_8V, params->pad_ctrl.reg);
		else if (signal_voltage == MMC_SIGNAL_VOLTAGE_330)
			writel(ARMADA_3700_SOC_PAD_3_3V, params->pad_ctrl.reg);
	}
}

 
static void xenon_emmc_phy_set_soc_pad(struct sdhci_host *host,
				       unsigned char signal_voltage)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct xenon_emmc_phy_params *params = priv->phy_params;

	if (!params->pad_ctrl.reg)
		return;

	if (params->pad_ctrl.set_soc_pad)
		params->pad_ctrl.set_soc_pad(host, signal_voltage);
}

 
static int xenon_emmc_phy_enable_dll(struct sdhci_host *host)
{
	u32 reg;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct xenon_emmc_phy_regs *phy_regs = priv->emmc_phy_regs;
	ktime_t timeout;

	if (WARN_ON(host->clock <= MMC_HIGH_52_MAX_DTR))
		return -EINVAL;

	reg = sdhci_readl(host, phy_regs->dll_ctrl);
	if (reg & XENON_DLL_ENABLE)
		return 0;

	 
	reg = sdhci_readl(host, phy_regs->dll_ctrl);
	reg |= (XENON_DLL_ENABLE | XENON_DLL_FAST_LOCK);

	 
	reg &= ~((XENON_DLL_PHASE_MASK << XENON_DLL_PHSEL0_SHIFT) |
		 (XENON_DLL_PHASE_MASK << XENON_DLL_PHSEL1_SHIFT));
	reg |= ((XENON_DLL_PHASE_90_DEGREE << XENON_DLL_PHSEL0_SHIFT) |
		(XENON_DLL_PHASE_90_DEGREE << XENON_DLL_PHSEL1_SHIFT));

	reg &= ~XENON_DLL_BYPASS_EN;
	reg |= phy_regs->dll_update;
	if (priv->phy_type == EMMC_5_1_PHY)
		reg &= ~XENON_DLL_REFCLK_SEL;
	sdhci_writel(host, reg, phy_regs->dll_ctrl);

	 
	timeout = ktime_add_ms(ktime_get(), 32);
	while (1) {
		bool timedout = ktime_after(ktime_get(), timeout);

		if (sdhci_readw(host, XENON_SLOT_EXT_PRESENT_STATE) &
		    XENON_DLL_LOCK_STATE)
			break;
		if (timedout) {
			dev_err(mmc_dev(host->mmc), "Wait for DLL Lock time-out\n");
			return -ETIMEDOUT;
		}
		udelay(100);
	}
	return 0;
}

 
static int xenon_emmc_phy_config_tuning(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct xenon_emmc_phy_params *params = priv->phy_params;
	u32 reg, tuning_step;
	int ret;

	if (host->clock <= MMC_HIGH_52_MAX_DTR)
		return -EINVAL;

	ret = xenon_emmc_phy_enable_dll(host);
	if (ret)
		return ret;

	 
	reg = sdhci_readl(host, XENON_SLOT_DLL_CUR_DLY_VAL);
	tuning_step = reg / params->tun_step_divider;
	if (unlikely(tuning_step > XENON_TUNING_STEP_MASK)) {
		dev_warn(mmc_dev(host->mmc),
			 "HS200 TUNING_STEP %d is larger than MAX value\n",
			 tuning_step);
		tuning_step = XENON_TUNING_STEP_MASK;
	}

	 
	reg = sdhci_readl(host, XENON_SLOT_OP_STATUS_CTRL);
	reg &= ~(XENON_TUN_CONSECUTIVE_TIMES_MASK <<
		 XENON_TUN_CONSECUTIVE_TIMES_SHIFT);
	reg |= (params->nr_tun_times << XENON_TUN_CONSECUTIVE_TIMES_SHIFT);
	reg &= ~(XENON_TUNING_STEP_MASK << XENON_TUNING_STEP_SHIFT);
	reg |= (tuning_step << XENON_TUNING_STEP_SHIFT);
	sdhci_writel(host, reg, XENON_SLOT_OP_STATUS_CTRL);

	return 0;
}

static void xenon_emmc_phy_disable_strobe(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	u32 reg;

	 
	reg = sdhci_readl(host, XENON_SLOT_EMMC_CTRL);
	reg &= ~(XENON_ENABLE_DATA_STROBE | XENON_ENABLE_RESP_STROBE);
	sdhci_writel(host, reg, XENON_SLOT_EMMC_CTRL);

	 
	if (priv->phy_type == EMMC_5_0_PHY) {
		reg = sdhci_readl(host, XENON_EMMC_5_0_PHY_PAD_CONTROL);
		reg &= ~(XENON_EMMC5_FC_QSP_PD | XENON_EMMC5_FC_QSP_PU);
		sdhci_writel(host, reg, XENON_EMMC_5_0_PHY_PAD_CONTROL);
	} else {
		reg = sdhci_readl(host, XENON_EMMC_PHY_PAD_CONTROL1);
		reg &= ~(XENON_EMMC5_1_FC_QSP_PD | XENON_EMMC5_1_FC_QSP_PU);
		sdhci_writel(host, reg, XENON_EMMC_PHY_PAD_CONTROL1);
	}
}

 
static void xenon_emmc_phy_strobe_delay_adj(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	u32 reg;

	if (WARN_ON(host->timing != MMC_TIMING_MMC_HS400))
		return;

	if (host->clock <= MMC_HIGH_52_MAX_DTR)
		return;

	dev_dbg(mmc_dev(host->mmc), "starts HS400 strobe delay adjustment\n");

	xenon_emmc_phy_enable_dll(host);

	 
	reg = sdhci_readl(host, XENON_SLOT_EMMC_CTRL);
	reg |= XENON_ENABLE_DATA_STROBE;
	 
	if (host->mmc->ios.enhanced_strobe)
		reg |= XENON_ENABLE_RESP_STROBE;
	sdhci_writel(host, reg, XENON_SLOT_EMMC_CTRL);

	 
	if (priv->phy_type == EMMC_5_0_PHY) {
		reg = sdhci_readl(host, XENON_EMMC_5_0_PHY_PAD_CONTROL);
		reg |= XENON_EMMC5_FC_QSP_PD;
		reg &= ~XENON_EMMC5_FC_QSP_PU;
		sdhci_writel(host, reg, XENON_EMMC_5_0_PHY_PAD_CONTROL);
	} else {
		reg = sdhci_readl(host, XENON_EMMC_PHY_PAD_CONTROL1);
		reg |= XENON_EMMC5_1_FC_QSP_PD;
		reg &= ~XENON_EMMC5_1_FC_QSP_PU;
		sdhci_writel(host, reg, XENON_EMMC_PHY_PAD_CONTROL1);
	}
}

 
static bool xenon_emmc_phy_slow_mode(struct sdhci_host *host,
				     unsigned char timing)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct xenon_emmc_phy_params *params = priv->phy_params;
	struct xenon_emmc_phy_regs *phy_regs = priv->emmc_phy_regs;
	u32 reg;
	int ret;

	if (host->clock > MMC_HIGH_52_MAX_DTR)
		return false;

	reg = sdhci_readl(host, phy_regs->timing_adj);
	 
	switch (timing) {
	case MMC_TIMING_LEGACY:
		 
		if (params->slow_mode) {
			reg |= XENON_TIMING_ADJUST_SLOW_MODE;
			ret = true;
		} else {
			reg &= ~XENON_TIMING_ADJUST_SLOW_MODE;
			ret = false;
		}
		break;
	case MMC_TIMING_UHS_SDR25:
	case MMC_TIMING_UHS_SDR12:
	case MMC_TIMING_SD_HS:
	case MMC_TIMING_MMC_HS:
		if ((priv->init_card_type == MMC_TYPE_SDIO) ||
		    params->slow_mode) {
			reg |= XENON_TIMING_ADJUST_SLOW_MODE;
			ret = true;
			break;
		}
		fallthrough;
	default:
		reg &= ~XENON_TIMING_ADJUST_SLOW_MODE;
		ret = false;
	}

	sdhci_writel(host, reg, phy_regs->timing_adj);
	return ret;
}

 
static void xenon_emmc_phy_set(struct sdhci_host *host,
			       unsigned char timing)
{
	u32 reg;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	struct xenon_emmc_phy_params *params = priv->phy_params;
	struct xenon_emmc_phy_regs *phy_regs = priv->emmc_phy_regs;

	dev_dbg(mmc_dev(host->mmc), "eMMC PHY setting starts\n");

	 
	reg = sdhci_readl(host, phy_regs->pad_ctrl);
	reg |= (XENON_FC_DQ_RECEN | XENON_FC_CMD_RECEN |
		XENON_FC_QSP_RECEN | XENON_OEN_QSN);
	 
	reg |= XENON_FC_ALL_CMOS_RECEIVER;
	sdhci_writel(host, reg, phy_regs->pad_ctrl);

	 
	if (priv->phy_type == EMMC_5_0_PHY) {
		reg = sdhci_readl(host, XENON_EMMC_5_0_PHY_PAD_CONTROL);
		reg |= (XENON_EMMC5_FC_CMD_PU | XENON_EMMC5_FC_DQ_PU);
		reg &= ~(XENON_EMMC5_FC_CMD_PD | XENON_EMMC5_FC_DQ_PD);
		sdhci_writel(host, reg, XENON_EMMC_5_0_PHY_PAD_CONTROL);
	} else {
		reg = sdhci_readl(host, XENON_EMMC_PHY_PAD_CONTROL1);
		reg |= (XENON_EMMC5_1_FC_CMD_PU | XENON_EMMC5_1_FC_DQ_PU);
		reg &= ~(XENON_EMMC5_1_FC_CMD_PD | XENON_EMMC5_1_FC_DQ_PD);
		sdhci_writel(host, reg, XENON_EMMC_PHY_PAD_CONTROL1);
	}

	if (timing == MMC_TIMING_LEGACY) {
		xenon_emmc_phy_slow_mode(host, timing);
		goto phy_init;
	}

	 
	reg = sdhci_readl(host, phy_regs->timing_adj);
	if (priv->init_card_type == MMC_TYPE_SDIO)
		reg |= XENON_TIMING_ADJUST_SDIO_MODE;
	else
		reg &= ~XENON_TIMING_ADJUST_SDIO_MODE;
	sdhci_writel(host, reg, phy_regs->timing_adj);

	if (xenon_emmc_phy_slow_mode(host, timing))
		goto phy_init;

	 
	reg = sdhci_readl(host, phy_regs->pad_ctrl2);
	reg &= ~((XENON_ZNR_MASK << XENON_ZNR_SHIFT) | XENON_ZPR_MASK);
	reg |= ((params->znr << XENON_ZNR_SHIFT) | params->zpr);
	sdhci_writel(host, reg, phy_regs->pad_ctrl2);

	 
	reg = sdhci_readl(host, SDHCI_CLOCK_CONTROL);
	reg &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, reg, SDHCI_CLOCK_CONTROL);

	reg = sdhci_readl(host, phy_regs->func_ctrl);
	switch (timing) {
	case MMC_TIMING_MMC_HS400:
		reg |= (XENON_DQ_DDR_MODE_MASK << XENON_DQ_DDR_MODE_SHIFT) |
		       XENON_CMD_DDR_MODE;
		reg &= ~XENON_DQ_ASYNC_MODE;
		break;
	case MMC_TIMING_UHS_DDR50:
	case MMC_TIMING_MMC_DDR52:
		reg |= (XENON_DQ_DDR_MODE_MASK << XENON_DQ_DDR_MODE_SHIFT) |
		       XENON_CMD_DDR_MODE | XENON_DQ_ASYNC_MODE;
		break;
	default:
		reg &= ~((XENON_DQ_DDR_MODE_MASK << XENON_DQ_DDR_MODE_SHIFT) |
			 XENON_CMD_DDR_MODE);
		reg |= XENON_DQ_ASYNC_MODE;
	}
	sdhci_writel(host, reg, phy_regs->func_ctrl);

	 
	reg = sdhci_readl(host, SDHCI_CLOCK_CONTROL);
	reg |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, reg, SDHCI_CLOCK_CONTROL);

	if (timing == MMC_TIMING_MMC_HS400)
		 
		sdhci_writel(host, phy_regs->logic_timing_val,
			     phy_regs->logic_timing_adj);
	else
		xenon_emmc_phy_disable_strobe(host);

phy_init:
	xenon_emmc_phy_init(host);

	dev_dbg(mmc_dev(host->mmc), "eMMC PHY setting completes\n");
}

static int get_dt_pad_ctrl_data(struct sdhci_host *host,
				struct device_node *np,
				struct xenon_emmc_phy_params *params)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	int ret = 0;
	const char *name;
	struct resource iomem;

	if (priv->hw_version == XENON_A3700)
		params->pad_ctrl.set_soc_pad = armada_3700_soc_pad_voltage_set;
	else
		return 0;

	if (of_address_to_resource(np, 1, &iomem)) {
		dev_err(mmc_dev(host->mmc), "Unable to find SoC PAD ctrl register address for %pOFn\n",
			np);
		return -EINVAL;
	}

	params->pad_ctrl.reg = devm_ioremap_resource(mmc_dev(host->mmc),
						     &iomem);
	if (IS_ERR(params->pad_ctrl.reg))
		return PTR_ERR(params->pad_ctrl.reg);

	ret = of_property_read_string(np, "marvell,pad-type", &name);
	if (ret) {
		dev_err(mmc_dev(host->mmc), "Unable to determine SoC PHY PAD ctrl type\n");
		return ret;
	}
	if (!strcmp(name, "sd")) {
		params->pad_ctrl.pad_type = SOC_PAD_SD;
	} else if (!strcmp(name, "fixed-1-8v")) {
		params->pad_ctrl.pad_type = SOC_PAD_FIXED_1_8V;
	} else {
		dev_err(mmc_dev(host->mmc), "Unsupported SoC PHY PAD ctrl type %s\n",
			name);
		return -EINVAL;
	}

	return ret;
}

static int xenon_emmc_phy_parse_params(struct sdhci_host *host,
				       struct device *dev,
				       struct xenon_emmc_phy_params *params)
{
	u32 value;

	params->slow_mode = false;
	if (device_property_read_bool(dev, "marvell,xenon-phy-slow-mode"))
		params->slow_mode = true;

	params->znr = XENON_ZNR_DEF_VALUE;
	if (!device_property_read_u32(dev, "marvell,xenon-phy-znr", &value))
		params->znr = value & XENON_ZNR_MASK;

	params->zpr = XENON_ZPR_DEF_VALUE;
	if (!device_property_read_u32(dev, "marvell,xenon-phy-zpr", &value))
		params->zpr = value & XENON_ZPR_MASK;

	params->nr_tun_times = XENON_TUN_CONSECUTIVE_TIMES;
	if (!device_property_read_u32(dev, "marvell,xenon-phy-nr-success-tun",
				      &value))
		params->nr_tun_times = value & XENON_TUN_CONSECUTIVE_TIMES_MASK;

	params->tun_step_divider = XENON_TUNING_STEP_DIVIDER;
	if (!device_property_read_u32(dev, "marvell,xenon-phy-tun-step-divider",
				      &value))
		params->tun_step_divider = value & 0xFF;

	if (dev->of_node)
		return get_dt_pad_ctrl_data(host, dev->of_node, params);
	return 0;
}

 
void xenon_soc_pad_ctrl(struct sdhci_host *host,
			unsigned char signal_voltage)
{
	xenon_emmc_phy_set_soc_pad(host, signal_voltage);
}

 
static int xenon_hs_delay_adj(struct sdhci_host *host)
{
	int ret = 0;

	if (WARN_ON(host->clock <= XENON_DEFAULT_SDCLK_FREQ))
		return -EINVAL;

	switch (host->timing) {
	case MMC_TIMING_MMC_HS400:
		xenon_emmc_phy_strobe_delay_adj(host);
		return 0;
	case MMC_TIMING_MMC_HS200:
	case MMC_TIMING_UHS_SDR104:
		return xenon_emmc_phy_config_tuning(host);
	case MMC_TIMING_MMC_DDR52:
	case MMC_TIMING_UHS_DDR50:
		 
		dev_warn_once(mmc_dev(host->mmc), "Timing issue might occur in DDR mode\n");
		return 0;
	}

	return ret;
}

 
int xenon_phy_adj(struct sdhci_host *host, struct mmc_ios *ios)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	int ret = 0;

	if (!host->clock) {
		priv->clock = 0;
		return 0;
	}

	 
	if ((host->clock == priv->clock) &&
	    (ios->bus_width == priv->bus_width) &&
	    (ios->timing == priv->timing))
		return 0;

	xenon_emmc_phy_set(host, ios->timing);

	 
	priv->bus_width = ios->bus_width;

	priv->timing = ios->timing;
	priv->clock = host->clock;

	 
	if (ios->timing == MMC_TIMING_LEGACY)
		return 0;

	if (host->clock > XENON_DEFAULT_SDCLK_FREQ)
		ret = xenon_hs_delay_adj(host);
	return ret;
}

static int xenon_add_phy(struct device *dev, struct sdhci_host *host,
			 const char *phy_name)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	int ret;

	priv->phy_type = match_string(phy_types, NR_PHY_TYPES, phy_name);
	if (priv->phy_type < 0) {
		dev_err(mmc_dev(host->mmc),
			"Unable to determine PHY name %s. Use default eMMC 5.1 PHY\n",
			phy_name);
		priv->phy_type = EMMC_5_1_PHY;
	}

	ret = xenon_alloc_emmc_phy(host);
	if (ret)
		return ret;

	return xenon_emmc_phy_parse_params(host, dev, priv->phy_params);
}

int xenon_phy_parse_params(struct device *dev, struct sdhci_host *host)
{
	const char *phy_type = NULL;

	if (!device_property_read_string(dev, "marvell,xenon-phy-type", &phy_type))
		return xenon_add_phy(dev, host, phy_type);

	return xenon_add_phy(dev, host, "emmc 5.1 phy");
}
