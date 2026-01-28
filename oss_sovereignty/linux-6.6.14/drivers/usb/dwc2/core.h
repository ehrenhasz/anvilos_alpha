#ifndef __DWC2_CORE_H__
#define __DWC2_CORE_H__
#include <linux/acpi.h>
#include <linux/phy/phy.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
#include <linux/usb/phy.h>
#include "hw.h"
#define DWC2_TRACE_SCHEDULER		no_printk
#define DWC2_TRACE_SCHEDULER_VB		no_printk
#define dwc2_sch_dbg(hsotg, fmt, ...)					\
	DWC2_TRACE_SCHEDULER(pr_fmt("%s: SCH: " fmt),			\
			     dev_name(hsotg->dev), ##__VA_ARGS__)
#define dwc2_sch_vdbg(hsotg, fmt, ...)					\
	DWC2_TRACE_SCHEDULER_VB(pr_fmt("%s: SCH: " fmt),		\
				dev_name(hsotg->dev), ##__VA_ARGS__)
#define MAX_EPS_CHANNELS	16
static const char * const dwc2_hsotg_supply_names[] = {
	"vusb_d",                
	"vusb_a",                
};
#define DWC2_NUM_SUPPLIES ARRAY_SIZE(dwc2_hsotg_supply_names)
#define EP0_MPS_LIMIT   64
struct dwc2_hsotg;
struct dwc2_hsotg_req;
struct dwc2_hsotg_ep {
	struct usb_ep           ep;
	struct list_head        queue;
	struct dwc2_hsotg       *parent;
	struct dwc2_hsotg_req    *req;
	struct dentry           *debugfs;
	unsigned long           total_data;
	unsigned int            size_loaded;
	unsigned int            last_load;
	unsigned int            fifo_load;
	unsigned short          fifo_size;
	unsigned short		fifo_index;
	unsigned char           dir_in;
	unsigned char           map_dir;
	unsigned char           index;
	unsigned char           mc;
	u16                     interval;
	unsigned int            halted:1;
	unsigned int            periodic:1;
	unsigned int            isochronous:1;
	unsigned int            send_zlp:1;
	unsigned int            wedged:1;
	unsigned int            target_frame;
#define TARGET_FRAME_INITIAL   0xFFFFFFFF
	bool			frame_overrun;
	dma_addr_t		desc_list_dma;
	struct dwc2_dma_desc	*desc_list;
	u8			desc_count;
	unsigned int		next_desc;
	unsigned int		compl_desc;
	char                    name[10];
};
struct dwc2_hsotg_req {
	struct usb_request      req;
	struct list_head        queue;
	void *saved_req_buf;
};
#if IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL) || \
	IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
#define call_gadget(_hs, _entry) \
do { \
	if ((_hs)->gadget.speed != USB_SPEED_UNKNOWN && \
		(_hs)->driver && (_hs)->driver->_entry) { \
		spin_unlock(&_hs->lock); \
		(_hs)->driver->_entry(&(_hs)->gadget); \
		spin_lock(&_hs->lock); \
	} \
} while (0)
#else
#define call_gadget(_hs, _entry)	do {} while (0)
#endif
struct dwc2_hsotg;
struct dwc2_host_chan;
enum dwc2_lx_state {
	DWC2_L0,	 
	DWC2_L1,	 
	DWC2_L2,	 
	DWC2_L3,	 
};
enum dwc2_ep0_state {
	DWC2_EP0_SETUP,
	DWC2_EP0_DATA_IN,
	DWC2_EP0_DATA_OUT,
	DWC2_EP0_STATUS_IN,
	DWC2_EP0_STATUS_OUT,
};
struct dwc2_core_params {
	struct usb_otg_caps otg_caps;
	u8 phy_type;
#define DWC2_PHY_TYPE_PARAM_FS		0
#define DWC2_PHY_TYPE_PARAM_UTMI	1
#define DWC2_PHY_TYPE_PARAM_ULPI	2
	u8 speed;
#define DWC2_SPEED_PARAM_HIGH	0
#define DWC2_SPEED_PARAM_FULL	1
#define DWC2_SPEED_PARAM_LOW	2
	u8 phy_utmi_width;
	bool phy_ulpi_ddr;
	bool phy_ulpi_ext_vbus;
	bool enable_dynamic_fifo;
	bool en_multiple_tx_fifo;
	bool i2c_enable;
	bool acg_enable;
	bool ulpi_fs_ls;
	bool ts_dline;
	bool reload_ctl;
	bool uframe_sched;
	bool external_id_pin_ctl;
	int power_down;
#define DWC2_POWER_DOWN_PARAM_NONE		0
#define DWC2_POWER_DOWN_PARAM_PARTIAL		1
#define DWC2_POWER_DOWN_PARAM_HIBERNATION	2
	bool no_clock_gating;
	bool lpm;
	bool lpm_clock_gating;
	bool besl;
	bool hird_threshold_en;
	bool service_interval;
	u8 hird_threshold;
	bool activate_stm_fs_transceiver;
	bool activate_stm_id_vb_detection;
	bool activate_ingenic_overcurrent_detection;
	bool ipg_isoc_en;
	u16 max_packet_count;
	u32 max_transfer_size;
	u32 ahbcfg;
	u32 ref_clk_per;
	u16 sof_cnt_wkup_alert;
	bool host_dma;
	bool dma_desc_enable;
	bool dma_desc_fs_enable;
	bool host_support_fs_ls_low_power;
	bool host_ls_low_power_phy_clk;
	bool oc_disable;
	u8 host_channels;
	u16 host_rx_fifo_size;
	u16 host_nperio_tx_fifo_size;
	u16 host_perio_tx_fifo_size;
	bool g_dma;
	bool g_dma_desc;
	u32 g_rx_fifo_size;
	u32 g_np_tx_fifo_size;
	u32 g_tx_fifo_size[MAX_EPS_CHANNELS];
	bool change_speed_quirk;
};
struct dwc2_hw_params {
	unsigned op_mode:3;
	unsigned arch:2;
	unsigned dma_desc_enable:1;
	unsigned enable_dynamic_fifo:1;
	unsigned en_multiple_tx_fifo:1;
	unsigned rx_fifo_size:16;
	unsigned host_nperio_tx_fifo_size:16;
	unsigned dev_nperio_tx_fifo_size:16;
	unsigned host_perio_tx_fifo_size:16;
	unsigned nperio_tx_q_depth:3;
	unsigned host_perio_tx_q_depth:3;
	unsigned dev_token_q_depth:5;
	unsigned max_transfer_size:26;
	unsigned max_packet_count:11;
	unsigned host_channels:5;
	unsigned hs_phy_type:2;
	unsigned fs_phy_type:2;
	unsigned i2c_enable:1;
	unsigned acg_enable:1;
	unsigned num_dev_ep:4;
	unsigned num_dev_in_eps : 4;
	unsigned num_dev_perio_in_ep:4;
	unsigned total_fifo_size:16;
	unsigned power_optimized:1;
	unsigned hibernation:1;
	unsigned utmi_phy_data_width:2;
	unsigned lpm_mode:1;
	unsigned ipg_isoc_en:1;
	unsigned service_interval_mode:1;
	u32 snpsid;
	u32 dev_ep_dirs;
	u32 g_tx_fifo_size[MAX_EPS_CHANNELS];
};
#define DWC2_CTRL_BUFF_SIZE 8
struct dwc2_gregs_backup {
	u32 gotgctl;
	u32 gintmsk;
	u32 gahbcfg;
	u32 gusbcfg;
	u32 grxfsiz;
	u32 gnptxfsiz;
	u32 gi2cctl;
	u32 glpmcfg;
	u32 pcgcctl;
	u32 pcgcctl1;
	u32 gdfifocfg;
	u32 gpwrdn;
	bool valid;
};
struct dwc2_dregs_backup {
	u32 dcfg;
	u32 dctl;
	u32 daintmsk;
	u32 diepmsk;
	u32 doepmsk;
	u32 diepctl[MAX_EPS_CHANNELS];
	u32 dieptsiz[MAX_EPS_CHANNELS];
	u32 diepdma[MAX_EPS_CHANNELS];
	u32 doepctl[MAX_EPS_CHANNELS];
	u32 doeptsiz[MAX_EPS_CHANNELS];
	u32 doepdma[MAX_EPS_CHANNELS];
	u32 dtxfsiz[MAX_EPS_CHANNELS];
	bool valid;
};
struct dwc2_hregs_backup {
	u32 hcfg;
	u32 haintmsk;
	u32 hcintmsk[MAX_EPS_CHANNELS];
	u32 hprt0;
	u32 hfir;
	u32 hptxfsiz;
	bool valid;
};
#define DWC2_US_PER_UFRAME		125
#define DWC2_HS_PERIODIC_US_PER_UFRAME	100
#define DWC2_HS_SCHEDULE_UFRAMES	8
#define DWC2_HS_SCHEDULE_US		(DWC2_HS_SCHEDULE_UFRAMES * \
					 DWC2_HS_PERIODIC_US_PER_UFRAME)
#define DWC2_US_PER_SLICE	25
#define DWC2_SLICES_PER_UFRAME	(DWC2_US_PER_UFRAME / DWC2_US_PER_SLICE)
#define DWC2_ROUND_US_TO_SLICE(us) \
				(DIV_ROUND_UP((us), DWC2_US_PER_SLICE) * \
				 DWC2_US_PER_SLICE)
#define DWC2_LS_PERIODIC_US_PER_FRAME \
				900
#define DWC2_LS_PERIODIC_SLICES_PER_FRAME \
				(DWC2_LS_PERIODIC_US_PER_FRAME / \
				 DWC2_US_PER_SLICE)
#define DWC2_LS_SCHEDULE_FRAMES	1
#define DWC2_LS_SCHEDULE_SLICES	(DWC2_LS_SCHEDULE_FRAMES * \
				 DWC2_LS_PERIODIC_SLICES_PER_FRAME)
