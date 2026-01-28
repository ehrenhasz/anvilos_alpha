#ifndef __LINUX_CDNS3_CORE_H
#define __LINUX_CDNS3_CORE_H
#include <linux/usb/otg.h>
#include <linux/usb/role.h>
struct cdns;
struct cdns_role_driver {
	int (*start)(struct cdns *cdns);
	void (*stop)(struct cdns *cdns);
	int (*suspend)(struct cdns *cdns, bool do_wakeup);
	int (*resume)(struct cdns *cdns, bool hibernated);
	const char *name;
#define CDNS_ROLE_STATE_INACTIVE	0
#define CDNS_ROLE_STATE_ACTIVE		1
	int state;
};
#define CDNS_XHCI_RESOURCES_NUM	2
struct cdns3_platform_data {
	int (*platform_suspend)(struct device *dev,
			bool suspend, bool wakeup);
	unsigned long quirks;
#define CDNS3_DEFAULT_PM_RUNTIME_ALLOW	BIT(0)
};
struct cdns {
	struct device			*dev;
	void __iomem			*xhci_regs;
	struct resource			xhci_res[CDNS_XHCI_RESOURCES_NUM];
	struct cdns3_usb_regs __iomem	*dev_regs;
	struct resource				otg_res;
	struct cdns3_otg_legacy_regs __iomem	*otg_v0_regs;
	struct cdns3_otg_regs __iomem		*otg_v1_regs;
	struct cdnsp_otg_regs __iomem		*otg_cdnsp_regs;
	struct cdns_otg_common_regs __iomem	*otg_regs;
	struct cdns_otg_irq_regs __iomem	*otg_irq_regs;
#define CDNS3_CONTROLLER_V0	0
#define CDNS3_CONTROLLER_V1	1
#define CDNSP_CONTROLLER_V2	2
	u32				version;
	bool				phyrst_a_enable;
	int				otg_irq;
	int				dev_irq;
	int				wakeup_irq;
	struct cdns_role_driver	*roles[USB_ROLE_DEVICE + 1];
	enum usb_role			role;
	struct platform_device		*host_dev;
	void				*gadget_dev;
	struct phy			*usb2_phy;
	struct phy			*usb3_phy;
	struct mutex			mutex;
	enum usb_dr_mode		dr_mode;
	struct usb_role_switch		*role_sw;
	bool				in_lpm;
	bool				wakeup_pending;
	struct cdns3_platform_data	*pdata;
	spinlock_t			lock;
	struct xhci_plat_priv		*xhci_plat_data;
	int (*gadget_init)(struct cdns *cdns);
};
int cdns_hw_role_switch(struct cdns *cdns);
int cdns_init(struct cdns *cdns);
int cdns_remove(struct cdns *cdns);
#ifdef CONFIG_PM_SLEEP
int cdns_resume(struct cdns *cdns);
int cdns_suspend(struct cdns *cdns);
void cdns_set_active(struct cdns *cdns, u8 set_active);
#else  
static inline int cdns_resume(struct cdns *cdns)
{ return 0; }
static inline void cdns_set_active(struct cdns *cdns, u8 set_active) { }
static inline int cdns_suspend(struct cdns *cdns)
{ return 0; }
#endif  
#endif  
