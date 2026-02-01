
 

#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include "sdhci-pltfm.h"
#include "sdhci-xenon.h"

static int xenon_enable_internal_clk(struct sdhci_host *host)
{
	u32 reg;
	ktime_t timeout;

	reg = sdhci_readl(host, SDHCI_CLOCK_CONTROL);
	reg |= SDHCI_CLOCK_INT_EN;
	sdhci_writel(host, reg, SDHCI_CLOCK_CONTROL);
	 
	timeout = ktime_add_ms(ktime_get(), 20);
	while (1) {
		bool timedout = ktime_after(ktime_get(), timeout);

		reg = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
		if (reg & SDHCI_CLOCK_INT_STABLE)
			break;
		if (timedout) {
			dev_err(mmc_dev(host->mmc), "Internal clock never stabilised.\n");
			return -ETIMEDOUT;
		}
		usleep_range(900, 1100);
	}

	return 0;
}

 
static void xenon_set_sdclk_off_idle(struct sdhci_host *host,
				     unsigned char sdhc_id, bool enable)
{
	u32 reg;
	u32 mask;

	reg = sdhci_readl(host, XENON_SYS_OP_CTRL);
	 
	mask = (0x1 << (XENON_SDCLK_IDLEOFF_ENABLE_SHIFT + sdhc_id));
	if (enable)
		reg |= mask;
	else
		reg &= ~mask;

	sdhci_writel(host, reg, XENON_SYS_OP_CTRL);
}

 
static void xenon_set_acg(struct sdhci_host *host, bool enable)
{
	u32 reg;

	reg = sdhci_readl(host, XENON_SYS_OP_CTRL);
	if (enable)
		reg &= ~XENON_AUTO_CLKGATE_DISABLE_MASK;
	else
		reg |= XENON_AUTO_CLKGATE_DISABLE_MASK;
	sdhci_writel(host, reg, XENON_SYS_OP_CTRL);
}

 
static void xenon_enable_sdhc(struct sdhci_host *host,
			      unsigned char sdhc_id)
{
	u32 reg;

	reg = sdhci_readl(host, XENON_SYS_OP_CTRL);
	reg |= (BIT(sdhc_id) << XENON_SLOT_ENABLE_SHIFT);
	sdhci_writel(host, reg, XENON_SYS_OP_CTRL);

	host->mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY;
	 
	host->mmc->caps &= ~MMC_CAP_BUS_WIDTH_TEST;
}

 
static void xenon_disable_sdhc(struct sdhci_host *host,
			       unsigned char sdhc_id)
{
	u32 reg;

	reg = sdhci_readl(host, XENON_SYS_OP_CTRL);
	reg &= ~(BIT(sdhc_id) << XENON_SLOT_ENABLE_SHIFT);
	sdhci_writel(host, reg, XENON_SYS_OP_CTRL);
}

 
static void xenon_enable_sdhc_parallel_tran(struct sdhci_host *host,
					    unsigned char sdhc_id)
{
	u32 reg;

	reg = sdhci_readl(host, XENON_SYS_EXT_OP_CTRL);
	reg |= BIT(sdhc_id);
	sdhci_writel(host, reg, XENON_SYS_EXT_OP_CTRL);
}

 
static void xenon_mask_cmd_conflict_err(struct sdhci_host *host)
{
	u32  reg;

	reg = sdhci_readl(host, XENON_SYS_EXT_OP_CTRL);
	reg |= XENON_MASK_CMD_CONFLICT_ERR;
	sdhci_writel(host, reg, XENON_SYS_EXT_OP_CTRL);
}

static void xenon_retune_setup(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	u32 reg;

	 
	reg = sdhci_readl(host, XENON_SLOT_RETUNING_REQ_CTRL);
	reg &= ~XENON_RETUNING_COMPATIBLE;
	sdhci_writel(host, reg, XENON_SLOT_RETUNING_REQ_CTRL);

	 
	reg = sdhci_readl(host, SDHCI_SIGNAL_ENABLE);
	reg &= ~SDHCI_INT_RETUNE;
	sdhci_writel(host, reg, SDHCI_SIGNAL_ENABLE);
	reg = sdhci_readl(host, SDHCI_INT_ENABLE);
	reg &= ~SDHCI_INT_RETUNE;
	sdhci_writel(host, reg, SDHCI_INT_ENABLE);

	 
	host->tuning_mode = SDHCI_TUNING_MODE_1;
	 
	host->tuning_count = 1 << (priv->tuning_count - 1);
}

 
 
