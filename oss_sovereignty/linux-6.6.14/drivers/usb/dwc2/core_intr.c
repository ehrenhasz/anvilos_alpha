
 

 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include <linux/usb/hcd.h>
#include <linux/usb/ch11.h>

#include "core.h"
#include "hcd.h"

static const char *dwc2_op_state_str(struct dwc2_hsotg *hsotg)
{
	switch (hsotg->op_state) {
	case OTG_STATE_A_HOST:
		return "a_host";
	case OTG_STATE_A_SUSPEND:
		return "a_suspend";
	case OTG_STATE_A_PERIPHERAL:
		return "a_peripheral";
	case OTG_STATE_B_PERIPHERAL:
		return "b_peripheral";
	case OTG_STATE_B_HOST:
		return "b_host";
	default:
		return "unknown";
	}
}

 
static void dwc2_handle_usb_port_intr(struct dwc2_hsotg *hsotg)
{
	u32 hprt0 = dwc2_readl(hsotg, HPRT0);

	if (hprt0 & HPRT0_ENACHG) {
		hprt0 &= ~HPRT0_ENA;
		dwc2_writel(hsotg, hprt0, HPRT0);
	}
}

 
static void dwc2_handle_mode_mismatch_intr(struct dwc2_hsotg *hsotg)
{
	 
	dwc2_writel(hsotg, GINTSTS_MODEMIS, GINTSTS);

	dev_warn(hsotg->dev, "Mode Mismatch Interrupt: currently in %s mode\n",
		 dwc2_is_host_mode(hsotg) ? "Host" : "Device");
}

 
static void dwc2_handle_otg_intr(struct dwc2_hsotg *hsotg)
{
	u32 gotgint;
	u32 gotgctl;
	u32 gintmsk;

	gotgint = dwc2_readl(hsotg, GOTGINT);
	gotgctl = dwc2_readl(hsotg, GOTGCTL);
	dev_dbg(hsotg->dev, "++OTG Interrupt gotgint=%0x [%s]\n", gotgint,
		dwc2_op_state_str(hsotg));

	if (gotgint & GOTGINT_SES_END_DET) {
		dev_dbg(hsotg->dev,
			" ++OTG Interrupt: Session End Detected++ (%s)\n",
			dwc2_op_state_str(hsotg));
		gotgctl = dwc2_readl(hsotg, GOTGCTL);

		if (dwc2_is_device_mode(hsotg))
			dwc2_hsotg_disconnect(hsotg);

		if (hsotg->op_state == OTG_STATE_B_HOST) {
			hsotg->op_state = OTG_STATE_B_PERIPHERAL;
		} else {
			 
			if (gotgctl & GOTGCTL_DEVHNPEN) {
				dev_dbg(hsotg->dev, "Session End Detected\n");
				dev_err(hsotg->dev,
					"Device Not Connected/Responding!\n");
			}

			 
			 
			hsotg->lx_state = DWC2_L0;
		}

		gotgctl = dwc2_readl(hsotg, GOTGCTL);
		gotgctl &= ~GOTGCTL_DEVHNPEN;
		dwc2_writel(hsotg, gotgctl, GOTGCTL);
	}

	if (gotgint & GOTGINT_SES_REQ_SUC_STS_CHNG) {
		dev_dbg(hsotg->dev,
			" ++OTG Interrupt: Session Request Success Status Change++\n");
		gotgctl = dwc2_readl(hsotg, GOTGCTL);
		if (gotgctl & GOTGCTL_SESREQSCS) {
			if (hsotg->params.phy_type == DWC2_PHY_TYPE_PARAM_FS &&
			    hsotg->params.i2c_enable) {
				hsotg->srp_success = 1;
			} else {
				 
				gotgctl = dwc2_readl(hsotg, GOTGCTL);
				gotgctl &= ~GOTGCTL_SESREQ;
				dwc2_writel(hsotg, gotgctl, GOTGCTL);
			}
		}
	}

	if (gotgint & GOTGINT_HST_NEG_SUC_STS_CHNG) {
		 
		gotgctl = dwc2_readl(hsotg, GOTGCTL);
		 
		if (hsotg->hw_params.snpsid >= DWC2_CORE_REV_3_00a)
			udelay(100);
		if (gotgctl & GOTGCTL_HSTNEGSCS) {
			if (dwc2_is_host_mode(hsotg)) {
				hsotg->op_state = OTG_STATE_B_HOST;
				 
				gintmsk = dwc2_readl(hsotg, GINTMSK);
				gintmsk &= ~GINTSTS_SOF;
				dwc2_writel(hsotg, gintmsk, GINTMSK);

				 
				spin_unlock(&hsotg->lock);

				 
				dwc2_hcd_start(hsotg);
				spin_lock(&hsotg->lock);
				hsotg->op_state = OTG_STATE_B_HOST;
			}
		} else {
			gotgctl = dwc2_readl(hsotg, GOTGCTL);
			gotgctl &= ~(GOTGCTL_HNPREQ | GOTGCTL_DEVHNPEN);
			dwc2_writel(hsotg, gotgctl, GOTGCTL);
			dev_dbg(hsotg->dev, "HNP Failed\n");
			dev_err(hsotg->dev,
				"Device Not Connected/Responding\n");
		}
	}

	if (gotgint & GOTGINT_HST_NEG_DET) {
		 
		dev_dbg(hsotg->dev,
			" ++OTG Interrupt: Host Negotiation Detected++ (%s)\n",
			(dwc2_is_host_mode(hsotg) ? "Host" : "Device"));
		if (dwc2_is_device_mode(hsotg)) {
			dev_dbg(hsotg->dev, "a_suspend->a_peripheral (%d)\n",
				hsotg->op_state);
			spin_unlock(&hsotg->lock);
			dwc2_hcd_disconnect(hsotg, false);
			spin_lock(&hsotg->lock);
			hsotg->op_state = OTG_STATE_A_PERIPHERAL;
		} else {
			 
			gintmsk = dwc2_readl(hsotg, GINTMSK);
			gintmsk &= ~GINTSTS_SOF;
			dwc2_writel(hsotg, gintmsk, GINTMSK);
			spin_unlock(&hsotg->lock);
			dwc2_hcd_start(hsotg);
			spin_lock(&hsotg->lock);
			hsotg->op_state = OTG_STATE_A_HOST;
		}
	}

	if (gotgint & GOTGINT_A_DEV_TOUT_CHG)
		dev_dbg(hsotg->dev,
			" ++OTG Interrupt: A-Device Timeout Change++\n");
	if (gotgint & GOTGINT_DBNCE_DONE)
		dev_dbg(hsotg->dev, " ++OTG Interrupt: Debounce Done++\n");

	 
	dwc2_writel(hsotg, gotgint, GOTGINT);
}

 
static void dwc2_handle_conn_id_status_change_intr(struct dwc2_hsotg *hsotg)
{
	u32 gintmsk;

	 
	dwc2_writel(hsotg, GINTSTS_CONIDSTSCHNG, GINTSTS);

	 
	gintmsk = dwc2_readl(hsotg, GINTMSK);
	gintmsk &= ~GINTSTS_SOF;
	dwc2_writel(hsotg, gintmsk, GINTMSK);

	dev_dbg(hsotg->dev, " ++Connector ID Status Change Interrupt++  (%s)\n",
		dwc2_is_host_mode(hsotg) ? "Host" : "Device");

	 
	if (hsotg->wq_otg)
		queue_work(hsotg->wq_otg, &hsotg->wf_otg);
}

 
static void dwc2_handle_session_req_intr(struct dwc2_hsotg *hsotg)
{
	int ret;
	u32 hprt0;

	 
	dwc2_writel(hsotg, GINTSTS_SESSREQINT, GINTSTS);

	dev_dbg(hsotg->dev, "Session request interrupt - lx_state=%d\n",
		hsotg->lx_state);

	if (dwc2_is_device_mode(hsotg)) {
		if (hsotg->lx_state == DWC2_L2) {
			if (hsotg->in_ppd) {
				ret = dwc2_exit_partial_power_down(hsotg, 0,
								   true);
				if (ret)
					dev_err(hsotg->dev,
						"exit power_down failed\n");
			}

			 
			if (hsotg->params.power_down ==
			    DWC2_POWER_DOWN_PARAM_NONE && hsotg->bus_suspended)
				dwc2_gadget_exit_clock_gating(hsotg, 0);
		}

		 
		dwc2_hsotg_disconnect(hsotg);
	} else {
		 
		hprt0 = dwc2_read_hprt0(hsotg);
		hprt0 |= HPRT0_PWR;
		dwc2_writel(hsotg, hprt0, HPRT0);
		 
		dwc2_hcd_connect(hsotg);
	}
}

 
static void dwc2_wakeup_from_lpm_l1(struct dwc2_hsotg *hsotg)
{
	u32 glpmcfg;
	u32 i = 0;

	if (hsotg->lx_state != DWC2_L1) {
		dev_err(hsotg->dev, "Core isn't in DWC2_L1 state\n");
		return;
	}

	glpmcfg = dwc2_readl(hsotg, GLPMCFG);
	if (dwc2_is_device_mode(hsotg)) {
		dev_dbg(hsotg->dev, "Exit from L1 state\n");
		glpmcfg &= ~GLPMCFG_ENBLSLPM;
		glpmcfg &= ~GLPMCFG_HIRD_THRES_EN;
		dwc2_writel(hsotg, glpmcfg, GLPMCFG);

		do {
			glpmcfg = dwc2_readl(hsotg, GLPMCFG);

			if (!(glpmcfg & (GLPMCFG_COREL1RES_MASK |
					 GLPMCFG_L1RESUMEOK | GLPMCFG_SLPSTS)))
				break;

			udelay(1);
		} while (++i < 200);

		if (i == 200) {
			dev_err(hsotg->dev, "Failed to exit L1 sleep state in 200us.\n");
			return;
		}
		dwc2_gadget_init_lpm(hsotg);
	} else {
		 
		dev_err(hsotg->dev, "Host side LPM is not supported.\n");
		return;
	}

	 
	hsotg->lx_state = DWC2_L0;

	 
	call_gadget(hsotg, resume);
}

 
static void dwc2_handle_wakeup_detected_intr(struct dwc2_hsotg *hsotg)
{
	int ret;

	 
	dwc2_writel(hsotg, GINTSTS_WKUPINT, GINTSTS);

	dev_dbg(hsotg->dev, "++Resume or Remote Wakeup Detected Interrupt++\n");
	dev_dbg(hsotg->dev, "%s lxstate = %d\n", __func__, hsotg->lx_state);

	if (hsotg->lx_state == DWC2_L1) {
		dwc2_wakeup_from_lpm_l1(hsotg);
		return;
	}

	if (dwc2_is_device_mode(hsotg)) {
		dev_dbg(hsotg->dev, "DSTS=0x%0x\n",
			dwc2_readl(hsotg, DSTS));
		if (hsotg->lx_state == DWC2_L2) {
			if (hsotg->in_ppd) {
				u32 dctl = dwc2_readl(hsotg, DCTL);
				 
				dctl &= ~DCTL_RMTWKUPSIG;
				dwc2_writel(hsotg, dctl, DCTL);
				ret = dwc2_exit_partial_power_down(hsotg, 1,
								   true);
				if (ret)
					dev_err(hsotg->dev,
						"exit partial_power_down failed\n");
				call_gadget(hsotg, resume);
			}

			 
			if (hsotg->params.power_down ==
			    DWC2_POWER_DOWN_PARAM_NONE && hsotg->bus_suspended)
				dwc2_gadget_exit_clock_gating(hsotg, 0);
		} else {
			 
			hsotg->lx_state = DWC2_L0;
		}
	} else {
		if (hsotg->lx_state == DWC2_L2) {
			if (hsotg->in_ppd) {
				ret = dwc2_exit_partial_power_down(hsotg, 1,
								   true);
				if (ret)
					dev_err(hsotg->dev,
						"exit partial_power_down failed\n");
			}

			if (hsotg->params.power_down ==
			    DWC2_POWER_DOWN_PARAM_NONE && hsotg->bus_suspended)
				dwc2_host_exit_clock_gating(hsotg, 1);

			 
			if (hsotg->reset_phy_on_wake)
				dwc2_host_schedule_phy_reset(hsotg);

			mod_timer(&hsotg->wkp_timer,
				  jiffies + msecs_to_jiffies(71));
		} else {
			 
			hsotg->lx_state = DWC2_L0;
		}
	}
}

 
static void dwc2_handle_disconnect_intr(struct dwc2_hsotg *hsotg)
{
	dwc2_writel(hsotg, GINTSTS_DISCONNINT, GINTSTS);

	dev_dbg(hsotg->dev, "++Disconnect Detected Interrupt++ (%s) %s\n",
		dwc2_is_host_mode(hsotg) ? "Host" : "Device",
		dwc2_op_state_str(hsotg));

	if (hsotg->op_state == OTG_STATE_A_HOST)
		dwc2_hcd_disconnect(hsotg, false);
}

 
static void dwc2_handle_usb_suspend_intr(struct dwc2_hsotg *hsotg)
{
	u32 dsts;
	int ret;

	 
	dwc2_writel(hsotg, GINTSTS_USBSUSP, GINTSTS);

	dev_dbg(hsotg->dev, "USB SUSPEND\n");

	if (dwc2_is_device_mode(hsotg)) {
		 
		dsts = dwc2_readl(hsotg, DSTS);
		dev_dbg(hsotg->dev, "%s: DSTS=0x%0x\n", __func__, dsts);
		dev_dbg(hsotg->dev,
			"DSTS.Suspend Status=%d HWCFG4.Power Optimize=%d HWCFG4.Hibernation=%d\n",
			!!(dsts & DSTS_SUSPSTS),
			hsotg->hw_params.power_optimized,
			hsotg->hw_params.hibernation);

		 
		if (!dwc2_is_device_connected(hsotg)) {
			dev_dbg(hsotg->dev,
				"ignore suspend request before enumeration\n");
			return;
		}
		if (dsts & DSTS_SUSPSTS) {
			switch (hsotg->params.power_down) {
			case DWC2_POWER_DOWN_PARAM_PARTIAL:
				ret = dwc2_enter_partial_power_down(hsotg);
				if (ret)
					dev_err(hsotg->dev,
						"enter partial_power_down failed\n");

				udelay(100);

				 
				if (!IS_ERR_OR_NULL(hsotg->uphy))
					usb_phy_set_suspend(hsotg->uphy, true);
				break;
			case DWC2_POWER_DOWN_PARAM_HIBERNATION:
				ret = dwc2_enter_hibernation(hsotg, 0);
				if (ret)
					dev_err(hsotg->dev,
						"enter hibernation failed\n");
				break;
			case DWC2_POWER_DOWN_PARAM_NONE:
				 
				if (!hsotg->params.no_clock_gating)
					dwc2_gadget_enter_clock_gating(hsotg);
			}

			 
			hsotg->lx_state = DWC2_L2;

			 
			call_gadget(hsotg, suspend);
		}
	} else {
		if (hsotg->op_state == OTG_STATE_A_PERIPHERAL) {
			dev_dbg(hsotg->dev, "a_peripheral->a_host\n");

			 
			hsotg->lx_state = DWC2_L2;
			 
			spin_unlock(&hsotg->lock);
			dwc2_hcd_start(hsotg);
			spin_lock(&hsotg->lock);
			hsotg->op_state = OTG_STATE_A_HOST;
		}
	}
}

 
static void dwc2_handle_lpm_intr(struct dwc2_hsotg *hsotg)
{
	u32 glpmcfg;
	u32 pcgcctl;
	u32 hird;
	u32 hird_thres;
	u32 hird_thres_en;
	u32 enslpm;

	 
	dwc2_writel(hsotg, GINTSTS_LPMTRANRCVD, GINTSTS);

	glpmcfg = dwc2_readl(hsotg, GLPMCFG);

	if (!(glpmcfg & GLPMCFG_LPMCAP)) {
		dev_err(hsotg->dev, "Unexpected LPM interrupt\n");
		return;
	}

	hird = (glpmcfg & GLPMCFG_HIRD_MASK) >> GLPMCFG_HIRD_SHIFT;
	hird_thres = (glpmcfg & GLPMCFG_HIRD_THRES_MASK &
			~GLPMCFG_HIRD_THRES_EN) >> GLPMCFG_HIRD_THRES_SHIFT;
	hird_thres_en = glpmcfg & GLPMCFG_HIRD_THRES_EN;
	enslpm = glpmcfg & GLPMCFG_ENBLSLPM;

	if (dwc2_is_device_mode(hsotg)) {
		dev_dbg(hsotg->dev, "HIRD_THRES_EN = %d\n", hird_thres_en);

		if (hird_thres_en && hird >= hird_thres) {
			dev_dbg(hsotg->dev, "L1 with utmi_l1_suspend_n\n");
		} else if (enslpm) {
			dev_dbg(hsotg->dev, "L1 with utmi_sleep_n\n");
		} else {
			dev_dbg(hsotg->dev, "Entering Sleep with L1 Gating\n");

			pcgcctl = dwc2_readl(hsotg, PCGCTL);
			pcgcctl |= PCGCTL_ENBL_SLEEP_GATING;
			dwc2_writel(hsotg, pcgcctl, PCGCTL);
		}
		 
		udelay(10);

		glpmcfg = dwc2_readl(hsotg, GLPMCFG);

		if (glpmcfg & GLPMCFG_SLPSTS) {
			 
			hsotg->lx_state = DWC2_L1;
			dev_dbg(hsotg->dev,
				"Core is in L1 sleep glpmcfg=%08x\n", glpmcfg);

			 
			call_gadget(hsotg, suspend);
		}
	}
}

