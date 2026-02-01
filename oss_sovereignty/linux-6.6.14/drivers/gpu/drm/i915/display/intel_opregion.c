 

#include <linux/acpi.h>
#include <linux/dmi.h>
#include <linux/firmware.h>
#include <acpi/video.h>

#include <drm/drm_edid.h>

#include "i915_drv.h"
#include "intel_acpi.h"
#include "intel_backlight.h"
#include "intel_display_types.h"
#include "intel_opregion.h"
#include "intel_pci_config.h"

#define OPREGION_HEADER_OFFSET 0
#define OPREGION_ACPI_OFFSET   0x100
#define   ACPI_CLID 0x01ac  
#define   ACPI_CDCK 0x01b0  
#define OPREGION_SWSCI_OFFSET  0x200
#define OPREGION_ASLE_OFFSET   0x300
#define OPREGION_VBT_OFFSET    0x400
#define OPREGION_ASLE_EXT_OFFSET	0x1C00

#define OPREGION_SIGNATURE "IntelGraphicsMem"
#define MBOX_ACPI		BIT(0)	 
#define MBOX_SWSCI		BIT(1)	 
#define MBOX_ASLE		BIT(2)	 
#define MBOX_ASLE_EXT		BIT(4)	 
#define MBOX_BACKLIGHT		BIT(5)	 

#define PCON_HEADLESS_SKU	BIT(13)

struct opregion_header {
	u8 signature[16];
	u32 size;
	struct {
		u8 rsvd;
		u8 revision;
		u8 minor;
		u8 major;
	}  __packed over;
	u8 bios_ver[32];
	u8 vbios_ver[16];
	u8 driver_ver[16];
	u32 mboxes;
	u32 driver_model;
	u32 pcon;
	u8 dver[32];
	u8 rsvd[124];
} __packed;

 
struct opregion_acpi {
	u32 drdy;        
	u32 csts;        
	u32 cevt;        
	u8 rsvd1[20];
	u32 didl[8];     
	u32 cpdl[8];     
	u32 cadl[8];     
	u32 nadl[8];     
	u32 aslp;        
	u32 tidx;        
	u32 chpd;        
	u32 clid;        
	u32 cdck;        
	u32 sxsw;        
	u32 evts;        
	u32 cnot;        
	u32 nrdy;        
	u32 did2[7];	 
	u32 cpd2[7];	 
	u8 rsvd2[4];
} __packed;

 
struct opregion_swsci {
	u32 scic;        
	u32 parm;        
	u32 dslp;        
	u8 rsvd[244];
} __packed;

 
struct opregion_asle {
	u32 ardy;        
	u32 aslc;        
	u32 tche;        
	u32 alsi;        
	u32 bclp;        
	u32 pfit;        
	u32 cblv;        
	u16 bclm[20];    
	u32 cpfm;        
	u32 epfm;        
	u8 plut[74];     
	u32 pfmb;        
	u32 cddv;        
	u32 pcft;        
	u32 srot;        
	u32 iuer;        
	u64 fdss;
	u32 fdsp;
	u32 stat;
	u64 rvda;	 
	u32 rvds;	 
	u8 rsvd[58];
} __packed;

 
struct opregion_asle_ext {
	u32 phed;	 
	u8 bddc[256];	 
	u8 rsvd[764];
} __packed;

 
#define ASLE_ARDY_READY		(1 << 0)
#define ASLE_ARDY_NOT_READY	(0 << 0)

 
#define ASLC_SET_ALS_ILLUM		(1 << 0)
#define ASLC_SET_BACKLIGHT		(1 << 1)
#define ASLC_SET_PFIT			(1 << 2)
#define ASLC_SET_PWM_FREQ		(1 << 3)
#define ASLC_SUPPORTED_ROTATION_ANGLES	(1 << 4)
#define ASLC_BUTTON_ARRAY		(1 << 5)
#define ASLC_CONVERTIBLE_INDICATOR	(1 << 6)
#define ASLC_DOCKING_INDICATOR		(1 << 7)
#define ASLC_ISCT_STATE_CHANGE		(1 << 8)
#define ASLC_REQ_MSK			0x1ff
 
#define ASLC_ALS_ILLUM_FAILED		(1 << 10)
#define ASLC_BACKLIGHT_FAILED		(1 << 12)
#define ASLC_PFIT_FAILED		(1 << 14)
#define ASLC_PWM_FREQ_FAILED		(1 << 16)
#define ASLC_ROTATION_ANGLES_FAILED	(1 << 18)
#define ASLC_BUTTON_ARRAY_FAILED	(1 << 20)
#define ASLC_CONVERTIBLE_FAILED		(1 << 22)
#define ASLC_DOCKING_FAILED		(1 << 24)
#define ASLC_ISCT_STATE_FAILED		(1 << 26)

 
#define ASLE_TCHE_ALS_EN	(1 << 0)
#define ASLE_TCHE_BLC_EN	(1 << 1)
#define ASLE_TCHE_PFIT_EN	(1 << 2)
#define ASLE_TCHE_PFMB_EN	(1 << 3)

 
#define ASLE_BCLP_VALID                (1<<31)
#define ASLE_BCLP_MSK          (~(1<<31))

 
#define ASLE_PFIT_VALID         (1<<31)
#define ASLE_PFIT_CENTER (1<<0)
#define ASLE_PFIT_STRETCH_TEXT (1<<1)
#define ASLE_PFIT_STRETCH_GFX (1<<2)

 
#define ASLE_PFMB_BRIGHTNESS_MASK (0xff)
#define ASLE_PFMB_BRIGHTNESS_VALID (1<<8)
#define ASLE_PFMB_PWM_MASK (0x7ffffe00)
#define ASLE_PFMB_PWM_VALID (1<<31)