struct dwc2_hsotg {
	struct device *dev;
	void __iomem *regs;
	struct dwc2_hw_params hw_params;
	struct dwc2_core_params params;
	enum usb_otg_state op_state;
	enum usb_dr_mode dr_mode;
	struct usb_role_switch *role_sw;
	enum usb_dr_mode role_sw_default_mode;
	unsigned int hcd_enabled:1;
	unsigned int gadget_enabled:1;
	unsigned int ll_hw_enabled:1;
	unsigned int hibernated:1;
	unsigned int in_ppd:1;
	bool bus_suspended;
	unsigned int reset_phy_on_wake:1;
	unsigned int need_phy_for_wake:1;
	unsigned int phy_off_for_suspend:1;
	u16 frame_number;
	struct phy *phy;
	struct usb_phy *uphy;
	struct dwc2_hsotg_plat *plat;
	struct regulator_bulk_data supplies[DWC2_NUM_SUPPLIES];
	struct regulator *vbus_supply;
	struct regulator *usb33d;
	spinlock_t lock;
	void *priv;
	int     irq;
	struct clk *clk;
	struct clk *utmi_clk;
	struct reset_control *reset;
	struct reset_control *reset_ecc;
	unsigned int queuing_high_bandwidth:1;
	unsigned int srp_success:1;
	struct workqueue_struct *wq_otg;
	struct work_struct wf_otg;
	struct timer_list wkp_timer;
	enum dwc2_lx_state lx_state;
	struct dwc2_gregs_backup gr_backup;
	struct dwc2_dregs_backup dr_backup;
	struct dwc2_hregs_backup hr_backup;
	struct dentry *debug_root;
	struct debugfs_regset32 *regset;
	bool needs_byte_swap;
#define DWC2_CORE_REV_2_71a	0x4f54271a
#define DWC2_CORE_REV_2_72a     0x4f54272a
#define DWC2_CORE_REV_2_80a	0x4f54280a
#define DWC2_CORE_REV_2_90a	0x4f54290a
#define DWC2_CORE_REV_2_91a	0x4f54291a
#define DWC2_CORE_REV_2_92a	0x4f54292a
#define DWC2_CORE_REV_2_94a	0x4f54294a
#define DWC2_CORE_REV_3_00a	0x4f54300a
#define DWC2_CORE_REV_3_10a	0x4f54310a
#define DWC2_CORE_REV_4_00a	0x4f54400a
#define DWC2_CORE_REV_4_20a	0x4f54420a
#define DWC2_FS_IOT_REV_1_00a	0x5531100a
#define DWC2_HS_IOT_REV_1_00a	0x5532100a
#define DWC2_CORE_REV_MASK	0x0000ffff
#define DWC2_OTG_ID		0x4f540000
#define DWC2_FS_IOT_ID		0x55310000
#define DWC2_HS_IOT_ID		0x55320000
#if IS_ENABLED(CONFIG_USB_DWC2_HOST) || IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
	union dwc2_hcd_internal_flags {
		u32 d32;
		struct {
			unsigned port_connect_status_change:1;
			unsigned port_connect_status:1;
			unsigned port_reset_change:1;
			unsigned port_enable_change:1;
			unsigned port_suspend_change:1;
			unsigned port_over_current_change:1;
			unsigned port_l1_change:1;
			unsigned reserved:25;
		} b;
	} flags;
	struct list_head non_periodic_sched_inactive;
	struct list_head non_periodic_sched_waiting;
	struct list_head non_periodic_sched_active;
	struct list_head *non_periodic_qh_ptr;
	struct list_head periodic_sched_inactive;
	struct list_head periodic_sched_ready;
	struct list_head periodic_sched_assigned;
	struct list_head periodic_sched_queued;
	struct list_head split_order;
	u16 periodic_usecs;
	DECLARE_BITMAP(hs_periodic_bitmap, DWC2_HS_SCHEDULE_US);
	u16 periodic_qh_count;
	bool new_connection;
	u16 last_frame_num;
#ifdef CONFIG_USB_DWC2_TRACK_MISSED_SOFS
#define FRAME_NUM_ARRAY_SIZE 1000
	u16 *frame_num_array;
	u16 *last_frame_num_array;
	int frame_num_idx;
	int dumped_frame_num_array;
#endif
	struct list_head free_hc_list;
	int periodic_channels;
	int non_periodic_channels;
	int available_host_channels;
	struct dwc2_host_chan *hc_ptr_array[MAX_EPS_CHANNELS];
	u8 *status_buf;
	dma_addr_t status_buf_dma;
#define DWC2_HCD_STATUS_BUF_SIZE 64
	struct delayed_work start_work;
	struct delayed_work reset_work;
	struct work_struct phy_reset_work;
	u8 otg_port;
	u32 *frame_list;
	dma_addr_t frame_list_dma;
	u32 frame_list_sz;
	struct kmem_cache *desc_gen_cache;
	struct kmem_cache *desc_hsisoc_cache;
	struct kmem_cache *unaligned_cache;
#define DWC2_KMEM_UNALIGNED_BUF_SIZE 1024
#endif  
#if IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL) || \
	IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
	struct usb_gadget_driver *driver;
	int fifo_mem;
	unsigned int dedicated_fifos:1;
	unsigned char num_of_eps;
	u32 fifo_map;
	struct usb_request *ep0_reply;
	struct usb_request *ctrl_req;
	void *ep0_buff;
	void *ctrl_buff;
	enum dwc2_ep0_state ep0_state;
	unsigned delayed_status : 1;
	u8 test_mode;
	dma_addr_t setup_desc_dma[2];
	struct dwc2_dma_desc *setup_desc[2];
	dma_addr_t ctrl_in_desc_dma;
	struct dwc2_dma_desc *ctrl_in_desc;
	dma_addr_t ctrl_out_desc_dma;
	struct dwc2_dma_desc *ctrl_out_desc;
	struct usb_gadget gadget;
	unsigned int enabled:1;
	unsigned int connected:1;
	unsigned int remote_wakeup_allowed:1;
	struct dwc2_hsotg_ep *eps_in[MAX_EPS_CHANNELS];
	struct dwc2_hsotg_ep *eps_out[MAX_EPS_CHANNELS];
#endif  
};
static inline u32 dwc2_readl(struct dwc2_hsotg *hsotg, u32 offset)
{
	u32 val;
	val = readl(hsotg->regs + offset);
	if (hsotg->needs_byte_swap)
		return swab32(val);
	else
		return val;
}
static inline void dwc2_writel(struct dwc2_hsotg *hsotg, u32 value, u32 offset)
{
	if (hsotg->needs_byte_swap)
		writel(swab32(value), hsotg->regs + offset);
	else
		writel(value, hsotg->regs + offset);
#ifdef DWC2_LOG_WRITES
	pr_info("info:: wrote %08x to %p\n", value, hsotg->regs + offset);
#endif
}
static inline void dwc2_readl_rep(struct dwc2_hsotg *hsotg, u32 offset,
				  void *buffer, unsigned int count)
{
	if (count) {
		u32 *buf = buffer;
		do {
			u32 x = dwc2_readl(hsotg, offset);
			*buf++ = x;
		} while (--count);
	}
}
static inline void dwc2_writel_rep(struct dwc2_hsotg *hsotg, u32 offset,
				   const void *buffer, unsigned int count)
{
	if (count) {
		const u32 *buf = buffer;
		do {
			dwc2_writel(hsotg, *buf++, offset);
		} while (--count);
	}
}
enum dwc2_halt_status {
	DWC2_HC_XFER_NO_HALT_STATUS,
	DWC2_HC_XFER_COMPLETE,
	DWC2_HC_XFER_URB_COMPLETE,
	DWC2_HC_XFER_ACK,
	DWC2_HC_XFER_NAK,
	DWC2_HC_XFER_NYET,
	DWC2_HC_XFER_STALL,
	DWC2_HC_XFER_XACT_ERR,
	DWC2_HC_XFER_FRAME_OVERRUN,
	DWC2_HC_XFER_BABBLE_ERR,
	DWC2_HC_XFER_DATA_TOGGLE_ERR,
	DWC2_HC_XFER_AHB_ERR,
	DWC2_HC_XFER_PERIODIC_INCOMPLETE,
	DWC2_HC_XFER_URB_DEQUEUE,
};
static inline bool dwc2_is_iot(struct dwc2_hsotg *hsotg)
{
	return (hsotg->hw_params.snpsid & 0xfff00000) == 0x55300000;
}
static inline bool dwc2_is_fs_iot(struct dwc2_hsotg *hsotg)
{
	return (hsotg->hw_params.snpsid & 0xffff0000) == 0x55310000;
}
static inline bool dwc2_is_hs_iot(struct dwc2_hsotg *hsotg)
{
	return (hsotg->hw_params.snpsid & 0xffff0000) == 0x55320000;
}
int dwc2_core_reset(struct dwc2_hsotg *hsotg, bool skip_wait);
int dwc2_enter_partial_power_down(struct dwc2_hsotg *hsotg);
int dwc2_exit_partial_power_down(struct dwc2_hsotg *hsotg, int rem_wakeup,
				 bool restore);