static void xenon_reset_exit(struct sdhci_host *host,
			     unsigned char sdhc_id, u8 mask)
{
	 
	if (!(mask & SDHCI_RESET_ALL))
		return;

	 
	xenon_retune_setup(host);

	 
	xenon_set_acg(host, false);

	xenon_set_sdclk_off_idle(host, sdhc_id, false);

	xenon_mask_cmd_conflict_err(host);
}

static void xenon_reset(struct sdhci_host *host, u8 mask)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);

	sdhci_reset(host, mask);
	xenon_reset_exit(host, priv->sdhc_id, mask);
}

 
static void xenon_set_uhs_signaling(struct sdhci_host *host,
				    unsigned int timing)
{
	u16 ctrl_2;

	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	 
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	if (timing == MMC_TIMING_MMC_HS200)
		ctrl_2 |= XENON_CTRL_HS200;
	else if (timing == MMC_TIMING_UHS_SDR104)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
	else if (timing == MMC_TIMING_UHS_SDR12)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
	else if (timing == MMC_TIMING_UHS_SDR25)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
	else if (timing == MMC_TIMING_UHS_SDR50)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50;
	else if ((timing == MMC_TIMING_UHS_DDR50) ||
		 (timing == MMC_TIMING_MMC_DDR52))
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50;
	else if (timing == MMC_TIMING_MMC_HS400)
		ctrl_2 |= XENON_CTRL_HS400;
	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);
}

static void xenon_set_power(struct sdhci_host *host, unsigned char mode,
		unsigned short vdd)
{
	struct mmc_host *mmc = host->mmc;
	u8 pwr = host->pwr;

	sdhci_set_power_noreg(host, mode, vdd);

	if (host->pwr == pwr)
		return;

	if (host->pwr == 0)
		vdd = 0;

	if (!IS_ERR(mmc->supply.vmmc))
		mmc_regulator_set_ocr(mmc, mmc->supply.vmmc, vdd);
}

static void xenon_voltage_switch(struct sdhci_host *host)
{
	 
	usleep_range(5000, 5500);
}

static unsigned int xenon_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	if (pltfm_host->clk)
		return sdhci_pltfm_clk_get_max_clock(host);
	else
		return pltfm_host->clock;
}

static const struct sdhci_ops sdhci_xenon_ops = {
	.voltage_switch		= xenon_voltage_switch,
	.set_clock		= sdhci_set_clock,
	.set_power		= xenon_set_power,
	.set_bus_width		= sdhci_set_bus_width,
	.reset			= xenon_reset,
	.set_uhs_signaling	= xenon_set_uhs_signaling,
	.get_max_clock		= xenon_get_max_clock,
};

static const struct sdhci_pltfm_data sdhci_xenon_pdata = {
	.ops = &sdhci_xenon_ops,
	.quirks = SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC |
		  SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER |
		  SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
};

 
static void xenon_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	u32 reg;

	 
	if ((ios->timing == MMC_TIMING_MMC_HS400) ||
	    (ios->timing == MMC_TIMING_MMC_HS200) ||
	    (ios->timing == MMC_TIMING_MMC_HS)) {
		host->preset_enabled = false;
		host->quirks2 |= SDHCI_QUIRK2_PRESET_VALUE_BROKEN;
		host->flags &= ~SDHCI_PV_ENABLED;

		reg = sdhci_readw(host, SDHCI_HOST_CONTROL2);
		reg &= ~SDHCI_CTRL_PRESET_VAL_ENABLE;
		sdhci_writew(host, reg, SDHCI_HOST_CONTROL2);
	} else {
		host->quirks2 &= ~SDHCI_QUIRK2_PRESET_VALUE_BROKEN;
	}

	sdhci_set_ios(mmc, ios);
	xenon_phy_adj(host, ios);

	if (host->clock > XENON_DEFAULT_SDCLK_FREQ)
		xenon_set_sdclk_off_idle(host, priv->sdhc_id, true);
}

static int xenon_start_signal_voltage_switch(struct mmc_host *mmc,
					     struct mmc_ios *ios)
{
	struct sdhci_host *host = mmc_priv(mmc);

	 
	xenon_enable_internal_clk(host);

	xenon_soc_pad_ctrl(host, ios->signal_voltage);

	 
	if (PTR_ERR(mmc->supply.vqmmc) == -ENODEV)
		return 0;

	return sdhci_start_signal_voltage_switch(mmc, ios);
}

 
static void xenon_init_card(struct mmc_host *mmc, struct mmc_card *card)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);

	 
	priv->init_card_type = card->type;
}

static int xenon_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct sdhci_host *host = mmc_priv(mmc);

	if (host->timing == MMC_TIMING_UHS_DDR50 ||
		host->timing == MMC_TIMING_MMC_DDR52)
		return 0;

	 
	if (host->tuning_mode != SDHCI_TUNING_MODE_1)
		xenon_retune_setup(host);

	return sdhci_execute_tuning(mmc, opcode);
}