#define ASLE_CBLV_VALID         (1<<31)

 
#define ASLE_IUER_DOCKING		(1 << 7)
#define ASLE_IUER_CONVERTIBLE		(1 << 6)
#define ASLE_IUER_ROTATION_LOCK_BTN	(1 << 4)
#define ASLE_IUER_VOLUME_DOWN_BTN	(1 << 3)
#define ASLE_IUER_VOLUME_UP_BTN		(1 << 2)
#define ASLE_IUER_WINDOWS_BTN		(1 << 1)
#define ASLE_IUER_POWER_BTN		(1 << 0)

#define ASLE_PHED_EDID_VALID_MASK	0x3

 
#define SWSCI_SCIC_INDICATOR		(1 << 0)
#define SWSCI_SCIC_MAIN_FUNCTION_SHIFT	1
#define SWSCI_SCIC_MAIN_FUNCTION_MASK	(0xf << 1)
#define SWSCI_SCIC_SUB_FUNCTION_SHIFT	8
#define SWSCI_SCIC_SUB_FUNCTION_MASK	(0xff << 8)
#define SWSCI_SCIC_EXIT_PARAMETER_SHIFT	8
#define SWSCI_SCIC_EXIT_PARAMETER_MASK	(0xff << 8)
#define SWSCI_SCIC_EXIT_STATUS_SHIFT	5
#define SWSCI_SCIC_EXIT_STATUS_MASK	(7 << 5)
#define SWSCI_SCIC_EXIT_STATUS_SUCCESS	1

#define SWSCI_FUNCTION_CODE(main, sub) \
	((main) << SWSCI_SCIC_MAIN_FUNCTION_SHIFT | \
	 (sub) << SWSCI_SCIC_SUB_FUNCTION_SHIFT)

 
#define SWSCI_GBDA			4
#define SWSCI_GBDA_SUPPORTED_CALLS	SWSCI_FUNCTION_CODE(SWSCI_GBDA, 0)
#define SWSCI_GBDA_REQUESTED_CALLBACKS	SWSCI_FUNCTION_CODE(SWSCI_GBDA, 1)
#define SWSCI_GBDA_BOOT_DISPLAY_PREF	SWSCI_FUNCTION_CODE(SWSCI_GBDA, 4)
#define SWSCI_GBDA_PANEL_DETAILS	SWSCI_FUNCTION_CODE(SWSCI_GBDA, 5)
#define SWSCI_GBDA_TV_STANDARD		SWSCI_FUNCTION_CODE(SWSCI_GBDA, 6)
#define SWSCI_GBDA_INTERNAL_GRAPHICS	SWSCI_FUNCTION_CODE(SWSCI_GBDA, 7)
#define SWSCI_GBDA_SPREAD_SPECTRUM	SWSCI_FUNCTION_CODE(SWSCI_GBDA, 10)

 
#define SWSCI_SBCB			6
#define SWSCI_SBCB_SUPPORTED_CALLBACKS	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 0)
#define SWSCI_SBCB_INIT_COMPLETION	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 1)
#define SWSCI_SBCB_PRE_HIRES_SET_MODE	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 3)
#define SWSCI_SBCB_POST_HIRES_SET_MODE	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 4)
#define SWSCI_SBCB_DISPLAY_SWITCH	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 5)
#define SWSCI_SBCB_SET_TV_FORMAT	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 6)
#define SWSCI_SBCB_ADAPTER_POWER_STATE	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 7)
#define SWSCI_SBCB_DISPLAY_POWER_STATE	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 8)
#define SWSCI_SBCB_SET_BOOT_DISPLAY	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 9)
#define SWSCI_SBCB_SET_PANEL_DETAILS	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 10)
#define SWSCI_SBCB_SET_INTERNAL_GFX	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 11)
#define SWSCI_SBCB_POST_HIRES_TO_DOS_FS	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 16)
#define SWSCI_SBCB_SUSPEND_RESUME	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 17)
#define SWSCI_SBCB_SET_SPREAD_SPECTRUM	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 18)
#define SWSCI_SBCB_POST_VBE_PM		SWSCI_FUNCTION_CODE(SWSCI_SBCB, 19)
#define SWSCI_SBCB_ENABLE_DISABLE_AUDIO	SWSCI_FUNCTION_CODE(SWSCI_SBCB, 21)

#define MAX_DSLP	1500

