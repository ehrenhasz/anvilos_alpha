
 

 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include <linux/usb/hcd.h>
#include <linux/usb/ch11.h>

#include "core.h"
#include "hcd.h"

 
int dwc2_backup_global_registers(struct dwc2_hsotg *hsotg)
{
	struct dwc2_gregs_backup *gr;

	dev_dbg(hsotg->dev, "%s\n", __func__);

	 
	gr = &hsotg->gr_backup;

	gr->gotgctl = dwc2_readl(hsotg, GOTGCTL);
	gr->gintmsk = dwc2_readl(hsotg, GINTMSK);
	gr->gahbcfg = dwc2_readl(hsotg, GAHBCFG);
	gr->gusbcfg = dwc2_readl(hsotg, GUSBCFG);
	gr->grxfsiz = dwc2_readl(hsotg, GRXFSIZ);
	gr->gnptxfsiz = dwc2_readl(hsotg, GNPTXFSIZ);
	gr->gdfifocfg = dwc2_readl(hsotg, GDFIFOCFG);
	gr->pcgcctl1 = dwc2_readl(hsotg, PCGCCTL1);
	gr->glpmcfg = dwc2_readl(hsotg, GLPMCFG);
	gr->gi2cctl = dwc2_readl(hsotg, GI2CCTL);
	gr->pcgcctl = dwc2_readl(hsotg, PCGCTL);

	gr->valid = true;
	return 0;
}

 
int dwc2_restore_global_registers(struct dwc2_hsotg *hsotg)
{
	struct dwc2_gregs_backup *gr;

	dev_dbg(hsotg->dev, "%s\n", __func__);

	 
	gr = &hsotg->gr_backup;
	if (!gr->valid) {
		dev_err(hsotg->dev, "%s: no global registers to restore\n",
			__func__);
		return -EINVAL;
	}
	gr->valid = false;

	dwc2_writel(hsotg, 0xffffffff, GINTSTS);
	dwc2_writel(hsotg, gr->gotgctl, GOTGCTL);
	dwc2_writel(hsotg, gr->gintmsk, GINTMSK);
	dwc2_writel(hsotg, gr->gusbcfg, GUSBCFG);
	dwc2_writel(hsotg, gr->gahbcfg, GAHBCFG);
	dwc2_writel(hsotg, gr->grxfsiz, GRXFSIZ);
	dwc2_writel(hsotg, gr->gnptxfsiz, GNPTXFSIZ);
	dwc2_writel(hsotg, gr->gdfifocfg, GDFIFOCFG);
	dwc2_writel(hsotg, gr->pcgcctl1, PCGCCTL1);
	dwc2_writel(hsotg, gr->glpmcfg, GLPMCFG);
	dwc2_writel(hsotg, gr->pcgcctl, PCGCTL);
	dwc2_writel(hsotg, gr->gi2cctl, GI2CCTL);

	return 0;
}

 
int dwc2_exit_partial_power_down(struct dwc2_hsotg *hsotg, int rem_wakeup,
				 bool restore)
{
	struct dwc2_gregs_backup *gr;

	gr = &hsotg->gr_backup;

	 
	if (gr->gotgctl & GOTGCTL_CURMODE_HOST)
		return dwc2_host_exit_partial_power_down(hsotg, rem_wakeup,
							 restore);
	else
		return dwc2_gadget_exit_partial_power_down(hsotg, restore);
}

 
int dwc2_enter_partial_power_down(struct dwc2_hsotg *hsotg)
{
	if (dwc2_is_host_mode(hsotg))
		return dwc2_host_enter_partial_power_down(hsotg);
	else
		return dwc2_gadget_enter_partial_power_down(hsotg);
}

 
static void dwc2_restore_essential_regs(struct dwc2_hsotg *hsotg, int rmode,
					int is_host)
{
	u32 pcgcctl;
	struct dwc2_gregs_backup *gr;
	struct dwc2_dregs_backup *dr;
	struct dwc2_hregs_backup *hr;

	gr = &hsotg->gr_backup;
	dr = &hsotg->dr_backup;
	hr = &hsotg->hr_backup;

	dev_dbg(hsotg->dev, "%s: restoring essential regs\n", __func__);

	 
	pcgcctl = (gr->pcgcctl & 0xffffc000);
	 
	if (is_host) {
		if (!(pcgcctl & PCGCTL_P2HD_PRT_SPD_MASK))
			pcgcctl |= BIT(17);
	} else {
		if (!(pcgcctl & PCGCTL_P2HD_DEV_ENUM_SPD_MASK))
			pcgcctl |= BIT(17);
	}
	dwc2_writel(hsotg, pcgcctl, PCGCTL);

	 
	dwc2_writel(hsotg, gr->gahbcfg | GAHBCFG_GLBL_INTR_EN, GAHBCFG);

	 
	dwc2_writel(hsotg, 0xffffffff, GINTSTS);

	 
	dwc2_writel(hsotg, GINTSTS_RESTOREDONE, GINTMSK);

	 
	dwc2_writel(hsotg, gr->gusbcfg, GUSBCFG);

	if (is_host) {
		dwc2_writel(hsotg, hr->hcfg, HCFG);
		if (rmode)
			pcgcctl |= PCGCTL_RESTOREMODE;
		dwc2_writel(hsotg, pcgcctl, PCGCTL);
		udelay(10);

		pcgcctl |= PCGCTL_ESS_REG_RESTORED;
		dwc2_writel(hsotg, pcgcctl, PCGCTL);
		udelay(10);
	} else {
		dwc2_writel(hsotg, dr->dcfg, DCFG);
		if (!rmode)
			pcgcctl |= PCGCTL_RESTOREMODE | PCGCTL_RSTPDWNMODULE;
		dwc2_writel(hsotg, pcgcctl, PCGCTL);
		udelay(10);

		pcgcctl |= PCGCTL_ESS_REG_RESTORED;
		dwc2_writel(hsotg, pcgcctl, PCGCTL);
		udelay(10);
	}
}

 
void dwc2_hib_restore_common(struct dwc2_hsotg *hsotg, int rem_wakeup,
			     int is_host)
{
	u32 gpwrdn;

	 
	gpwrdn = dwc2_readl(hsotg, GPWRDN);
	gpwrdn &= ~GPWRDN_PWRDNSWTCH;
	dwc2_writel(hsotg, gpwrdn, GPWRDN);
	udelay(10);

	 
	gpwrdn = dwc2_readl(hsotg, GPWRDN);
	gpwrdn &= ~GPWRDN_PWRDNRSTN;
	dwc2_writel(hsotg, gpwrdn, GPWRDN);
	udelay(10);

	 
	gpwrdn = dwc2_readl(hsotg, GPWRDN);
	gpwrdn |= GPWRDN_RESTORE;
	dwc2_writel(hsotg, gpwrdn, GPWRDN);
	udelay(10);

	 
	gpwrdn = dwc2_readl(hsotg, GPWRDN);
	gpwrdn &= ~GPWRDN_PWRDNCLMP;
	dwc2_writel(hsotg, gpwrdn, GPWRDN);
	udelay(50);

	if (!is_host && rem_wakeup)
		udelay(70);

	 
	gpwrdn = dwc2_readl(hsotg, GPWRDN);
	gpwrdn |= GPWRDN_PWRDNRSTN;
	dwc2_writel(hsotg, gpwrdn, GPWRDN);
	udelay(10);

	 
	gpwrdn = dwc2_readl(hsotg, GPWRDN);
	gpwrdn &= ~GPWRDN_PMUINTSEL;
	dwc2_writel(hsotg, gpwrdn, GPWRDN);
	udelay(10);

	 
	dwc2_restore_essential_regs(hsotg, rem_wakeup, is_host);

	 
	if (dwc2_hsotg_wait_bit_set(hsotg, GINTSTS, GINTSTS_RESTOREDONE,
				    20000)) {
		dev_dbg(hsotg->dev,
			"%s: Restore Done wasn't generated here\n",
			__func__);
	} else {
		dev_dbg(hsotg->dev, "restore done  generated here\n");

		 
		dwc2_writel(hsotg, GINTSTS_RESTOREDONE, GINTSTS);
	}
}

 
static void dwc2_wait_for_mode(struct dwc2_hsotg *hsotg,
			       bool host_mode)
{
	ktime_t start;
	ktime_t end;
	unsigned int timeout = 110;

	dev_vdbg(hsotg->dev, "Waiting for %s mode\n",
		 host_mode ? "host" : "device");

	start = ktime_get();

	while (1) {
		s64 ms;

		if (dwc2_is_host_mode(hsotg) == host_mode) {
			dev_vdbg(hsotg->dev, "%s mode set\n",
				 host_mode ? "Host" : "Device");
			break;
		}

		end = ktime_get();
		ms = ktime_to_ms(ktime_sub(end, start));

		if (ms >= (s64)timeout) {
			dev_warn(hsotg->dev, "%s: Couldn't set %s mode\n",
				 __func__, host_mode ? "host" : "device");
			break;
		}

		usleep_range(1000, 2000);
	}
}

 
static bool dwc2_iddig_filter_enabled(struct dwc2_hsotg *hsotg)
{
	u32 gsnpsid;
	u32 ghwcfg4;

	if (!dwc2_hw_is_otg(hsotg))
		return false;

	 
	ghwcfg4 = dwc2_readl(hsotg, GHWCFG4);
	if (!(ghwcfg4 & GHWCFG4_IDDIG_FILT_EN))
		return false;

	 
	gsnpsid = dwc2_readl(hsotg, GSNPSID);
	if (gsnpsid >= DWC2_CORE_REV_3_10a) {
		u32 gotgctl = dwc2_readl(hsotg, GOTGCTL);

		if (gotgctl & GOTGCTL_DBNCE_FLTR_BYPASS)
			return false;
	}

	return true;
}

 
int dwc2_enter_hibernation(struct dwc2_hsotg *hsotg, int is_host)
{
	if (is_host)
		return dwc2_host_enter_hibernation(hsotg);
	else
		return dwc2_gadget_enter_hibernation(hsotg);
}

 
int dwc2_exit_hibernation(struct dwc2_hsotg *hsotg, int rem_wakeup,
			  int reset, int is_host)
{
	if (is_host)
		return dwc2_host_exit_hibernation(hsotg, rem_wakeup, reset);
	else
		return dwc2_gadget_exit_hibernation(hsotg, rem_wakeup, reset);
}

 
int dwc2_core_reset(struct dwc2_hsotg *hsotg, bool skip_wait)
{
	u32 greset;
	bool wait_for_host_mode = false;

	dev_vdbg(hsotg->dev, "%s()\n", __func__);

	 
	if (dwc2_iddig_filter_enabled(hsotg)) {
		u32 gotgctl = dwc2_readl(hsotg, GOTGCTL);
		u32 gusbcfg = dwc2_readl(hsotg, GUSBCFG);

		if (!(gotgctl & GOTGCTL_CONID_B) ||
		    (gusbcfg & GUSBCFG_FORCEHOSTMODE)) {
			wait_for_host_mode = true;
		}
	}

	 
	greset = dwc2_readl(hsotg, GRSTCTL);
	greset |= GRSTCTL_CSFTRST;
	dwc2_writel(hsotg, greset, GRSTCTL);

	if ((hsotg->hw_params.snpsid & DWC2_CORE_REV_MASK) <
		(DWC2_CORE_REV_4_20a & DWC2_CORE_REV_MASK)) {
		if (dwc2_hsotg_wait_bit_clear(hsotg, GRSTCTL,
					      GRSTCTL_CSFTRST, 10000)) {
			dev_warn(hsotg->dev, "%s: HANG! Soft Reset timeout GRSTCTL_CSFTRST\n",
				 __func__);
			return -EBUSY;
		}
	} else {
		if (dwc2_hsotg_wait_bit_set(hsotg, GRSTCTL,
					    GRSTCTL_CSFTRST_DONE, 10000)) {
			dev_warn(hsotg->dev, "%s: HANG! Soft Reset timeout GRSTCTL_CSFTRST_DONE\n",
				 __func__);
			return -EBUSY;
		}
		greset = dwc2_readl(hsotg, GRSTCTL);
		greset &= ~GRSTCTL_CSFTRST;
		greset |= GRSTCTL_CSFTRST_DONE;
		dwc2_writel(hsotg, greset, GRSTCTL);
	}

	 
	dwc2_clear_fifo_map(hsotg);

	 
	if (dwc2_hsotg_wait_bit_set(hsotg, GRSTCTL, GRSTCTL_AHBIDLE, 10000)) {
		dev_warn(hsotg->dev, "%s: HANG! AHB Idle timeout GRSTCTL GRSTCTL_AHBIDLE\n",
			 __func__);
		return -EBUSY;
	}

	if (wait_for_host_mode && !skip_wait)
		dwc2_wait_for_mode(hsotg, true);

	return 0;
}

 
void dwc2_force_mode(struct dwc2_hsotg *hsotg, bool host)
{
	u32 gusbcfg;
	u32 set;
	u32 clear;

	dev_dbg(hsotg->dev, "Forcing mode to %s\n", host ? "host" : "device");

	 
	if (!dwc2_hw_is_otg(hsotg))
		return;

	 
	if (WARN_ON(host && hsotg->dr_mode == USB_DR_MODE_PERIPHERAL))
		return;

	if (WARN_ON(!host && hsotg->dr_mode == USB_DR_MODE_HOST))
		return;

	gusbcfg = dwc2_readl(hsotg, GUSBCFG);

	set = host ? GUSBCFG_FORCEHOSTMODE : GUSBCFG_FORCEDEVMODE;
	clear = host ? GUSBCFG_FORCEDEVMODE : GUSBCFG_FORCEHOSTMODE;

	gusbcfg &= ~clear;
	gusbcfg |= set;
	dwc2_writel(hsotg, gusbcfg, GUSBCFG);

	dwc2_wait_for_mode(hsotg, host);
	return;
}

 
static void dwc2_clear_force_mode(struct dwc2_hsotg *hsotg)
{
	u32 gusbcfg;

	if (!dwc2_hw_is_otg(hsotg))
		return;

	dev_dbg(hsotg->dev, "Clearing force mode bits\n");

	gusbcfg = dwc2_readl(hsotg, GUSBCFG);
	gusbcfg &= ~GUSBCFG_FORCEHOSTMODE;
	gusbcfg &= ~GUSBCFG_FORCEDEVMODE;
	dwc2_writel(hsotg, gusbcfg, GUSBCFG);

	if (dwc2_iddig_filter_enabled(hsotg))
		msleep(100);
}

 
void dwc2_force_dr_mode(struct dwc2_hsotg *hsotg)
{
	switch (hsotg->dr_mode) {
	case USB_DR_MODE_HOST:
		 
		if (!dwc2_hw_is_otg(hsotg))
			msleep(50);

		break;
	case USB_DR_MODE_PERIPHERAL:
		dwc2_force_mode(hsotg, false);
		break;
	case USB_DR_MODE_OTG:
		dwc2_clear_force_mode(hsotg);
		break;
	default:
		dev_warn(hsotg->dev, "%s() Invalid dr_mode=%d\n",
			 __func__, hsotg->dr_mode);
		break;
	}
}

 
void dwc2_enable_acg(struct dwc2_hsotg *hsotg)
{
	if (hsotg->params.acg_enable) {
		u32 pcgcctl1 = dwc2_readl(hsotg, PCGCCTL1);

		dev_dbg(hsotg->dev, "Enabling Active Clock Gating\n");
		pcgcctl1 |= PCGCCTL1_GATEEN;
		dwc2_writel(hsotg, pcgcctl1, PCGCCTL1);
	}
}

 
void dwc2_dump_host_registers(struct dwc2_hsotg *hsotg)
{
#ifdef DEBUG
	u32 __iomem *addr;
	int i;

	dev_dbg(hsotg->dev, "Host Global Registers\n");
	addr = hsotg->regs + HCFG;
	dev_dbg(hsotg->dev, "HCFG	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, HCFG));
	addr = hsotg->regs + HFIR;
	dev_dbg(hsotg->dev, "HFIR	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, HFIR));
	addr = hsotg->regs + HFNUM;
	dev_dbg(hsotg->dev, "HFNUM	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, HFNUM));
	addr = hsotg->regs + HPTXSTS;
	dev_dbg(hsotg->dev, "HPTXSTS	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, HPTXSTS));
	addr = hsotg->regs + HAINT;
	dev_dbg(hsotg->dev, "HAINT	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, HAINT));
	addr = hsotg->regs + HAINTMSK;
	dev_dbg(hsotg->dev, "HAINTMSK	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, HAINTMSK));
	if (hsotg->params.dma_desc_enable) {
		addr = hsotg->regs + HFLBADDR;
		dev_dbg(hsotg->dev, "HFLBADDR @0x%08lX : 0x%08X\n",
			(unsigned long)addr, dwc2_readl(hsotg, HFLBADDR));
	}

	addr = hsotg->regs + HPRT0;
	dev_dbg(hsotg->dev, "HPRT0	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, HPRT0));

	for (i = 0; i < hsotg->params.host_channels; i++) {
		dev_dbg(hsotg->dev, "Host Channel %d Specific Registers\n", i);
		addr = hsotg->regs + HCCHAR(i);
		dev_dbg(hsotg->dev, "HCCHAR	 @0x%08lX : 0x%08X\n",
			(unsigned long)addr, dwc2_readl(hsotg, HCCHAR(i)));
		addr = hsotg->regs + HCSPLT(i);
		dev_dbg(hsotg->dev, "HCSPLT	 @0x%08lX : 0x%08X\n",
			(unsigned long)addr, dwc2_readl(hsotg, HCSPLT(i)));
		addr = hsotg->regs + HCINT(i);
		dev_dbg(hsotg->dev, "HCINT	 @0x%08lX : 0x%08X\n",
			(unsigned long)addr, dwc2_readl(hsotg, HCINT(i)));
		addr = hsotg->regs + HCINTMSK(i);
		dev_dbg(hsotg->dev, "HCINTMSK	 @0x%08lX : 0x%08X\n",
			(unsigned long)addr, dwc2_readl(hsotg, HCINTMSK(i)));
		addr = hsotg->regs + HCTSIZ(i);
		dev_dbg(hsotg->dev, "HCTSIZ	 @0x%08lX : 0x%08X\n",
			(unsigned long)addr, dwc2_readl(hsotg, HCTSIZ(i)));
		addr = hsotg->regs + HCDMA(i);
		dev_dbg(hsotg->dev, "HCDMA	 @0x%08lX : 0x%08X\n",
			(unsigned long)addr, dwc2_readl(hsotg, HCDMA(i)));
		if (hsotg->params.dma_desc_enable) {
			addr = hsotg->regs + HCDMAB(i);
			dev_dbg(hsotg->dev, "HCDMAB	 @0x%08lX : 0x%08X\n",
				(unsigned long)addr, dwc2_readl(hsotg,
								HCDMAB(i)));
		}
	}
#endif
}

 
void dwc2_dump_global_registers(struct dwc2_hsotg *hsotg)
{
#ifdef DEBUG
	u32 __iomem *addr;

	dev_dbg(hsotg->dev, "Core Global Registers\n");
	addr = hsotg->regs + GOTGCTL;
	dev_dbg(hsotg->dev, "GOTGCTL	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GOTGCTL));
	addr = hsotg->regs + GOTGINT;
	dev_dbg(hsotg->dev, "GOTGINT	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GOTGINT));
	addr = hsotg->regs + GAHBCFG;
	dev_dbg(hsotg->dev, "GAHBCFG	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GAHBCFG));
	addr = hsotg->regs + GUSBCFG;
	dev_dbg(hsotg->dev, "GUSBCFG	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GUSBCFG));
	addr = hsotg->regs + GRSTCTL;
	dev_dbg(hsotg->dev, "GRSTCTL	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GRSTCTL));
	addr = hsotg->regs + GINTSTS;
	dev_dbg(hsotg->dev, "GINTSTS	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GINTSTS));
	addr = hsotg->regs + GINTMSK;
	dev_dbg(hsotg->dev, "GINTMSK	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GINTMSK));
	addr = hsotg->regs + GRXSTSR;
	dev_dbg(hsotg->dev, "GRXSTSR	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GRXSTSR));
	addr = hsotg->regs + GRXFSIZ;
	dev_dbg(hsotg->dev, "GRXFSIZ	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GRXFSIZ));
	addr = hsotg->regs + GNPTXFSIZ;
	dev_dbg(hsotg->dev, "GNPTXFSIZ	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GNPTXFSIZ));
	addr = hsotg->regs + GNPTXSTS;
	dev_dbg(hsotg->dev, "GNPTXSTS	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GNPTXSTS));
	addr = hsotg->regs + GI2CCTL;
	dev_dbg(hsotg->dev, "GI2CCTL	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GI2CCTL));
	addr = hsotg->regs + GPVNDCTL;
	dev_dbg(hsotg->dev, "GPVNDCTL	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GPVNDCTL));
	addr = hsotg->regs + GGPIO;
	dev_dbg(hsotg->dev, "GGPIO	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GGPIO));
	addr = hsotg->regs + GUID;
	dev_dbg(hsotg->dev, "GUID	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GUID));
	addr = hsotg->regs + GSNPSID;
	dev_dbg(hsotg->dev, "GSNPSID	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GSNPSID));
	addr = hsotg->regs + GHWCFG1;
	dev_dbg(hsotg->dev, "GHWCFG1	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GHWCFG1));
	addr = hsotg->regs + GHWCFG2;
	dev_dbg(hsotg->dev, "GHWCFG2	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GHWCFG2));
	addr = hsotg->regs + GHWCFG3;
	dev_dbg(hsotg->dev, "GHWCFG3	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GHWCFG3));
	addr = hsotg->regs + GHWCFG4;
	dev_dbg(hsotg->dev, "GHWCFG4	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GHWCFG4));
	addr = hsotg->regs + GLPMCFG;
	dev_dbg(hsotg->dev, "GLPMCFG	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GLPMCFG));
	addr = hsotg->regs + GPWRDN;
	dev_dbg(hsotg->dev, "GPWRDN	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GPWRDN));
	addr = hsotg->regs + GDFIFOCFG;
	dev_dbg(hsotg->dev, "GDFIFOCFG	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, GDFIFOCFG));
	addr = hsotg->regs + HPTXFSIZ;
	dev_dbg(hsotg->dev, "HPTXFSIZ	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, HPTXFSIZ));

	addr = hsotg->regs + PCGCTL;
	dev_dbg(hsotg->dev, "PCGCTL	 @0x%08lX : 0x%08X\n",
		(unsigned long)addr, dwc2_readl(hsotg, PCGCTL));
#endif
}

 
void dwc2_flush_tx_fifo(struct dwc2_hsotg *hsotg, const int num)
{
	u32 greset;

	dev_vdbg(hsotg->dev, "Flush Tx FIFO %d\n", num);

	 
	if (dwc2_hsotg_wait_bit_set(hsotg, GRSTCTL, GRSTCTL_AHBIDLE, 10000))
		dev_warn(hsotg->dev, "%s:  HANG! AHB Idle GRSCTL\n",
			 __func__);

	greset = GRSTCTL_TXFFLSH;
	greset |= num << GRSTCTL_TXFNUM_SHIFT & GRSTCTL_TXFNUM_MASK;
	dwc2_writel(hsotg, greset, GRSTCTL);

	if (dwc2_hsotg_wait_bit_clear(hsotg, GRSTCTL, GRSTCTL_TXFFLSH, 10000))
		dev_warn(hsotg->dev, "%s:  HANG! timeout GRSTCTL GRSTCTL_TXFFLSH\n",
			 __func__);

	 
	udelay(1);
}

 
void dwc2_flush_rx_fifo(struct dwc2_hsotg *hsotg)
{
	u32 greset;

	dev_vdbg(hsotg->dev, "%s()\n", __func__);

	 
	if (dwc2_hsotg_wait_bit_set(hsotg, GRSTCTL, GRSTCTL_AHBIDLE, 10000))
		dev_warn(hsotg->dev, "%s:  HANG! AHB Idle GRSCTL\n",
			 __func__);

	greset = GRSTCTL_RXFFLSH;
	dwc2_writel(hsotg, greset, GRSTCTL);

	 
	if (dwc2_hsotg_wait_bit_clear(hsotg, GRSTCTL, GRSTCTL_RXFFLSH, 10000))
		dev_warn(hsotg->dev, "%s: HANG! timeout GRSTCTL GRSTCTL_RXFFLSH\n",
			 __func__);

	 
	udelay(1);
}

bool dwc2_is_controller_alive(struct dwc2_hsotg *hsotg)
{
	if (dwc2_readl(hsotg, GSNPSID) == 0xffffffff)
		return false;
	else
		return true;
}

 
void dwc2_enable_global_interrupts(struct dwc2_hsotg *hsotg)
{
	u32 ahbcfg = dwc2_readl(hsotg, GAHBCFG);

	ahbcfg |= GAHBCFG_GLBL_INTR_EN;
	dwc2_writel(hsotg, ahbcfg, GAHBCFG);
}

 
void dwc2_disable_global_interrupts(struct dwc2_hsotg *hsotg)
{
	u32 ahbcfg = dwc2_readl(hsotg, GAHBCFG);

	ahbcfg &= ~GAHBCFG_GLBL_INTR_EN;
	dwc2_writel(hsotg, ahbcfg, GAHBCFG);
}

 
unsigned int dwc2_op_mode(struct dwc2_hsotg *hsotg)
{
	u32 ghwcfg2 = dwc2_readl(hsotg, GHWCFG2);

	return (ghwcfg2 & GHWCFG2_OP_MODE_MASK) >>
		GHWCFG2_OP_MODE_SHIFT;
}

 
bool dwc2_hw_is_otg(struct dwc2_hsotg *hsotg)
{
	unsigned int op_mode = dwc2_op_mode(hsotg);

	return (op_mode == GHWCFG2_OP_MODE_HNP_SRP_CAPABLE) ||
		(op_mode == GHWCFG2_OP_MODE_SRP_ONLY_CAPABLE) ||
		(op_mode == GHWCFG2_OP_MODE_NO_HNP_SRP_CAPABLE);
}

 
bool dwc2_hw_is_host(struct dwc2_hsotg *hsotg)
{
	unsigned int op_mode = dwc2_op_mode(hsotg);

	return (op_mode == GHWCFG2_OP_MODE_SRP_CAPABLE_HOST) ||
		(op_mode == GHWCFG2_OP_MODE_NO_SRP_CAPABLE_HOST);
}

 
bool dwc2_hw_is_device(struct dwc2_hsotg *hsotg)
{
	unsigned int op_mode = dwc2_op_mode(hsotg);

	return (op_mode == GHWCFG2_OP_MODE_SRP_CAPABLE_DEVICE) ||
		(op_mode == GHWCFG2_OP_MODE_NO_SRP_CAPABLE_DEVICE);
}

 
int dwc2_hsotg_wait_bit_set(struct dwc2_hsotg *hsotg, u32 offset, u32 mask,
			    u32 timeout)
{
	u32 i;

	for (i = 0; i < timeout; i++) {
		if (dwc2_readl(hsotg, offset) & mask)
			return 0;
		udelay(1);
	}

	return -ETIMEDOUT;
}

 
int dwc2_hsotg_wait_bit_clear(struct dwc2_hsotg *hsotg, u32 offset, u32 mask,
			      u32 timeout)
{
	u32 i;

	for (i = 0; i < timeout; i++) {
		if (!(dwc2_readl(hsotg, offset) & mask))
			return 0;
		udelay(1);
	}

	return -ETIMEDOUT;
}

 
void dwc2_init_fs_ls_pclk_sel(struct dwc2_hsotg *hsotg)
{
	u32 hcfg, val;

	if ((hsotg->hw_params.hs_phy_type == GHWCFG2_HS_PHY_TYPE_ULPI &&
	     hsotg->hw_params.fs_phy_type == GHWCFG2_FS_PHY_TYPE_DEDICATED &&
	     hsotg->params.ulpi_fs_ls) ||
	    hsotg->params.phy_type == DWC2_PHY_TYPE_PARAM_FS) {
		 
		val = HCFG_FSLSPCLKSEL_48_MHZ;
	} else {
		 
		val = HCFG_FSLSPCLKSEL_30_60_MHZ;
	}

	dev_dbg(hsotg->dev, "Initializing HCFG.FSLSPClkSel to %08x\n", val);
	hcfg = dwc2_readl(hsotg, HCFG);
	hcfg &= ~HCFG_FSLSPCLKSEL_MASK;
	hcfg |= val << HCFG_FSLSPCLKSEL_SHIFT;
	dwc2_writel(hsotg, hcfg, HCFG);
}

static int dwc2_fs_phy_init(struct dwc2_hsotg *hsotg, bool select_phy)
{
	u32 usbcfg, ggpio, i2cctl;
	int retval = 0;

	 
	if (select_phy) {
		dev_dbg(hsotg->dev, "FS PHY selected\n");

		usbcfg = dwc2_readl(hsotg, GUSBCFG);
		if (!(usbcfg & GUSBCFG_PHYSEL)) {
			usbcfg |= GUSBCFG_PHYSEL;
			dwc2_writel(hsotg, usbcfg, GUSBCFG);

			 
			retval = dwc2_core_reset(hsotg, false);

			if (retval) {
				dev_err(hsotg->dev,
					"%s: Reset failed, aborting", __func__);
				return retval;
			}
		}

		if (hsotg->params.activate_stm_fs_transceiver) {
			ggpio = dwc2_readl(hsotg, GGPIO);
			if (!(ggpio & GGPIO_STM32_OTG_GCCFG_PWRDWN)) {
				dev_dbg(hsotg->dev, "Activating transceiver\n");
				 
				ggpio |= GGPIO_STM32_OTG_GCCFG_PWRDWN;
				dwc2_writel(hsotg, ggpio, GGPIO);
			}
		}
	}

	 
	if (dwc2_is_host_mode(hsotg))
		dwc2_init_fs_ls_pclk_sel(hsotg);

	if (hsotg->params.i2c_enable) {
		dev_dbg(hsotg->dev, "FS PHY enabling I2C\n");

		 
		usbcfg = dwc2_readl(hsotg, GUSBCFG);
		usbcfg |= GUSBCFG_OTG_UTMI_FS_SEL;
		dwc2_writel(hsotg, usbcfg, GUSBCFG);

		 
		i2cctl = dwc2_readl(hsotg, GI2CCTL);
		i2cctl &= ~GI2CCTL_I2CDEVADDR_MASK;
		i2cctl |= 1 << GI2CCTL_I2CDEVADDR_SHIFT;
		i2cctl &= ~GI2CCTL_I2CEN;
		dwc2_writel(hsotg, i2cctl, GI2CCTL);
		i2cctl |= GI2CCTL_I2CEN;
		dwc2_writel(hsotg, i2cctl, GI2CCTL);
	}

	return retval;
}

static int dwc2_hs_phy_init(struct dwc2_hsotg *hsotg, bool select_phy)
{
	u32 usbcfg, usbcfg_old;
	int retval = 0;

	if (!select_phy)
		return 0;

	usbcfg = dwc2_readl(hsotg, GUSBCFG);
	usbcfg_old = usbcfg;

	 
	switch (hsotg->params.phy_type) {
	case DWC2_PHY_TYPE_PARAM_ULPI:
		 
		dev_dbg(hsotg->dev, "HS ULPI PHY selected\n");
		usbcfg |= GUSBCFG_ULPI_UTMI_SEL;
		usbcfg &= ~(GUSBCFG_PHYIF16 | GUSBCFG_DDRSEL);
		if (hsotg->params.phy_ulpi_ddr)
			usbcfg |= GUSBCFG_DDRSEL;

		 
		if (hsotg->params.oc_disable)
			usbcfg |= (GUSBCFG_ULPI_INT_VBUS_IND |
				   GUSBCFG_INDICATORPASSTHROUGH);
		break;
	case DWC2_PHY_TYPE_PARAM_UTMI:
		 
		dev_dbg(hsotg->dev, "HS UTMI+ PHY selected\n");
		usbcfg &= ~(GUSBCFG_ULPI_UTMI_SEL | GUSBCFG_PHYIF16);
		if (hsotg->params.phy_utmi_width == 16)
			usbcfg |= GUSBCFG_PHYIF16;
		break;
	default:
		dev_err(hsotg->dev, "FS PHY selected at HS!\n");
		break;
	}

	if (usbcfg != usbcfg_old) {
		dwc2_writel(hsotg, usbcfg, GUSBCFG);

		 
		retval = dwc2_core_reset(hsotg, false);
		if (retval) {
			dev_err(hsotg->dev,
				"%s: Reset failed, aborting", __func__);
			return retval;
		}
	}

	return retval;
}

static void dwc2_set_turnaround_time(struct dwc2_hsotg *hsotg)
{
	u32 usbcfg;

	if (hsotg->params.phy_type != DWC2_PHY_TYPE_PARAM_UTMI)
		return;

	usbcfg = dwc2_readl(hsotg, GUSBCFG);

	usbcfg &= ~GUSBCFG_USBTRDTIM_MASK;
	if (hsotg->params.phy_utmi_width == 16)
		usbcfg |= 5 << GUSBCFG_USBTRDTIM_SHIFT;
	else
		usbcfg |= 9 << GUSBCFG_USBTRDTIM_SHIFT;

	dwc2_writel(hsotg, usbcfg, GUSBCFG);
}

int dwc2_phy_init(struct dwc2_hsotg *hsotg, bool select_phy)
{
	u32 usbcfg;
	u32 otgctl;
	int retval = 0;

	if ((hsotg->params.speed == DWC2_SPEED_PARAM_FULL ||
	     hsotg->params.speed == DWC2_SPEED_PARAM_LOW) &&
	    hsotg->params.phy_type == DWC2_PHY_TYPE_PARAM_FS) {
		 
		retval = dwc2_fs_phy_init(hsotg, select_phy);
		if (retval)
			return retval;
	} else {
		 
		retval = dwc2_hs_phy_init(hsotg, select_phy);
		if (retval)
			return retval;

		if (dwc2_is_device_mode(hsotg))
			dwc2_set_turnaround_time(hsotg);
	}

	if (hsotg->hw_params.hs_phy_type == GHWCFG2_HS_PHY_TYPE_ULPI &&
	    hsotg->hw_params.fs_phy_type == GHWCFG2_FS_PHY_TYPE_DEDICATED &&
	    hsotg->params.ulpi_fs_ls) {
		dev_dbg(hsotg->dev, "Setting ULPI FSLS\n");
		usbcfg = dwc2_readl(hsotg, GUSBCFG);
		usbcfg |= GUSBCFG_ULPI_FS_LS;
		usbcfg |= GUSBCFG_ULPI_CLK_SUSP_M;
		dwc2_writel(hsotg, usbcfg, GUSBCFG);
	} else {
		usbcfg = dwc2_readl(hsotg, GUSBCFG);
		usbcfg &= ~GUSBCFG_ULPI_FS_LS;
		usbcfg &= ~GUSBCFG_ULPI_CLK_SUSP_M;
		dwc2_writel(hsotg, usbcfg, GUSBCFG);
	}

	if (!hsotg->params.activate_ingenic_overcurrent_detection) {
		if (dwc2_is_host_mode(hsotg)) {
			otgctl = readl(hsotg->regs + GOTGCTL);
			otgctl |= GOTGCTL_VBVALOEN | GOTGCTL_VBVALOVAL;
			writel(otgctl, hsotg->regs + GOTGCTL);
		}
	}

	return retval;
}

MODULE_DESCRIPTION("DESIGNWARE HS OTG Core");
MODULE_AUTHOR("Synopsys, Inc.");
MODULE_LICENSE("Dual BSD/GPL");