int dwc2_enter_hibernation(struct dwc2_hsotg *hsotg, int is_host);
int dwc2_exit_hibernation(struct dwc2_hsotg *hsotg, int rem_wakeup,
		int reset, int is_host);
void dwc2_init_fs_ls_pclk_sel(struct dwc2_hsotg *hsotg);
int dwc2_phy_init(struct dwc2_hsotg *hsotg, bool select_phy);
void dwc2_force_mode(struct dwc2_hsotg *hsotg, bool host);
void dwc2_force_dr_mode(struct dwc2_hsotg *hsotg);
bool dwc2_is_controller_alive(struct dwc2_hsotg *hsotg);
int dwc2_check_core_version(struct dwc2_hsotg *hsotg);
void dwc2_read_packet(struct dwc2_hsotg *hsotg, u8 *dest, u16 bytes);
void dwc2_flush_tx_fifo(struct dwc2_hsotg *hsotg, const int num);
void dwc2_flush_rx_fifo(struct dwc2_hsotg *hsotg);
void dwc2_enable_global_interrupts(struct dwc2_hsotg *hcd);
void dwc2_disable_global_interrupts(struct dwc2_hsotg *hcd);
void dwc2_hib_restore_common(struct dwc2_hsotg *hsotg, int rem_wakeup,
			     int is_host);