static int check_swsci_function(struct drm_i915_private *i915, u32 function)
{
	struct opregion_swsci *swsci = i915->display.opregion.swsci;
	u32 main_function, sub_function;

	if (!swsci)
		return -ENODEV;

	main_function = (function & SWSCI_SCIC_MAIN_FUNCTION_MASK) >>
		SWSCI_SCIC_MAIN_FUNCTION_SHIFT;
	sub_function = (function & SWSCI_SCIC_SUB_FUNCTION_MASK) >>
		SWSCI_SCIC_SUB_FUNCTION_SHIFT;

	 
	if (main_function == SWSCI_SBCB) {
		if ((i915->display.opregion.swsci_sbcb_sub_functions &
		     (1 << sub_function)) == 0)
			return -EINVAL;
	} else if (main_function == SWSCI_GBDA) {
		if ((i915->display.opregion.swsci_gbda_sub_functions &
		     (1 << sub_function)) == 0)
			return -EINVAL;
	}

	return 0;
}

static int swsci(struct drm_i915_private *dev_priv,
		 u32 function, u32 parm, u32 *parm_out)
{
	struct opregion_swsci *swsci = dev_priv->display.opregion.swsci;
	struct pci_dev *pdev = to_pci_dev(dev_priv->drm.dev);
	u32 scic, dslp;
	u16 swsci_val;
	int ret;

	ret = check_swsci_function(dev_priv, function);
	if (ret)
		return ret;

	 
	dslp = swsci->dslp;
	if (!dslp) {
		 
		dslp = 50;
	} else if (dslp > MAX_DSLP) {
		 
		DRM_INFO_ONCE("ACPI BIOS requests an excessive sleep of %u ms, "
			      "using %u ms instead\n", dslp, MAX_DSLP);
		dslp = MAX_DSLP;
	}

	 
	scic = swsci->scic;
	if (scic & SWSCI_SCIC_INDICATOR) {
		drm_dbg(&dev_priv->drm, "SWSCI request already in progress\n");
		return -EBUSY;
	}

	scic = function | SWSCI_SCIC_INDICATOR;

	swsci->parm = parm;
	swsci->scic = scic;

	 
	pci_read_config_word(pdev, SWSCI, &swsci_val);
	if (!(swsci_val & SWSCI_SCISEL) || (swsci_val & SWSCI_GSSCIE)) {
		swsci_val |= SWSCI_SCISEL;
		swsci_val &= ~SWSCI_GSSCIE;
		pci_write_config_word(pdev, SWSCI, swsci_val);
	}

	 
	swsci_val |= SWSCI_GSSCIE;
	pci_write_config_word(pdev, SWSCI, swsci_val);

	 
#define C (((scic = swsci->scic) & SWSCI_SCIC_INDICATOR) == 0)
	if (wait_for(C, dslp)) {
		drm_dbg(&dev_priv->drm, "SWSCI request timed out\n");
		return -ETIMEDOUT;
	}

	scic = (scic & SWSCI_SCIC_EXIT_STATUS_MASK) >>
		SWSCI_SCIC_EXIT_STATUS_SHIFT;

	 
	if (scic != SWSCI_SCIC_EXIT_STATUS_SUCCESS) {
		drm_dbg(&dev_priv->drm, "SWSCI request error %u\n", scic);
		return -EIO;
	}

	if (parm_out)
		*parm_out = swsci->parm;

	return 0;

#undef C
}

#define DISPLAY_TYPE_CRT			0
#define DISPLAY_TYPE_TV				1
#define DISPLAY_TYPE_EXTERNAL_FLAT_PANEL	2
#define DISPLAY_TYPE_INTERNAL_FLAT_PANEL	3

int intel_opregion_notify_encoder(struct intel_encoder *intel_encoder,
				  bool enable)
{
	struct drm_i915_private *dev_priv = to_i915(intel_encoder->base.dev);
	u32 parm = 0;
	u32 type = 0;
	u32 port;
	int ret;

	 
	if (!HAS_DDI(dev_priv))
		return 0;

	 
	ret = check_swsci_function(dev_priv, SWSCI_SBCB_DISPLAY_POWER_STATE);
	if (ret)
		return ret;

	if (intel_encoder->type == INTEL_OUTPUT_DSI)
		port = 0;
	else
		port = intel_encoder->port;

	if (port == PORT_E)  {
		port = 0;
	} else {
		parm |= 1 << port;
		port++;
	}

	 
	if (port > 4) {
		drm_dbg_kms(&dev_priv->drm,
			    "[ENCODER:%d:%s] port %c (index %u) out of bounds for display power state notification\n",
			    intel_encoder->base.base.id, intel_encoder->base.name,
			    port_name(intel_encoder->port), port);
		return -EINVAL;
	}

	if (!enable)
		parm |= 4 << 8;

	switch (intel_encoder->type) {
	case INTEL_OUTPUT_ANALOG:
		type = DISPLAY_TYPE_CRT;
		break;
	case INTEL_OUTPUT_DDI:
	case INTEL_OUTPUT_DP:
	case INTEL_OUTPUT_HDMI:
	case INTEL_OUTPUT_DP_MST:
		type = DISPLAY_TYPE_EXTERNAL_FLAT_PANEL;
		break;
	case INTEL_OUTPUT_EDP:
	case INTEL_OUTPUT_DSI:
		type = DISPLAY_TYPE_INTERNAL_FLAT_PANEL;
		break;
	default:
		drm_WARN_ONCE(&dev_priv->drm, 1,
			      "unsupported intel_encoder type %d\n",
			      intel_encoder->type);
		return -EINVAL;
	}

	parm |= type << (16 + port * 3);

	return swsci(dev_priv, SWSCI_SBCB_DISPLAY_POWER_STATE, parm, NULL);
}

