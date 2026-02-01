 
 

#ifndef _XHCI_MTK_H_
#define _XHCI_MTK_H_

#include <linux/clk.h>
#include <linux/hashtable.h>
#include <linux/regulator/consumer.h>

#include "xhci.h"

#define BULK_CLKS_NUM	6
#define BULK_VREGS_NUM	2

 
#define SCH_EP_HASH_BITS	5

 
#define XHCI_MTK_MAX_ESIT	(1 << 6)
#define XHCI_MTK_BW_INDEX(x)	((x) & (XHCI_MTK_MAX_ESIT - 1))

 
struct mu3h_sch_tt {
	u32 fs_bus_bw[XHCI_MTK_MAX_ESIT];
	struct list_head ep_list;
};

 
struct mu3h_sch_bw_info {
	u32 bus_bw[XHCI_MTK_MAX_ESIT];
};

 
struct mu3h_sch_ep_info {
	u32 esit;
	u32 num_esit;
	u32 num_budget_microframes;
	u32 bw_cost_per_microframe;
	struct list_head endpoint;
	struct hlist_node hentry;
	struct list_head tt_endpoint;
	struct mu3h_sch_bw_info *bw_info;
	struct mu3h_sch_tt *sch_tt;
	u32 ep_type;
	u32 maxpkt;
	struct usb_host_endpoint *ep;
	enum usb_device_speed speed;
	bool allocated;
	 
	u32 offset;
	u32 repeat;
	u32 pkts;
	u32 cs_count;
	u32 burst_mode;
};

#define MU3C_U3_PORT_MAX 4
#define MU3C_U2_PORT_MAX 5

 
struct mu3c_ippc_regs {
	__le32 ip_pw_ctr0;
	__le32 ip_pw_ctr1;
	__le32 ip_pw_ctr2;
	__le32 ip_pw_ctr3;
	__le32 ip_pw_sts1;
	__le32 ip_pw_sts2;
	__le32 reserved0[3];
	__le32 ip_xhci_cap;
	__le32 reserved1[2];
	__le64 u3_ctrl_p[MU3C_U3_PORT_MAX];
	__le64 u2_ctrl_p[MU3C_U2_PORT_MAX];
	__le32 reserved2;
	__le32 u2_phy_pll;
	__le32 reserved3[33];  
};

struct xhci_hcd_mtk {
	struct device *dev;
	struct usb_hcd *hcd;
	struct mu3h_sch_bw_info *sch_array;
	struct list_head bw_ep_chk_list;
	DECLARE_HASHTABLE(sch_ep_hash, SCH_EP_HASH_BITS);
	struct mu3c_ippc_regs __iomem *ippc_regs;
	int num_u2_ports;
	int num_u3_ports;
	int u2p_dis_msk;
	int u3p_dis_msk;
	struct clk_bulk_data clks[BULK_CLKS_NUM];
	struct regulator_bulk_data supplies[BULK_VREGS_NUM];
	unsigned int has_ippc:1;
	unsigned int lpm_support:1;
	unsigned int u2_lpm_disable:1;
	 
	unsigned int uwk_en:1;
	struct regmap *uwk;
	u32 uwk_reg_base;
	u32 uwk_vers;
	 
	u32 rxfifo_depth;
};

static inline struct xhci_hcd_mtk *hcd_to_mtk(struct usb_hcd *hcd)
{
	return dev_get_drvdata(hcd->self.controller);
}

int xhci_mtk_sch_init(struct xhci_hcd_mtk *mtk);
void xhci_mtk_sch_exit(struct xhci_hcd_mtk *mtk);
int xhci_mtk_add_ep(struct usb_hcd *hcd, struct usb_device *udev,
		    struct usb_host_endpoint *ep);
int xhci_mtk_drop_ep(struct usb_hcd *hcd, struct usb_device *udev,
		     struct usb_host_endpoint *ep);
int xhci_mtk_check_bandwidth(struct usb_hcd *hcd, struct usb_device *udev);
void xhci_mtk_reset_bandwidth(struct usb_hcd *hcd, struct usb_device *udev);

#endif		 