int dwc2_backup_global_registers(struct dwc2_hsotg *hsotg);
int dwc2_restore_global_registers(struct dwc2_hsotg *hsotg);
void dwc2_enable_acg(struct dwc2_hsotg *hsotg);
irqreturn_t dwc2_handle_common_intr(int irq, void *dev);
extern const struct of_device_id dwc2_of_match_table[];
extern const struct acpi_device_id dwc2_acpi_match[];
extern const struct pci_device_id dwc2_pci_ids[];
int dwc2_lowlevel_hw_enable(struct dwc2_hsotg *hsotg);
int dwc2_lowlevel_hw_disable(struct dwc2_hsotg *hsotg);
int dwc2_hsotg_wait_bit_set(struct dwc2_hsotg *hs_otg, u32 reg, u32 bit,
			    u32 timeout);
int dwc2_hsotg_wait_bit_clear(struct dwc2_hsotg *hs_otg, u32 reg, u32 bit,
			      u32 timeout);
int dwc2_get_hwparams(struct dwc2_hsotg *hsotg);
int dwc2_init_params(struct dwc2_hsotg *hsotg);
unsigned int dwc2_op_mode(struct dwc2_hsotg *hsotg);
bool dwc2_hw_is_otg(struct dwc2_hsotg *hsotg);
bool dwc2_hw_is_host(struct dwc2_hsotg *hsotg);
bool dwc2_hw_is_device(struct dwc2_hsotg *hsotg);
static inline int dwc2_is_host_mode(struct dwc2_hsotg *hsotg)
{
	return (dwc2_readl(hsotg, GINTSTS) & GINTSTS_CURMODE_HOST) != 0;
}
static inline int dwc2_is_device_mode(struct dwc2_hsotg *hsotg)
{
	return (dwc2_readl(hsotg, GINTSTS) & GINTSTS_CURMODE_HOST) == 0;
}
int dwc2_drd_init(struct dwc2_hsotg *hsotg);
void dwc2_drd_suspend(struct dwc2_hsotg *hsotg);
void dwc2_drd_resume(struct dwc2_hsotg *hsotg);
void dwc2_drd_exit(struct dwc2_hsotg *hsotg);
void dwc2_dump_dev_registers(struct dwc2_hsotg *hsotg);
void dwc2_dump_host_registers(struct dwc2_hsotg *hsotg);
void dwc2_dump_global_registers(struct dwc2_hsotg *hsotg);
#if IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL) || \
	IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