static const struct {
	pci_power_t pci_power_state;
	u32 parm;
} power_state_map[] = {
	{ PCI_D0,	0x00 },
	{ PCI_D1,	0x01 },
	{ PCI_D2,	0x02 },
	{ PCI_D3hot,	0x04 },
	{ PCI_D3cold,	0x04 },
};

int intel_opregion_notify_adapter(struct drm_i915_private *dev_priv,
				  pci_power_t state)
{
	int i;

	if (!HAS_DDI(dev_priv))
		return 0;

	for (i = 0; i < ARRAY_SIZE(power_state_map); i++) {
		if (state == power_state_map[i].pci_power_state)
			return swsci(dev_priv, SWSCI_SBCB_ADAPTER_POWER_STATE,
				     power_state_map[i].parm, NULL);
	}

	return -EINVAL;
}

static u32 asle_set_backlight(struct drm_i915_private *dev_priv, u32 bclp)
{
	struct intel_connector *connector;
	struct drm_connector_list_iter conn_iter;
	struct opregion_asle *asle = dev_priv->display.opregion.asle;

	drm_dbg(&dev_priv->drm, "bclp = 0x%08x\n", bclp);

	if (acpi_video_get_backlight_type() == acpi_backlight_native) {
		drm_dbg_kms(&dev_priv->drm,
			    "opregion backlight request ignored\n");
		return 0;
	}

	if (!(bclp & ASLE_BCLP_VALID))
		return ASLC_BACKLIGHT_FAILED;

	bclp &= ASLE_BCLP_MSK;
	if (bclp > 255)
		return ASLC_BACKLIGHT_FAILED;

	drm_modeset_lock(&dev_priv->drm.mode_config.connection_mutex, NULL);

	 
	drm_dbg_kms(&dev_priv->drm, "updating opregion backlight %d/255\n",
		    bclp);
	drm_connector_list_iter_begin(&dev_priv->drm, &conn_iter);
	for_each_intel_connector_iter(connector, &conn_iter)
		intel_backlight_set_acpi(connector->base.state, bclp, 255);
	drm_connector_list_iter_end(&conn_iter);
	asle->cblv = DIV_ROUND_UP(bclp * 100, 255) | ASLE_CBLV_VALID;

	drm_modeset_unlock(&dev_priv->drm.mode_config.connection_mutex);


	return 0;
}

static u32 asle_set_als_illum(struct drm_i915_private *dev_priv, u32 alsi)
{
	 
	drm_dbg(&dev_priv->drm, "Illum is not supported\n");
	return ASLC_ALS_ILLUM_FAILED;
}

static u32 asle_set_pwm_freq(struct drm_i915_private *dev_priv, u32 pfmb)
{
	drm_dbg(&dev_priv->drm, "PWM freq is not supported\n");
	return ASLC_PWM_FREQ_FAILED;
}

static u32 asle_set_pfit(struct drm_i915_private *dev_priv, u32 pfit)
{
	 
	drm_dbg(&dev_priv->drm, "Pfit is not supported\n");
	return ASLC_PFIT_FAILED;
}

static u32 asle_set_supported_rotation_angles(struct drm_i915_private *dev_priv, u32 srot)
{
	drm_dbg(&dev_priv->drm, "SROT is not supported\n");
	return ASLC_ROTATION_ANGLES_FAILED;
}

static u32 asle_set_button_array(struct drm_i915_private *dev_priv, u32 iuer)
{
	if (!iuer)
		drm_dbg(&dev_priv->drm,
			"Button array event is not supported (nothing)\n");
	if (iuer & ASLE_IUER_ROTATION_LOCK_BTN)
		drm_dbg(&dev_priv->drm,
			"Button array event is not supported (rotation lock)\n");
	if (iuer & ASLE_IUER_VOLUME_DOWN_BTN)
		drm_dbg(&dev_priv->drm,
			"Button array event is not supported (volume down)\n");
	if (iuer & ASLE_IUER_VOLUME_UP_BTN)
		drm_dbg(&dev_priv->drm,
			"Button array event is not supported (volume up)\n");
	if (iuer & ASLE_IUER_WINDOWS_BTN)
		drm_dbg(&dev_priv->drm,
			"Button array event is not supported (windows)\n");
	if (iuer & ASLE_IUER_POWER_BTN)
		drm_dbg(&dev_priv->drm,
			"Button array event is not supported (power)\n");

	return ASLC_BUTTON_ARRAY_FAILED;
}

static u32 asle_set_convertible(struct drm_i915_private *dev_priv, u32 iuer)
{
	if (iuer & ASLE_IUER_CONVERTIBLE)
		drm_dbg(&dev_priv->drm,
			"Convertible is not supported (clamshell)\n");
	else
		drm_dbg(&dev_priv->drm,
			"Convertible is not supported (slate)\n");

	return ASLC_CONVERTIBLE_FAILED;
}

static u32 asle_set_docking(struct drm_i915_private *dev_priv, u32 iuer)
{
	if (iuer & ASLE_IUER_DOCKING)
		drm_dbg(&dev_priv->drm, "Docking is not supported (docked)\n");
	else
		drm_dbg(&dev_priv->drm,
			"Docking is not supported (undocked)\n");

	return ASLC_DOCKING_FAILED;
}

