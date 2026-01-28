#ifndef __DRIVERS_USB_CHIPIDEA_CI_H
#define __DRIVERS_USB_CHIPIDEA_CI_H
#include <linux/list.h>
#include <linux/irqreturn.h>
#include <linux/usb.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg-fsm.h>
#include <linux/usb/otg.h>
#include <linux/usb/role.h>
#include <linux/ulpi/interface.h>
#define TD_PAGE_COUNT      5
#define CI_HDRC_PAGE_SIZE  4096ul  
#define ENDPT_MAX          32
#define CI_MAX_BUF_SIZE	(TD_PAGE_COUNT * CI_HDRC_PAGE_SIZE)
#define ID_ID				0x0
#define ID_HWGENERAL			0x4
#define ID_HWHOST			0x8
#define ID_HWDEVICE			0xc
#define ID_HWTXBUF			0x10
#define ID_HWRXBUF			0x14
#define ID_SBUSCFG			0x90
enum ci_hw_regs {
	CAP_CAPLENGTH,
	CAP_HCCPARAMS,
	CAP_DCCPARAMS,
	CAP_TESTMODE,
	CAP_LAST = CAP_TESTMODE,
	OP_USBCMD,
	OP_USBSTS,
	OP_USBINTR,
	OP_FRINDEX,
	OP_DEVICEADDR,
	OP_ENDPTLISTADDR,
	OP_TTCTRL,
	OP_BURSTSIZE,
	OP_ULPI_VIEWPORT,
	OP_PORTSC,
	OP_DEVLC,
	OP_OTGSC,
	OP_USBMODE,
	OP_ENDPTSETUPSTAT,
	OP_ENDPTPRIME,
	OP_ENDPTFLUSH,
	OP_ENDPTSTAT,
	OP_ENDPTCOMPLETE,
	OP_ENDPTCTRL,
	OP_LAST = OP_ENDPTCTRL + ENDPT_MAX / 2,
};
struct ci_hw_ep {
	struct usb_ep				ep;
	u8					dir;
	u8					num;
	u8					type;
	char					name[16];
	struct {
		struct list_head	queue;
		struct ci_hw_qh		*ptr;
		dma_addr_t		dma;
	}					qh;
	int					wedge;
	struct ci_hdrc				*ci;
	spinlock_t				*lock;
	struct dma_pool				*td_pool;
	struct td_node				*pending_td;
};
enum ci_role {
	CI_ROLE_HOST = 0,
	CI_ROLE_GADGET,
	CI_ROLE_END,
};
enum ci_revision {
	CI_REVISION_1X = 10,	 
	CI_REVISION_20 = 20,  
	CI_REVISION_21,  
	CI_REVISION_22,  
	CI_REVISION_23,  
	CI_REVISION_24,  
	CI_REVISION_25,  
	CI_REVISION_25_PLUS,  
	CI_REVISION_UNKNOWN = 99,  
};
struct ci_role_driver {
	int		(*start)(struct ci_hdrc *);
	void		(*stop)(struct ci_hdrc *);
	void		(*suspend)(struct ci_hdrc *ci);
	void		(*resume)(struct ci_hdrc *ci, bool power_lost);
	irqreturn_t	(*irq)(struct ci_hdrc *);
	const char	*name;
};
struct hw_bank {
	unsigned	lpm;
	resource_size_t	phys;
	void __iomem	*abs;
	void __iomem	*cap;
	void __iomem	*op;
	size_t		size;
	void __iomem	*regmap[OP_LAST + 1];
};
struct ci_hdrc {
	struct device			*dev;
	spinlock_t			lock;
	struct hw_bank			hw_bank;
	int				irq;
	struct ci_role_driver		*roles[CI_ROLE_END];
	enum ci_role			role;
	bool				is_otg;
	struct usb_otg			otg;
	struct otg_fsm			fsm;
	struct hrtimer			otg_fsm_hrtimer;
	ktime_t				hr_timeouts[NUM_OTG_FSM_TIMERS];
	unsigned			enabled_otg_timer_bits;
	enum otg_fsm_timer		next_otg_timer;
	struct usb_role_switch		*role_switch;
	struct work_struct		work;
	struct workqueue_struct		*wq;
	struct dma_pool			*qh_pool;
	struct dma_pool			*td_pool;
	struct usb_gadget		gadget;
	struct usb_gadget_driver	*driver;
	enum usb_device_state		resume_state;
	unsigned			hw_ep_max;
	struct ci_hw_ep			ci_hw_ep[ENDPT_MAX];
	u32				ep0_dir;
	struct ci_hw_ep			*ep0out, *ep0in;
	struct usb_request		*status;
	bool				setaddr;
	u8				address;
	u8				remote_wakeup;
	u8				suspended;
	u8				test_mode;
	struct ci_hdrc_platform_data	*platdata;
	int				vbus_active;
	struct ulpi			*ulpi;
	struct ulpi_ops 		ulpi_ops;
	struct phy			*phy;
	struct usb_phy			*usb_phy;
	struct usb_hcd			*hcd;
	bool				id_event;
	bool				b_sess_valid_event;
	bool				imx28_write_fix;
	bool				has_portsc_pec_bug;
	bool				supports_runtime_pm;
	bool				in_lpm;
	bool				wakeup_int;
	enum ci_revision		rev;
	struct mutex                    mutex;
};
static inline struct ci_role_driver *ci_role(struct ci_hdrc *ci)
{
	BUG_ON(ci->role >= CI_ROLE_END || !ci->roles[ci->role]);
	return ci->roles[ci->role];
}
static inline int ci_role_start(struct ci_hdrc *ci, enum ci_role role)
{
	int ret;
	if (role >= CI_ROLE_END)
		return -EINVAL;
	if (!ci->roles[role])
		return -ENXIO;
	ret = ci->roles[role]->start(ci);
	if (ret)
		return ret;
	ci->role = role;
	if (ci->usb_phy) {
		if (role == CI_ROLE_HOST)
			usb_phy_set_event(ci->usb_phy, USB_EVENT_ID);
		else
			usb_phy_set_event(ci->usb_phy, USB_EVENT_NONE);
	}
	return ret;
}
static inline void ci_role_stop(struct ci_hdrc *ci)
{
	enum ci_role role = ci->role;
	if (role == CI_ROLE_END)
		return;
	ci->role = CI_ROLE_END;
	ci->roles[role]->stop(ci);
	if (ci->usb_phy)
		usb_phy_set_event(ci->usb_phy, USB_EVENT_NONE);
}
static inline enum usb_role ci_role_to_usb_role(struct ci_hdrc *ci)
{
	if (ci->role == CI_ROLE_HOST)
		return USB_ROLE_HOST;
	else if (ci->role == CI_ROLE_GADGET && ci->vbus_active)
		return USB_ROLE_DEVICE;
	else
		return USB_ROLE_NONE;
}
static inline enum ci_role usb_role_to_ci_role(enum usb_role role)
{
	if (role == USB_ROLE_HOST)
		return CI_ROLE_HOST;
	else if (role == USB_ROLE_DEVICE)
		return CI_ROLE_GADGET;
	else
		return CI_ROLE_END;
}
static inline u32 hw_read_id_reg(struct ci_hdrc *ci, u32 offset, u32 mask)
{
	return ioread32(ci->hw_bank.abs + offset) & mask;
}
static inline void hw_write_id_reg(struct ci_hdrc *ci, u32 offset,
			    u32 mask, u32 data)
{
	if (~mask)
		data = (ioread32(ci->hw_bank.abs + offset) & ~mask)
			| (data & mask);
	iowrite32(data, ci->hw_bank.abs + offset);
}
static inline u32 hw_read(struct ci_hdrc *ci, enum ci_hw_regs reg, u32 mask)
{
	return ioread32(ci->hw_bank.regmap[reg]) & mask;
}
#ifdef CONFIG_SOC_IMX28
static inline void imx28_ci_writel(u32 val, volatile void __iomem *addr)
{
	__asm__ ("swp %0, %0, [%1]" : : "r"(val), "r"(addr));
}
#else
static inline void imx28_ci_writel(u32 val, volatile void __iomem *addr)
{
}
#endif
static inline void __hw_write(struct ci_hdrc *ci, u32 val,
		void __iomem *addr)
{
	if (ci->imx28_write_fix)
		imx28_ci_writel(val, addr);
	else
		iowrite32(val, addr);
}
static inline void hw_write(struct ci_hdrc *ci, enum ci_hw_regs reg,
			    u32 mask, u32 data)
{
	if (~mask)
		data = (ioread32(ci->hw_bank.regmap[reg]) & ~mask)
			| (data & mask);
	__hw_write(ci, data, ci->hw_bank.regmap[reg]);
}
static inline u32 hw_test_and_clear(struct ci_hdrc *ci, enum ci_hw_regs reg,
				    u32 mask)
{
	u32 val = ioread32(ci->hw_bank.regmap[reg]) & mask;
	__hw_write(ci, val, ci->hw_bank.regmap[reg]);
	return val;
}
static inline u32 hw_test_and_write(struct ci_hdrc *ci, enum ci_hw_regs reg,
				    u32 mask, u32 data)
{
	u32 val = hw_read(ci, reg, ~0);
	hw_write(ci, reg, mask, data);
	return (val & mask) >> __ffs(mask);
}
static inline bool ci_otg_is_fsm_mode(struct ci_hdrc *ci)
{
#ifdef CONFIG_USB_OTG_FSM
	struct usb_otg_caps *otg_caps = &ci->platdata->ci_otg_caps;
	return ci->is_otg && ci->roles[CI_ROLE_HOST] &&
		ci->roles[CI_ROLE_GADGET] && (otg_caps->srp_support ||
		otg_caps->hnp_support || otg_caps->adp_support);
#else
	return false;
#endif
}
int ci_ulpi_init(struct ci_hdrc *ci);
void ci_ulpi_exit(struct ci_hdrc *ci);
int ci_ulpi_resume(struct ci_hdrc *ci);
u32 hw_read_intr_enable(struct ci_hdrc *ci);
u32 hw_read_intr_status(struct ci_hdrc *ci);
int hw_device_reset(struct ci_hdrc *ci);
int hw_port_test_set(struct ci_hdrc *ci, u8 mode);
u8 hw_port_test_get(struct ci_hdrc *ci);
void hw_phymode_configure(struct ci_hdrc *ci);
void ci_platform_configure(struct ci_hdrc *ci);
void dbg_create_files(struct ci_hdrc *ci);
void dbg_remove_files(struct ci_hdrc *ci);
#endif	 