static void xenon_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	u32 reg;
	u8 sdhc_id = priv->sdhc_id;

	sdhci_enable_sdio_irq(mmc, enable);

	if (enable) {
		 
		reg = sdhci_readl(host, XENON_SYS_CFG_INFO);
		reg |= (1 << (sdhc_id + XENON_SLOT_TYPE_SDIO_SHIFT));
		sdhci_writel(host, reg, XENON_SYS_CFG_INFO);
	} else {
		 
		reg = sdhci_readl(host, XENON_SYS_CFG_INFO);
		reg &= ~(1 << (sdhc_id + XENON_SLOT_TYPE_SDIO_SHIFT));
		sdhci_writel(host, reg, XENON_SYS_CFG_INFO);
	}
}

static void xenon_replace_mmc_host_ops(struct sdhci_host *host)
{
	host->mmc_host_ops.set_ios = xenon_set_ios;
	host->mmc_host_ops.start_signal_voltage_switch =
			xenon_start_signal_voltage_switch;
	host->mmc_host_ops.init_card = xenon_init_card;
	host->mmc_host_ops.execute_tuning = xenon_execute_tuning;
	host->mmc_host_ops.enable_sdio_irq = xenon_enable_sdio_irq;
}

 
static int xenon_probe_params(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct mmc_host *mmc = host->mmc;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	u32 sdhc_id, nr_sdhc;
	u32 tuning_count;

	 
	if (priv->hw_version == XENON_AP806)
		host->quirks2 |= SDHCI_QUIRK2_BROKEN_HS200;

	sdhc_id = 0x0;
	if (!device_property_read_u32(dev, "marvell,xenon-sdhc-id", &sdhc_id)) {
		nr_sdhc = sdhci_readl(host, XENON_SYS_CFG_INFO);
		nr_sdhc &= XENON_NR_SUPPORTED_SLOT_MASK;
		if (unlikely(sdhc_id > nr_sdhc)) {
			dev_err(mmc_dev(mmc), "SDHC Index %d exceeds Number of SDHCs %d\n",
				sdhc_id, nr_sdhc);
			return -EINVAL;
		}
	}
	priv->sdhc_id = sdhc_id;

	tuning_count = XENON_DEF_TUNING_COUNT;
	if (!device_property_read_u32(dev, "marvell,xenon-tun-count",
				      &tuning_count)) {
		if (unlikely(tuning_count >= XENON_TMR_RETUN_NO_PRESENT)) {
			dev_err(mmc_dev(mmc), "Wrong Re-tuning Count. Set default value %d\n",
				XENON_DEF_TUNING_COUNT);
			tuning_count = XENON_DEF_TUNING_COUNT;
		}
	}
	priv->tuning_count = tuning_count;

	return xenon_phy_parse_params(dev, host);
}

static int xenon_sdhc_prepare(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	u8 sdhc_id = priv->sdhc_id;

	 
	xenon_enable_sdhc(host, sdhc_id);

	 
	xenon_set_acg(host, true);

	 
	xenon_enable_sdhc_parallel_tran(host, sdhc_id);

	 
	xenon_set_sdclk_off_idle(host, sdhc_id, false);

	xenon_mask_cmd_conflict_err(host);

	return 0;
}

static void xenon_sdhc_unprepare(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	u8 sdhc_id = priv->sdhc_id;

	 
	xenon_disable_sdhc(host, sdhc_id);
}

static int xenon_probe(struct platform_device *pdev)
{
	struct sdhci_pltfm_host *pltfm_host;
	struct device *dev = &pdev->dev;
	struct sdhci_host *host;
	struct xenon_priv *priv;
	int err;

	host = sdhci_pltfm_init(pdev, &sdhci_xenon_pdata,
				sizeof(struct xenon_priv));
	if (IS_ERR(host))
		return PTR_ERR(host);

	pltfm_host = sdhci_priv(host);
	priv = sdhci_pltfm_priv(pltfm_host);

	priv->hw_version = (unsigned long)device_get_match_data(&pdev->dev);

	 
	xenon_replace_mmc_host_ops(host);

	if (dev->of_node) {
		pltfm_host->clk = devm_clk_get(&pdev->dev, "core");
		if (IS_ERR(pltfm_host->clk)) {
			err = PTR_ERR(pltfm_host->clk);
			dev_err(&pdev->dev, "Failed to setup input clk: %d\n", err);
			goto free_pltfm;
		}
		err = clk_prepare_enable(pltfm_host->clk);
		if (err)
			goto free_pltfm;

		priv->axi_clk = devm_clk_get(&pdev->dev, "axi");
		if (IS_ERR(priv->axi_clk)) {
			err = PTR_ERR(priv->axi_clk);
			if (err == -EPROBE_DEFER)
				goto err_clk;
		} else {
			err = clk_prepare_enable(priv->axi_clk);
			if (err)
				goto err_clk;
		}
	}

	err = mmc_of_parse(host->mmc);
	if (err)
		goto err_clk_axi;

	sdhci_get_property(pdev);

	xenon_set_acg(host, false);

	 
	err = xenon_probe_params(pdev);
	if (err)
		goto err_clk_axi;

	err = xenon_sdhc_prepare(host);
	if (err)
		goto err_clk_axi;

	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 50);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);

	err = sdhci_add_host(host);
	if (err)
		goto remove_sdhc;

	pm_runtime_put_autosuspend(&pdev->dev);

	return 0;