static u32 asle_isct_state(struct drm_i915_private *dev_priv)
{
	drm_dbg(&dev_priv->drm, "ISCT is not supported\n");
	return ASLC_ISCT_STATE_FAILED;
}

static void asle_work(struct work_struct *work)
{
	struct intel_opregion *opregion =
		container_of(work, struct intel_opregion, asle_work);
	struct drm_i915_private *dev_priv =
		container_of(opregion, struct drm_i915_private, display.opregion);
	struct opregion_asle *asle = dev_priv->display.opregion.asle;
	u32 aslc_stat = 0;
	u32 aslc_req;

	if (!asle)
		return;

	aslc_req = asle->aslc;

	if (!(aslc_req & ASLC_REQ_MSK)) {
		drm_dbg(&dev_priv->drm,
			"No request on ASLC interrupt 0x%08x\n", aslc_req);
		return;
	}

	if (aslc_req & ASLC_SET_ALS_ILLUM)
		aslc_stat |= asle_set_als_illum(dev_priv, asle->alsi);

	if (aslc_req & ASLC_SET_BACKLIGHT)
		aslc_stat |= asle_set_backlight(dev_priv, asle->bclp);

	if (aslc_req & ASLC_SET_PFIT)
		aslc_stat |= asle_set_pfit(dev_priv, asle->pfit);

	if (aslc_req & ASLC_SET_PWM_FREQ)
		aslc_stat |= asle_set_pwm_freq(dev_priv, asle->pfmb);

	if (aslc_req & ASLC_SUPPORTED_ROTATION_ANGLES)
		aslc_stat |= asle_set_supported_rotation_angles(dev_priv,
							asle->srot);

	if (aslc_req & ASLC_BUTTON_ARRAY)
		aslc_stat |= asle_set_button_array(dev_priv, asle->iuer);

	if (aslc_req & ASLC_CONVERTIBLE_INDICATOR)
		aslc_stat |= asle_set_convertible(dev_priv, asle->iuer);

	if (aslc_req & ASLC_DOCKING_INDICATOR)
		aslc_stat |= asle_set_docking(dev_priv, asle->iuer);

	if (aslc_req & ASLC_ISCT_STATE_CHANGE)
		aslc_stat |= asle_isct_state(dev_priv);

	asle->aslc = aslc_stat;
}

void intel_opregion_asle_intr(struct drm_i915_private *dev_priv)
{
	if (dev_priv->display.opregion.asle)
		queue_work(dev_priv->unordered_wq,
			   &dev_priv->display.opregion.asle_work);
}

#define ACPI_EV_DISPLAY_SWITCH (1<<0)
#define ACPI_EV_LID            (1<<1)
#define ACPI_EV_DOCK           (1<<2)

 
static int intel_opregion_video_event(struct notifier_block *nb,
				      unsigned long val, void *data)
{
	struct intel_opregion *opregion = container_of(nb, struct intel_opregion,
						       acpi_notifier);
	struct acpi_bus_event *event = data;
	struct opregion_acpi *acpi;
	int ret = NOTIFY_OK;

	if (strcmp(event->device_class, ACPI_VIDEO_CLASS) != 0)
		return NOTIFY_DONE;

	acpi = opregion->acpi;

	if (event->type == 0x80 && ((acpi->cevt & 1) == 0))
		ret = NOTIFY_BAD;

	acpi->csts = 0;

	return ret;
}

 

static void set_did(struct intel_opregion *opregion, int i, u32 val)
{
	if (i < ARRAY_SIZE(opregion->acpi->didl)) {
		opregion->acpi->didl[i] = val;
	} else {
		i -= ARRAY_SIZE(opregion->acpi->didl);

		if (WARN_ON(i >= ARRAY_SIZE(opregion->acpi->did2)))
			return;

		opregion->acpi->did2[i] = val;
	}
}

static void intel_didl_outputs(struct drm_i915_private *dev_priv)
{
	struct intel_opregion *opregion = &dev_priv->display.opregion;
	struct intel_connector *connector;
	struct drm_connector_list_iter conn_iter;
	int i = 0, max_outputs;

	 
	max_outputs = ARRAY_SIZE(opregion->acpi->didl) +
		ARRAY_SIZE(opregion->acpi->did2);

	intel_acpi_device_id_update(dev_priv);

	drm_connector_list_iter_begin(&dev_priv->drm, &conn_iter);
	for_each_intel_connector_iter(connector, &conn_iter) {
		if (i < max_outputs)
			set_did(opregion, i, connector->acpi_device_id);
		i++;
	}
	drm_connector_list_iter_end(&conn_iter);

	drm_dbg_kms(&dev_priv->drm, "%d outputs detected\n", i);

	if (i > max_outputs)
		drm_err(&dev_priv->drm,
			"More than %d outputs in connector list\n",
			max_outputs);

	 
	if (i < max_outputs)
		set_did(opregion, i, 0);
}