#define GINTMSK_COMMON	(GINTSTS_WKUPINT | GINTSTS_SESSREQINT |		\
			 GINTSTS_CONIDSTSCHNG | GINTSTS_OTGINT |	\
			 GINTSTS_MODEMIS | GINTSTS_DISCONNINT |		\
			 GINTSTS_USBSUSP | GINTSTS_PRTINT |		\
			 GINTSTS_LPMTRANRCVD)

 
static u32 dwc2_read_common_intr(struct dwc2_hsotg *hsotg)
{
	u32 gintsts;
	u32 gintmsk;
	u32 gahbcfg;
	u32 gintmsk_common = GINTMSK_COMMON;

	gintsts = dwc2_readl(hsotg, GINTSTS);
	gintmsk = dwc2_readl(hsotg, GINTMSK);
	gahbcfg = dwc2_readl(hsotg, GAHBCFG);

	 
	if (gintsts & gintmsk_common)
		dev_dbg(hsotg->dev, "gintsts=%08x  gintmsk=%08x\n",
			gintsts, gintmsk);

	if (gahbcfg & GAHBCFG_GLBL_INTR_EN)
		return gintsts & gintmsk & gintmsk_common;
	else
		return 0;
}

 
static inline void dwc_handle_gpwrdn_disc_det(struct dwc2_hsotg *hsotg,
					      u32 gpwrdn)
{
	u32 gpwrdn_tmp;

	 
	gpwrdn_tmp = dwc2_readl(hsotg, GPWRDN);
	gpwrdn_tmp &= ~GPWRDN_PWRDNSWTCH;
	dwc2_writel(hsotg, gpwrdn_tmp, GPWRDN);
	udelay(5);

	 
	gpwrdn_tmp = dwc2_readl(hsotg, GPWRDN);
	gpwrdn_tmp &= ~GPWRDN_PWRDNRSTN;
	dwc2_writel(hsotg, gpwrdn_tmp, GPWRDN);
	udelay(5);

	 
	gpwrdn_tmp = dwc2_readl(hsotg, GPWRDN);
	gpwrdn_tmp &= ~GPWRDN_PWRDNCLMP;
	dwc2_writel(hsotg, gpwrdn_tmp, GPWRDN);
	udelay(5);

	 
	gpwrdn_tmp = dwc2_readl(hsotg, GPWRDN);
	gpwrdn_tmp |= GPWRDN_PWRDNRSTN;
	dwc2_writel(hsotg, gpwrdn_tmp, GPWRDN);
	udelay(5);

	 
	gpwrdn_tmp = dwc2_readl(hsotg, GPWRDN);
	gpwrdn_tmp &= ~GPWRDN_PMUINTSEL;
	dwc2_writel(hsotg, gpwrdn_tmp, GPWRDN);

	 
	gpwrdn_tmp = dwc2_readl(hsotg, GPWRDN);
	gpwrdn_tmp &= ~GPWRDN_PMUACTV;
	dwc2_writel(hsotg, gpwrdn_tmp, GPWRDN);

	hsotg->hibernated = 0;
	hsotg->bus_suspended = 0;

	if (gpwrdn & GPWRDN_IDSTS) {
		hsotg->op_state = OTG_STATE_B_PERIPHERAL;
		dwc2_core_init(hsotg, false);
		dwc2_enable_global_interrupts(hsotg);
		dwc2_hsotg_core_init_disconnected(hsotg, false);
		dwc2_hsotg_core_connect(hsotg);
	} else {
		hsotg->op_state = OTG_STATE_A_HOST;

		 
		dwc2_core_init(hsotg, false);
		dwc2_enable_global_interrupts(hsotg);
		dwc2_hcd_start(hsotg);
	}
}

 
static int dwc2_handle_gpwrdn_intr(struct dwc2_hsotg *hsotg)
{
	u32 gpwrdn;
	int linestate;
	int ret = 0;

	gpwrdn = dwc2_readl(hsotg, GPWRDN);
	 
	dwc2_writel(hsotg, gpwrdn, GPWRDN);
	linestate = (gpwrdn & GPWRDN_LINESTATE_MASK) >> GPWRDN_LINESTATE_SHIFT;
	dev_dbg(hsotg->dev,
		"%s: dwc2_handle_gpwrdwn_intr called gpwrdn= %08x\n", __func__,
		gpwrdn);

	if ((gpwrdn & GPWRDN_DISCONN_DET) &&
	    (gpwrdn & GPWRDN_DISCONN_DET_MSK) && !linestate) {
		dev_dbg(hsotg->dev, "%s: GPWRDN_DISCONN_DET\n", __func__);
		 
		dwc_handle_gpwrdn_disc_det(hsotg, gpwrdn);
	} else if ((gpwrdn & GPWRDN_LNSTSCHG) &&
		   (gpwrdn & GPWRDN_LNSTSCHG_MSK) && linestate) {
		dev_dbg(hsotg->dev, "%s: GPWRDN_LNSTSCHG\n", __func__);
		if (hsotg->hw_params.hibernation &&
		    hsotg->hibernated) {
			if (gpwrdn & GPWRDN_IDSTS) {
				ret = dwc2_exit_hibernation(hsotg, 0, 0, 0);
				if (ret)
					dev_err(hsotg->dev,
						"exit hibernation failed.\n");
				call_gadget(hsotg, resume);
			} else {
				ret = dwc2_exit_hibernation(hsotg, 1, 0, 1);
				if (ret)
					dev_err(hsotg->dev,
						"exit hibernation failed.\n");
			}
		}
	} else if ((gpwrdn & GPWRDN_RST_DET) &&
		   (gpwrdn & GPWRDN_RST_DET_MSK)) {
		dev_dbg(hsotg->dev, "%s: GPWRDN_RST_DET\n", __func__);
		if (!linestate) {
			ret = dwc2_exit_hibernation(hsotg, 0, 1, 0);
			if (ret)
				dev_err(hsotg->dev,
					"exit hibernation failed.\n");
		}
	} else if ((gpwrdn & GPWRDN_STS_CHGINT) &&
		   (gpwrdn & GPWRDN_STS_CHGINT_MSK)) {
		dev_dbg(hsotg->dev, "%s: GPWRDN_STS_CHGINT\n", __func__);
		 
		dwc_handle_gpwrdn_disc_det(hsotg, gpwrdn);
	}

	return ret;
}

 
irqreturn_t dwc2_handle_common_intr(int irq, void *dev)
{
	struct dwc2_hsotg *hsotg = dev;
	u32 gintsts;
	irqreturn_t retval = IRQ_NONE;

	spin_lock(&hsotg->lock);

	if (!dwc2_is_controller_alive(hsotg)) {
		dev_warn(hsotg->dev, "Controller is dead\n");
		goto out;
	}

	 
	if (dwc2_is_device_mode(hsotg))
		hsotg->frame_number = (dwc2_readl(hsotg, DSTS)
				       & DSTS_SOFFN_MASK) >> DSTS_SOFFN_SHIFT;
	else
		hsotg->frame_number = (dwc2_readl(hsotg, HFNUM)
				       & HFNUM_FRNUM_MASK) >> HFNUM_FRNUM_SHIFT;

	gintsts = dwc2_read_common_intr(hsotg);
	if (gintsts & ~GINTSTS_PRTINT)
		retval = IRQ_HANDLED;

	 
	if (hsotg->hibernated) {
		dwc2_handle_gpwrdn_intr(hsotg);
		retval = IRQ_HANDLED;
		goto out;
	}

	if (gintsts & GINTSTS_MODEMIS)
		dwc2_handle_mode_mismatch_intr(hsotg);
	if (gintsts & GINTSTS_OTGINT)
		dwc2_handle_otg_intr(hsotg);
	if (gintsts & GINTSTS_CONIDSTSCHNG)
		dwc2_handle_conn_id_status_change_intr(hsotg);
	if (gintsts & GINTSTS_DISCONNINT)
		dwc2_handle_disconnect_intr(hsotg);
	if (gintsts & GINTSTS_SESSREQINT)
		dwc2_handle_session_req_intr(hsotg);
	if (gintsts & GINTSTS_WKUPINT)
		dwc2_handle_wakeup_detected_intr(hsotg);
	if (gintsts & GINTSTS_USBSUSP)
		dwc2_handle_usb_suspend_intr(hsotg);
	if (gintsts & GINTSTS_LPMTRANRCVD)
		dwc2_handle_lpm_intr(hsotg);

	if (gintsts & GINTSTS_PRTINT) {
		 
		if (dwc2_is_device_mode(hsotg)) {
			dev_dbg(hsotg->dev,
				" --Port interrupt received in Device mode--\n");
			dwc2_handle_usb_port_intr(hsotg);
			retval = IRQ_HANDLED;
		}
	}

out:
	spin_unlock(&hsotg->lock);
	return retval;
}