int dwc2_hsotg_remove(struct dwc2_hsotg *hsotg);
int dwc2_hsotg_suspend(struct dwc2_hsotg *dwc2);
int dwc2_hsotg_resume(struct dwc2_hsotg *dwc2);
int dwc2_gadget_init(struct dwc2_hsotg *hsotg);
void dwc2_hsotg_core_init_disconnected(struct dwc2_hsotg *dwc2,
				       bool reset);
void dwc2_hsotg_core_disconnect(struct dwc2_hsotg *hsotg);
void dwc2_hsotg_core_connect(struct dwc2_hsotg *hsotg);
void dwc2_hsotg_disconnect(struct dwc2_hsotg *dwc2);
int dwc2_hsotg_set_test_mode(struct dwc2_hsotg *hsotg, int testmode);
#define dwc2_is_device_connected(hsotg) (hsotg->connected)
#define dwc2_is_device_enabled(hsotg) (hsotg->enabled)
int dwc2_backup_device_registers(struct dwc2_hsotg *hsotg);
int dwc2_restore_device_registers(struct dwc2_hsotg *hsotg, int remote_wakeup);
int dwc2_gadget_enter_hibernation(struct dwc2_hsotg *hsotg);
int dwc2_gadget_exit_hibernation(struct dwc2_hsotg *hsotg,
				 int rem_wakeup, int reset);