static void intel_setup_cadls(struct drm_i915_private *dev_priv)
{
	struct intel_opregion *opregion = &dev_priv->display.opregion;
	struct intel_connector *connector;
	struct drm_connector_list_iter conn_iter;
	int i = 0;

	 
	drm_connector_list_iter_begin(&dev_priv->drm, &conn_iter);
	for_each_intel_connector_iter(connector, &conn_iter) {
		if (i >= ARRAY_SIZE(opregion->acpi->cadl))
			break;
		opregion->acpi->cadl[i++] = connector->acpi_device_id;
	}
	drm_connector_list_iter_end(&conn_iter);

	 
	if (i < ARRAY_SIZE(opregion->acpi->cadl))
		opregion->acpi->cadl[i] = 0;
}

static void swsci_setup(struct drm_i915_private *dev_priv)
{
	struct intel_opregion *opregion = &dev_priv->display.opregion;
	bool requested_callbacks = false;
	u32 tmp;

	 
	opregion->swsci_gbda_sub_functions = 1;
	opregion->swsci_sbcb_sub_functions = 1;

	 
	if (swsci(dev_priv, SWSCI_GBDA_SUPPORTED_CALLS, 0, &tmp) == 0) {
		 
		tmp <<= 1;
		opregion->swsci_gbda_sub_functions |= tmp;
	}

	 
	if (swsci(dev_priv, SWSCI_GBDA_REQUESTED_CALLBACKS, 0, &tmp) == 0) {
		 
		opregion->swsci_sbcb_sub_functions |= tmp;
		requested_callbacks = true;
	}

	 
	if (swsci(dev_priv, SWSCI_SBCB_SUPPORTED_CALLBACKS, 0, &tmp) == 0) {
		 
		u32 low = tmp & 0x7ff;
		u32 high = tmp & ~0xfff;  
		tmp = (high << 4) | (low << 1) | 1;

		 
		if (requested_callbacks) {
			u32 req = opregion->swsci_sbcb_sub_functions;
			if ((req & tmp) != req)
				drm_dbg(&dev_priv->drm,
					"SWSCI BIOS requested (%08x) SBCB callbacks that are not supported (%08x)\n",
					req, tmp);
			 
			 
		} else {
			opregion->swsci_sbcb_sub_functions |= tmp;
		}
	}

	drm_dbg(&dev_priv->drm,
		"SWSCI GBDA callbacks %08x, SBCB callbacks %08x\n",
		opregion->swsci_gbda_sub_functions,
		opregion->swsci_sbcb_sub_functions);
}

static int intel_no_opregion_vbt_callback(const struct dmi_system_id *id)
{
	DRM_DEBUG_KMS("Falling back to manually reading VBT from "
		      "VBIOS ROM for %s\n", id->ident);
	return 1;
}

static const struct dmi_system_id intel_no_opregion_vbt[] = {
	{
		.callback = intel_no_opregion_vbt_callback,
		.ident = "ThinkCentre A57",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_MATCH(DMI_PRODUCT_NAME, "97027RG"),
		},
	},
	{ }
};

static int intel_load_vbt_firmware(struct drm_i915_private *dev_priv)
{
	struct intel_opregion *opregion = &dev_priv->display.opregion;
	const struct firmware *fw = NULL;
	const char *name = dev_priv->params.vbt_firmware;
	int ret;

	if (!name || !*name)
		return -ENOENT;

	ret = request_firmware(&fw, name, dev_priv->drm.dev);
	if (ret) {
		drm_err(&dev_priv->drm,
			"Requesting VBT firmware \"%s\" failed (%d)\n",
			name, ret);
		return ret;
	}

	if (intel_bios_is_valid_vbt(fw->data, fw->size)) {
		opregion->vbt_firmware = kmemdup(fw->data, fw->size, GFP_KERNEL);
		if (opregion->vbt_firmware) {
			drm_dbg_kms(&dev_priv->drm,
				    "Found valid VBT firmware \"%s\"\n", name);
			opregion->vbt = opregion->vbt_firmware;
			opregion->vbt_size = fw->size;
			ret = 0;
		} else {
			ret = -ENOMEM;
		}
	} else {
		drm_dbg_kms(&dev_priv->drm, "Invalid VBT firmware \"%s\"\n",
			    name);
		ret = -EINVAL;
	}

	release_firmware(fw);

	return ret;
}