remove_sdhc:
	pm_runtime_disable(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);
	xenon_sdhc_unprepare(host);
err_clk_axi:
	clk_disable_unprepare(priv->axi_clk);
err_clk:
	clk_disable_unprepare(pltfm_host->clk);
free_pltfm:
	sdhci_pltfm_free(pdev);
	return err;
}

static void xenon_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);

	pm_runtime_get_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);

	sdhci_remove_host(host, 0);

	xenon_sdhc_unprepare(host);
	clk_disable_unprepare(priv->axi_clk);
	clk_disable_unprepare(pltfm_host->clk);

	sdhci_pltfm_free(pdev);
}

#ifdef CONFIG_PM_SLEEP
static int xenon_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	int ret;

	ret = pm_runtime_force_suspend(dev);

	priv->restore_needed = true;
	return ret;
}
#endif

#ifdef CONFIG_PM
static int xenon_runtime_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	int ret;

	ret = sdhci_runtime_suspend_host(host);
	if (ret)
		return ret;

	if (host->tuning_mode != SDHCI_TUNING_MODE_3)
		mmc_retune_needed(host->mmc);

	clk_disable_unprepare(pltfm_host->clk);
	 
	priv->clock = 0;
	return 0;
}

static int xenon_runtime_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct xenon_priv *priv = sdhci_pltfm_priv(pltfm_host);
	int ret;

	ret = clk_prepare_enable(pltfm_host->clk);
	if (ret) {
		dev_err(dev, "can't enable mainck\n");
		return ret;
	}

	if (priv->restore_needed) {
		ret = xenon_sdhc_prepare(host);
		if (ret)
			goto out;
		priv->restore_needed = false;
	}

	ret = sdhci_runtime_resume_host(host, 0);
	if (ret)
		goto out;
	return 0;
out:
	clk_disable_unprepare(pltfm_host->clk);
	return ret;
}
#endif  

static const struct dev_pm_ops sdhci_xenon_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(xenon_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(xenon_runtime_suspend,
			   xenon_runtime_resume,
			   NULL)
};

static const struct of_device_id sdhci_xenon_dt_ids[] = {
	{ .compatible = "marvell,armada-ap806-sdhci", .data = (void *)XENON_AP806},
	{ .compatible = "marvell,armada-ap807-sdhci", .data = (void *)XENON_AP807},
	{ .compatible = "marvell,armada-cp110-sdhci", .data =  (void *)XENON_CP110},
	{ .compatible = "marvell,armada-3700-sdhci", .data =  (void *)XENON_A3700},
	{}
};
MODULE_DEVICE_TABLE(of, sdhci_xenon_dt_ids);

#ifdef CONFIG_ACPI
static const struct acpi_device_id sdhci_xenon_acpi_ids[] = {
	{ .id = "MRVL0002", XENON_AP806},
	{ .id = "MRVL0003", XENON_AP807},
	{ .id = "MRVL0004", XENON_CP110},
	{}
};
MODULE_DEVICE_TABLE(acpi, sdhci_xenon_acpi_ids);
#endif

static struct platform_driver sdhci_xenon_driver = {
	.driver	= {
		.name	= "xenon-sdhci",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = sdhci_xenon_dt_ids,
		.acpi_match_table = ACPI_PTR(sdhci_xenon_acpi_ids),
		.pm = &sdhci_xenon_dev_pm_ops,
	},
	.probe	= xenon_probe,
	.remove_new = xenon_remove,
};

module_platform_driver(sdhci_xenon_driver);

MODULE_DESCRIPTION("SDHCI platform driver for Marvell Xenon SDHC");
MODULE_AUTHOR("Hu Ziji <huziji@marvell.com>");
MODULE_LICENSE("GPL v2");