int dwc2_gadget_enter_partial_power_down(struct dwc2_hsotg *hsotg);
int dwc2_gadget_exit_partial_power_down(struct dwc2_hsotg *hsotg,
					bool restore);
void dwc2_gadget_enter_clock_gating(struct dwc2_hsotg *hsotg);
void dwc2_gadget_exit_clock_gating(struct dwc2_hsotg *hsotg,
				   int rem_wakeup);
int dwc2_hsotg_tx_fifo_count(struct dwc2_hsotg *hsotg);
int dwc2_hsotg_tx_fifo_total_depth(struct dwc2_hsotg *hsotg);
int dwc2_hsotg_tx_fifo_average_depth(struct dwc2_hsotg *hsotg);
void dwc2_gadget_init_lpm(struct dwc2_hsotg *hsotg);
void dwc2_gadget_program_ref_clk(struct dwc2_hsotg *hsotg);
static inline void dwc2_clear_fifo_map(struct dwc2_hsotg *hsotg)
{ hsotg->fifo_map = 0; }
#else
static inline int dwc2_hsotg_remove(struct dwc2_hsotg *dwc2)
{ return 0; }
static inline int dwc2_hsotg_suspend(struct dwc2_hsotg *dwc2)
{ return 0; }
static inline int dwc2_hsotg_resume(struct dwc2_hsotg *dwc2)
{ return 0; }
static inline int dwc2_gadget_init(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline void dwc2_hsotg_core_init_disconnected(struct dwc2_hsotg *dwc2,
						     bool reset) {}
static inline void dwc2_hsotg_core_disconnect(struct dwc2_hsotg *hsotg) {}
static inline void dwc2_hsotg_core_connect(struct dwc2_hsotg *hsotg) {}
static inline void dwc2_hsotg_disconnect(struct dwc2_hsotg *dwc2) {}
static inline int dwc2_hsotg_set_test_mode(struct dwc2_hsotg *hsotg,
					   int testmode)
{ return 0; }
#define dwc2_is_device_connected(hsotg) (0)
#define dwc2_is_device_enabled(hsotg) (0)
static inline int dwc2_backup_device_registers(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline int dwc2_restore_device_registers(struct dwc2_hsotg *hsotg,
						int remote_wakeup)
{ return 0; }
static inline int dwc2_gadget_enter_hibernation(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline int dwc2_gadget_exit_hibernation(struct dwc2_hsotg *hsotg,
					       int rem_wakeup, int reset)
{ return 0; }
static inline int dwc2_gadget_enter_partial_power_down(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline int dwc2_gadget_exit_partial_power_down(struct dwc2_hsotg *hsotg,
						      bool restore)
{ return 0; }
static inline void dwc2_gadget_enter_clock_gating(struct dwc2_hsotg *hsotg) {}
static inline void dwc2_gadget_exit_clock_gating(struct dwc2_hsotg *hsotg,
						 int rem_wakeup) {}
static inline int dwc2_hsotg_tx_fifo_count(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline int dwc2_hsotg_tx_fifo_total_depth(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline int dwc2_hsotg_tx_fifo_average_depth(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline void dwc2_gadget_init_lpm(struct dwc2_hsotg *hsotg) {}
static inline void dwc2_gadget_program_ref_clk(struct dwc2_hsotg *hsotg) {}
static inline void dwc2_clear_fifo_map(struct dwc2_hsotg *hsotg) {}
#endif
#if IS_ENABLED(CONFIG_USB_DWC2_HOST) || IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
int dwc2_hcd_get_frame_number(struct dwc2_hsotg *hsotg);
int dwc2_hcd_get_future_frame_number(struct dwc2_hsotg *hsotg, int us);
void dwc2_hcd_connect(struct dwc2_hsotg *hsotg);
void dwc2_hcd_disconnect(struct dwc2_hsotg *hsotg, bool force);
void dwc2_hcd_start(struct dwc2_hsotg *hsotg);
int dwc2_core_init(struct dwc2_hsotg *hsotg, bool initial_setup);
int dwc2_port_suspend(struct dwc2_hsotg *hsotg, u16 windex);
int dwc2_port_resume(struct dwc2_hsotg *hsotg);
int dwc2_backup_host_registers(struct dwc2_hsotg *hsotg);
int dwc2_restore_host_registers(struct dwc2_hsotg *hsotg);
int dwc2_host_enter_hibernation(struct dwc2_hsotg *hsotg);
int dwc2_host_exit_hibernation(struct dwc2_hsotg *hsotg,
			       int rem_wakeup, int reset);
int dwc2_host_enter_partial_power_down(struct dwc2_hsotg *hsotg);
int dwc2_host_exit_partial_power_down(struct dwc2_hsotg *hsotg,
				      int rem_wakeup, bool restore);
void dwc2_host_enter_clock_gating(struct dwc2_hsotg *hsotg);
void dwc2_host_exit_clock_gating(struct dwc2_hsotg *hsotg, int rem_wakeup);
bool dwc2_host_can_poweroff_phy(struct dwc2_hsotg *dwc2);
static inline void dwc2_host_schedule_phy_reset(struct dwc2_hsotg *hsotg)
{ schedule_work(&hsotg->phy_reset_work); }
#else
static inline int dwc2_hcd_get_frame_number(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline int dwc2_hcd_get_future_frame_number(struct dwc2_hsotg *hsotg,
						   int us)
{ return 0; }
static inline void dwc2_hcd_connect(struct dwc2_hsotg *hsotg) {}
static inline void dwc2_hcd_disconnect(struct dwc2_hsotg *hsotg, bool force) {}
static inline void dwc2_hcd_start(struct dwc2_hsotg *hsotg) {}
static inline void dwc2_hcd_remove(struct dwc2_hsotg *hsotg) {}
static inline int dwc2_core_init(struct dwc2_hsotg *hsotg, bool initial_setup)
{ return 0; }
static inline int dwc2_port_suspend(struct dwc2_hsotg *hsotg, u16 windex)
{ return 0; }
static inline int dwc2_port_resume(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline int dwc2_hcd_init(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline int dwc2_backup_host_registers(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline int dwc2_restore_host_registers(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline int dwc2_host_enter_hibernation(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline int dwc2_host_exit_hibernation(struct dwc2_hsotg *hsotg,
					     int rem_wakeup, int reset)
{ return 0; }
static inline int dwc2_host_enter_partial_power_down(struct dwc2_hsotg *hsotg)
{ return 0; }
static inline int dwc2_host_exit_partial_power_down(struct dwc2_hsotg *hsotg,
						    int rem_wakeup, bool restore)
{ return 0; }
static inline void dwc2_host_enter_clock_gating(struct dwc2_hsotg *hsotg) {}
static inline void dwc2_host_exit_clock_gating(struct dwc2_hsotg *hsotg,
					       int rem_wakeup) {}
static inline bool dwc2_host_can_poweroff_phy(struct dwc2_hsotg *dwc2)
{ return false; }
static inline void dwc2_host_schedule_phy_reset(struct dwc2_hsotg *hsotg) {}
#endif
#endif  