int intel_opregion_setup(struct drm_i915_private *dev_priv)
{
	struct intel_opregion *opregion = &dev_priv->display.opregion;
	struct pci_dev *pdev = to_pci_dev(dev_priv->drm.dev);
	u32 asls, mboxes;
	char buf[sizeof(OPREGION_SIGNATURE)];
	int err = 0;
	void *base;
	const void *vbt;
	u32 vbt_size;

	BUILD_BUG_ON(sizeof(struct opregion_header) != 0x100);
	BUILD_BUG_ON(sizeof(struct opregion_acpi) != 0x100);
	BUILD_BUG_ON(sizeof(struct opregion_swsci) != 0x100);
	BUILD_BUG_ON(sizeof(struct opregion_asle) != 0x100);
	BUILD_BUG_ON(sizeof(struct opregion_asle_ext) != 0x400);

	pci_read_config_dword(pdev, ASLS, &asls);
	drm_dbg(&dev_priv->drm, "graphic opregion physical addr: 0x%x\n",
		asls);
	if (asls == 0) {
		drm_dbg(&dev_priv->drm, "ACPI OpRegion not supported!\n");
		return -ENOTSUPP;
	}

	INIT_WORK(&opregion->asle_work, asle_work);

	base = memremap(asls, OPREGION_SIZE, MEMREMAP_WB);
	if (!base)
		return -ENOMEM;

	memcpy(buf, base, sizeof(buf));

	if (memcmp(buf, OPREGION_SIGNATURE, 16)) {
		drm_dbg(&dev_priv->drm, "opregion signature mismatch\n");
		err = -EINVAL;
		goto err_out;
	}
	opregion->header = base;
	opregion->lid_state = base + ACPI_CLID;

	drm_dbg(&dev_priv->drm, "ACPI OpRegion version %u.%u.%u\n",
		opregion->header->over.major,
		opregion->header->over.minor,
		opregion->header->over.revision);

	mboxes = opregion->header->mboxes;
	if (mboxes & MBOX_ACPI) {
		drm_dbg(&dev_priv->drm, "Public ACPI methods supported\n");
		opregion->acpi = base + OPREGION_ACPI_OFFSET;
		 
		opregion->acpi->chpd = 1;
	}

	if (mboxes & MBOX_SWSCI) {
		u8 major = opregion->header->over.major;

		if (major >= 3) {
			drm_err(&dev_priv->drm, "SWSCI Mailbox #2 present for opregion v3.x, ignoring\n");
		} else {
			if (major >= 2)
				drm_dbg(&dev_priv->drm, "SWSCI Mailbox #2 present for opregion v2.x\n");
			drm_dbg(&dev_priv->drm, "SWSCI supported\n");
			opregion->swsci = base + OPREGION_SWSCI_OFFSET;
			swsci_setup(dev_priv);
		}
	}

	if (mboxes & MBOX_ASLE) {
		drm_dbg(&dev_priv->drm, "ASLE supported\n");
		opregion->asle = base + OPREGION_ASLE_OFFSET;

		opregion->asle->ardy = ASLE_ARDY_NOT_READY;
	}

	if (mboxes & MBOX_ASLE_EXT) {
		drm_dbg(&dev_priv->drm, "ASLE extension supported\n");
		opregion->asle_ext = base + OPREGION_ASLE_EXT_OFFSET;
	}

	if (mboxes & MBOX_BACKLIGHT) {
		drm_dbg(&dev_priv->drm, "Mailbox #2 for backlight present\n");
	}

	if (intel_load_vbt_firmware(dev_priv) == 0)
		goto out;

	if (dmi_check_system(intel_no_opregion_vbt))
		goto out;

	if (opregion->header->over.major >= 2 && opregion->asle &&
	    opregion->asle->rvda && opregion->asle->rvds) {
		resource_size_t rvda = opregion->asle->rvda;

		 
		if (opregion->header->over.major > 2 ||
		    opregion->header->over.minor >= 1) {
			drm_WARN_ON(&dev_priv->drm, rvda < OPREGION_SIZE);

			rvda += asls;
		}

		opregion->rvda = memremap(rvda, opregion->asle->rvds,
					  MEMREMAP_WB);

		vbt = opregion->rvda;
		vbt_size = opregion->asle->rvds;
		if (intel_bios_is_valid_vbt(vbt, vbt_size)) {
			drm_dbg_kms(&dev_priv->drm,
				    "Found valid VBT in ACPI OpRegion (RVDA)\n");
			opregion->vbt = vbt;
			opregion->vbt_size = vbt_size;
			goto out;
		} else {
			drm_dbg_kms(&dev_priv->drm,
				    "Invalid VBT in ACPI OpRegion (RVDA)\n");
			memunmap(opregion->rvda);
			opregion->rvda = NULL;
		}
	}

	vbt = base + OPREGION_VBT_OFFSET;
	 
	vbt_size = (mboxes & MBOX_ASLE_EXT) ?
		OPREGION_ASLE_EXT_OFFSET : OPREGION_SIZE;
	vbt_size -= OPREGION_VBT_OFFSET;
	if (intel_bios_is_valid_vbt(vbt, vbt_size)) {
		drm_dbg_kms(&dev_priv->drm,
			    "Found valid VBT in ACPI OpRegion (Mailbox #4)\n");
		opregion->vbt = vbt;
		opregion->vbt_size = vbt_size;
	} else {
		drm_dbg_kms(&dev_priv->drm,
			    "Invalid VBT in ACPI OpRegion (Mailbox #4)\n");
	}

out:
	return 0;

err_out:
	memunmap(base);
	return err;
}

static int intel_use_opregion_panel_type_callback(const struct dmi_system_id *id)
{
	DRM_INFO("Using panel type from OpRegion on %s\n", id->ident);
	return 1;
}

static const struct dmi_system_id intel_use_opregion_panel_type[] = {
	{
		.callback = intel_use_opregion_panel_type_callback,
		.ident = "Conrac GmbH IX45GM2",
		.matches = {DMI_MATCH(DMI_SYS_VENDOR, "Conrac GmbH"),
			    DMI_MATCH(DMI_PRODUCT_NAME, "IX45GM2"),
		},
	},
	{ }
};

