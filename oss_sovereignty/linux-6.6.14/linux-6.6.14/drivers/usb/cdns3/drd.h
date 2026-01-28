#ifndef __LINUX_CDNS3_DRD
#define __LINUX_CDNS3_DRD
#include <linux/usb/otg.h>
#include "core.h"
struct cdns3_otg_regs {
	__le32 did;
	__le32 rid;
	__le32 capabilities;
	__le32 reserved1;
	__le32 cmd;
	__le32 sts;
	__le32 state;
	__le32 reserved2;
	__le32 ien;
	__le32 ivect;
	__le32 refclk;
	__le32 tmr;
	__le32 reserved3[4];
	__le32 simulate;
	__le32 override;
	__le32 susp_ctrl;
	__le32 phyrst_cfg;
	__le32 anasts;
	__le32 adp_ramp_time;
	__le32 ctrl1;
	__le32 ctrl2;
};
struct cdns3_otg_legacy_regs {
	__le32 cmd;
	__le32 sts;
	__le32 state;
	__le32 refclk;
	__le32 ien;
	__le32 ivect;
	__le32 reserved1[3];
	__le32 tmr;
	__le32 reserved2[2];
	__le32 version;
	__le32 capabilities;
	__le32 reserved3[2];
	__le32 simulate;
	__le32 reserved4[5];
	__le32 ctrl1;
};
struct cdnsp_otg_regs {
	__le32 did;
	__le32 rid;
	__le32 cfgs1;
	__le32 cfgs2;
	__le32 cmd;
	__le32 sts;
	__le32 state;
	__le32 ien;
	__le32 ivect;
	__le32 tmr;
	__le32 simulate;
	__le32 adpbc_sts;
	__le32 adp_ramp_time;
	__le32 adpbc_ctrl1;
	__le32 adpbc_ctrl2;
	__le32 override;
	__le32 vbusvalid_dbnc_cfg;
	__le32 sessvalid_dbnc_cfg;
	__le32 susp_timing_ctrl;
};
#define OTG_CDNSP_DID	0x0004034E
struct cdns_otg_common_regs {
	__le32 cmd;
	__le32 sts;
	__le32 state;
};
struct cdns_otg_irq_regs {
	__le32 ien;
	__le32 ivect;
};
#define CDNS_RID(p)			((p) & GENMASK(15, 0))
#define CDNS_DID(p)			((p) & GENMASK(31, 0))
#define OTGCMD_DEV_BUS_REQ		BIT(0)
#define OTGCMD_HOST_BUS_REQ		BIT(1)
#define OTGCMD_OTG_EN			BIT(2)
#define OTGCMD_OTG_DIS			BIT(3)
#define OTGCMD_A_DEV_EN			BIT(4)
#define OTGCMD_A_DEV_DIS		BIT(5)
#define OTGCMD_DEV_BUS_DROP		BIT(8)
#define OTGCMD_HOST_BUS_DROP		BIT(9)
#define OTGCMD_DEV_POWER_OFF		BIT(11)
#define OTGCMD_HOST_POWER_OFF		BIT(12)
#define OTGIEN_ID_CHANGE_INT		BIT(0)
#define OTGIEN_VBUSVALID_RISE_INT	BIT(4)
#define OTGIEN_VBUSVALID_FALL_INT	BIT(5)
#define OTGSTS_ID_VALUE			BIT(0)
#define OTGSTS_VBUS_VALID		BIT(1)
#define OTGSTS_SESSION_VALID		BIT(2)
#define OTGSTS_DEV_ACTIVE		BIT(3)
#define OTGSTS_HOST_ACTIVE		BIT(4)
#define OTGSTS_OTG_NRDY_MASK		BIT(11)
#define OTGSTS_OTG_NRDY(p)		((p) & OTGSTS_OTG_NRDY_MASK)
#define OTGSTS_STRAP(p)			(((p) & GENMASK(14, 12)) >> 12)
#define OTGSTS_STRAP_NO_DEFAULT_CFG	0x00
#define OTGSTS_STRAP_HOST_OTG		0x01
#define OTGSTS_STRAP_HOST		0x02
#define OTGSTS_STRAP_GADGET		0x04
#define OTGSTS_CDNSP_STRAP_HOST		0x01
#define OTGSTS_CDNSP_STRAP_GADGET	0x02
#define OTGSTS_CDNS3_XHCI_READY		BIT(26)
#define OTGSTS_CDNSP_XHCI_READY		BIT(27)
#define OTGSTS_CDNS3_DEV_READY		BIT(27)
#define OTGSTS_CDNSP_DEV_READY		BIT(26)
#define OTGSTATE_DEV_STATE_MASK		GENMASK(2, 0)
#define OTGSTATE_HOST_STATE_MASK	GENMASK(5, 3)
#define OTGSTATE_HOST_STATE_IDLE	0x0
#define OTGSTATE_HOST_STATE_VBUS_FALL	0x7
#define OTGSTATE_HOST_STATE(p)		(((p) & OTGSTATE_HOST_STATE_MASK) >> 3)
#define OTGREFCLK_STB_CLK_SWITCH_EN	BIT(31)
#define OVERRIDE_IDPULLUP		BIT(0)
#define OVERRIDE_IDPULLUP_V0		BIT(24)
#define OVERRIDE_SESS_VLD_SEL		BIT(10)
#define PHYRST_CFG_PHYRST_A_ENABLE     BIT(0)
#define CDNS3_ID_PERIPHERAL		1
#define CDNS3_ID_HOST			0
bool cdns_is_host(struct cdns *cdns);
bool cdns_is_device(struct cdns *cdns);
int cdns_get_id(struct cdns *cdns);
int cdns_get_vbus(struct cdns *cdns);
void cdns_clear_vbus(struct cdns *cdns);
void cdns_set_vbus(struct cdns *cdns);
int cdns_drd_init(struct cdns *cdns);
int cdns_drd_exit(struct cdns *cdns);
int cdns_drd_update_mode(struct cdns *cdns);
int cdns_drd_gadget_on(struct cdns *cdns);
void cdns_drd_gadget_off(struct cdns *cdns);
int cdns_drd_host_on(struct cdns *cdns);
void cdns_drd_host_off(struct cdns *cdns);
bool cdns_power_is_lost(struct cdns *cdns);
#endif  