int
intel_opregion_get_panel_type(struct drm_i915_private *dev_priv)
{
	u32 panel_details;
	int ret;

	ret = swsci(dev_priv, SWSCI_GBDA_PANEL_DETAILS, 0x0, &panel_details);
	if (ret)
		return ret;

	ret = (panel_details >> 8) & 0xff;
	if (ret > 0x10) {
		drm_dbg_kms(&dev_priv->drm,
			    "Invalid OpRegion panel type 0x%x\n", ret);
		return -EINVAL;
	}

	 
	if (ret == 0x0) {
		drm_dbg_kms(&dev_priv->drm, "No panel type in OpRegion\n");
		return -ENODEV;
	}

	 
	if (!dmi_check_system(intel_use_opregion_panel_type)) {
		drm_dbg_kms(&dev_priv->drm,
			    "Ignoring OpRegion panel type (%d)\n", ret - 1);
		return -ENODEV;
	}

	return ret - 1;
}

 
const struct drm_edid *intel_opregion_get_edid(struct intel_connector *intel_connector)
{
	struct drm_connector *connector = &intel_connector->base;
	struct drm_i915_private *i915 = to_i915(connector->dev);
	struct intel_opregion *opregion = &i915->display.opregion;
	const struct drm_edid *drm_edid;
	const void *edid;
	int len;

	if (!opregion->asle_ext)
		return NULL;

	edid = opregion->asle_ext->bddc;

	 
	len = (opregion->asle_ext->phed & ASLE_PHED_EDID_VALID_MASK) * 128;
	if (!len || !memchr_inv(edid, 0, len))
		return NULL;

	drm_edid = drm_edid_alloc(edid, len);

	if (!drm_edid_valid(drm_edid)) {
		drm_dbg_kms(&i915->drm, "Invalid EDID in ACPI OpRegion (Mailbox #5)\n");
		drm_edid_free(drm_edid);
		drm_edid = NULL;
	}

	return drm_edid;
}

bool intel_opregion_headless_sku(struct drm_i915_private *i915)
{
	struct intel_opregion *opregion = &i915->display.opregion;
	struct opregion_header *header = opregion->header;

	if (!header || header->over.major < 2 ||
	    (header->over.major == 2 && header->over.minor < 3))
		return false;

	return opregion->header->pcon & PCON_HEADLESS_SKU;
}

void intel_opregion_register(struct drm_i915_private *i915)
{
	struct intel_opregion *opregion = &i915->display.opregion;

	if (!opregion->header)
		return;

	if (opregion->acpi) {
		opregion->acpi_notifier.notifier_call =
			intel_opregion_video_event;
		register_acpi_notifier(&opregion->acpi_notifier);
	}

	intel_opregion_resume(i915);
}

static void intel_opregion_resume_display(struct drm_i915_private *i915)
{
	struct intel_opregion *opregion = &i915->display.opregion;

	if (opregion->acpi) {
		intel_didl_outputs(i915);
		intel_setup_cadls(i915);

		 
		opregion->acpi->csts = 0;
		opregion->acpi->drdy = 1;
	}

	if (opregion->asle) {
		opregion->asle->tche = ASLE_TCHE_BLC_EN;
		opregion->asle->ardy = ASLE_ARDY_READY;
	}

	 
	intel_dsm_get_bios_data_funcs_supported(i915);
}

void intel_opregion_resume(struct drm_i915_private *i915)
{
	struct intel_opregion *opregion = &i915->display.opregion;

	if (!opregion->header)
		return;

	if (HAS_DISPLAY(i915))
		intel_opregion_resume_display(i915);

	intel_opregion_notify_adapter(i915, PCI_D0);
}

static void intel_opregion_suspend_display(struct drm_i915_private *i915)
{
	struct intel_opregion *opregion = &i915->display.opregion;

	if (opregion->asle)
		opregion->asle->ardy = ASLE_ARDY_NOT_READY;

	cancel_work_sync(&i915->display.opregion.asle_work);

	if (opregion->acpi)
		opregion->acpi->drdy = 0;
}

void intel_opregion_suspend(struct drm_i915_private *i915, pci_power_t state)
{
	struct intel_opregion *opregion = &i915->display.opregion;

	if (!opregion->header)
		return;

	intel_opregion_notify_adapter(i915, state);

	if (HAS_DISPLAY(i915))
		intel_opregion_suspend_display(i915);
}

void intel_opregion_unregister(struct drm_i915_private *i915)
{
	struct intel_opregion *opregion = &i915->display.opregion;

	intel_opregion_suspend(i915, PCI_D1);

	if (!opregion->header)
		return;

	if (opregion->acpi_notifier.notifier_call) {
		unregister_acpi_notifier(&opregion->acpi_notifier);
		opregion->acpi_notifier.notifier_call = NULL;
	}
}

void intel_opregion_cleanup(struct drm_i915_private *i915)
{
	struct intel_opregion *opregion = &i915->display.opregion;

	if (!opregion->header)
		return;

	 
	memunmap(opregion->header);
	if (opregion->rvda) {
		memunmap(opregion->rvda);
		opregion->rvda = NULL;
	}
	if (opregion->vbt_firmware) {
		kfree(opregion->vbt_firmware);
		opregion->vbt_firmware = NULL;
	}
	opregion->header = NULL;
	opregion->acpi = NULL;
	opregion->swsci = NULL;
	opregion->asle = NULL;
	opregion->asle_ext = NULL;
	opregion->vbt = NULL;
	opregion->lid_state = NULL;
}
