#ifndef _VAS_H
#define _VAS_H
#include <linux/atomic.h>
#include <linux/idr.h>
#include <asm/vas.h>
#include <linux/io.h>
#include <linux/dcache.h>
#include <linux/mutex.h>
#include <linux/stringify.h>
#define VAS_WINDOWS_PER_CHIP		(64 << 10)
#define VAS_HVWC_SIZE			512
#define VAS_UWC_SIZE			PAGE_SIZE
#define VAS_TX_WCREDS_MAX		((4 << 10) - 1)
#define VAS_WCREDS_DEFAULT		(1 << 10)
#define VAS_LPID_OFFSET			0x010
#define VAS_LPID			PPC_BITMASK(0, 11)
#define VAS_PID_OFFSET			0x018
#define VAS_PID_ID			PPC_BITMASK(0, 19)
#define VAS_XLATE_MSR_OFFSET		0x020
#define VAS_XLATE_MSR_DR		PPC_BIT(0)
#define VAS_XLATE_MSR_TA		PPC_BIT(1)
#define VAS_XLATE_MSR_PR		PPC_BIT(2)
#define VAS_XLATE_MSR_US		PPC_BIT(3)
#define VAS_XLATE_MSR_HV		PPC_BIT(4)
#define VAS_XLATE_MSR_SF		PPC_BIT(5)
#define VAS_XLATE_LPCR_OFFSET		0x028
#define VAS_XLATE_LPCR_PAGE_SIZE	PPC_BITMASK(0, 2)
#define VAS_XLATE_LPCR_ISL		PPC_BIT(3)
#define VAS_XLATE_LPCR_TC		PPC_BIT(4)
#define VAS_XLATE_LPCR_SC		PPC_BIT(5)
#define VAS_XLATE_CTL_OFFSET		0x030
#define VAS_XLATE_MODE			PPC_BITMASK(0, 1)
#define VAS_AMR_OFFSET			0x040
#define VAS_AMR				PPC_BITMASK(0, 63)
#define VAS_SEIDR_OFFSET		0x048
#define VAS_SEIDR			PPC_BITMASK(0, 63)
#define VAS_FAULT_TX_WIN_OFFSET		0x050
#define VAS_FAULT_TX_WIN		PPC_BITMASK(48, 63)
#define VAS_OSU_INTR_SRC_RA_OFFSET	0x060
#define VAS_OSU_INTR_SRC_RA		PPC_BITMASK(8, 63)
#define VAS_HV_INTR_SRC_RA_OFFSET	0x070
#define VAS_HV_INTR_SRC_RA		PPC_BITMASK(8, 63)
#define VAS_PSWID_OFFSET		0x078
#define VAS_PSWID_EA_HANDLE		PPC_BITMASK(0, 31)
#define VAS_SPARE1_OFFSET		0x080
#define VAS_SPARE2_OFFSET		0x088
#define VAS_SPARE3_OFFSET		0x090
#define VAS_SPARE4_OFFSET		0x130
#define VAS_SPARE5_OFFSET		0x160
#define VAS_SPARE6_OFFSET		0x188
#define VAS_LFIFO_BAR_OFFSET		0x0A0
#define VAS_LFIFO_BAR			PPC_BITMASK(8, 53)
#define VAS_PAGE_MIGRATION_SELECT	PPC_BITMASK(54, 56)
#define VAS_LDATA_STAMP_CTL_OFFSET	0x0A8
#define VAS_LDATA_STAMP			PPC_BITMASK(0, 1)
#define VAS_XTRA_WRITE			PPC_BIT(2)
#define VAS_LDMA_CACHE_CTL_OFFSET	0x0B0
#define VAS_LDMA_TYPE			PPC_BITMASK(0, 1)
#define VAS_LDMA_FIFO_DISABLE		PPC_BIT(2)
#define VAS_LRFIFO_PUSH_OFFSET		0x0B8
#define VAS_LRFIFO_PUSH			PPC_BITMASK(0, 15)
#define VAS_CURR_MSG_COUNT_OFFSET	0x0C0
#define VAS_CURR_MSG_COUNT		PPC_BITMASK(0, 7)
#define VAS_LNOTIFY_AFTER_COUNT_OFFSET	0x0C8
#define VAS_LNOTIFY_AFTER_COUNT		PPC_BITMASK(0, 7)
#define VAS_LRX_WCRED_OFFSET		0x0E0
#define VAS_LRX_WCRED			PPC_BITMASK(0, 15)
#define VAS_LRX_WCRED_ADDER_OFFSET	0x190
#define VAS_LRX_WCRED_ADDER		PPC_BITMASK(0, 15)
#define VAS_TX_WCRED_OFFSET		0x0F0
#define VAS_TX_WCRED			PPC_BITMASK(4, 15)
#define VAS_TX_WCRED_ADDER_OFFSET	0x1A0
#define VAS_TX_WCRED_ADDER		PPC_BITMASK(4, 15)
#define VAS_LFIFO_SIZE_OFFSET		0x100
#define VAS_LFIFO_SIZE			PPC_BITMASK(0, 3)
#define VAS_WINCTL_OFFSET		0x108
#define VAS_WINCTL_OPEN			PPC_BIT(0)
#define VAS_WINCTL_REJ_NO_CREDIT	PPC_BIT(1)
#define VAS_WINCTL_PIN			PPC_BIT(2)
#define VAS_WINCTL_TX_WCRED_MODE	PPC_BIT(3)
#define VAS_WINCTL_RX_WCRED_MODE	PPC_BIT(4)
#define VAS_WINCTL_TX_WORD_MODE		PPC_BIT(5)
#define VAS_WINCTL_RX_WORD_MODE		PPC_BIT(6)
#define VAS_WINCTL_RSVD_TXBUF		PPC_BIT(7)
#define VAS_WINCTL_THRESH_CTL		PPC_BITMASK(8, 9)
#define VAS_WINCTL_FAULT_WIN		PPC_BIT(10)
#define VAS_WINCTL_NX_WIN		PPC_BIT(11)
#define VAS_WIN_STATUS_OFFSET		0x110
#define VAS_WIN_BUSY			PPC_BIT(1)
#define VAS_WIN_CTX_CACHING_CTL_OFFSET	0x118
#define VAS_CASTOUT_REQ			PPC_BIT(0)
#define VAS_PUSH_TO_MEM			PPC_BIT(1)
#define VAS_WIN_CACHE_STATUS		PPC_BIT(4)
#define VAS_TX_RSVD_BUF_COUNT_OFFSET	0x120
#define VAS_RXVD_BUF_COUNT		PPC_BITMASK(58, 63)
#define VAS_LRFIFO_WIN_PTR_OFFSET	0x128
#define VAS_LRX_WIN_ID			PPC_BITMASK(0, 15)
#define VAS_LNOTIFY_CTL_OFFSET		0x138
#define VAS_NOTIFY_DISABLE		PPC_BIT(0)
#define VAS_INTR_DISABLE		PPC_BIT(1)
#define VAS_NOTIFY_EARLY		PPC_BIT(2)
#define VAS_NOTIFY_OSU_INTR		PPC_BIT(3)
#define VAS_LNOTIFY_PID_OFFSET		0x140
#define VAS_LNOTIFY_PID			PPC_BITMASK(0, 19)
#define VAS_LNOTIFY_LPID_OFFSET		0x148
#define VAS_LNOTIFY_LPID		PPC_BITMASK(0, 11)
#define VAS_LNOTIFY_TID_OFFSET		0x150
#define VAS_LNOTIFY_TID			PPC_BITMASK(0, 15)
#define VAS_LNOTIFY_SCOPE_OFFSET	0x158
#define VAS_LNOTIFY_MIN_SCOPE		PPC_BITMASK(0, 1)
#define VAS_LNOTIFY_MAX_SCOPE		PPC_BITMASK(2, 3)
#define VAS_NX_UTIL_OFFSET		0x1B0
#define VAS_NX_UTIL			PPC_BITMASK(0, 63)
#define VAS_NX_UTIL_SE_OFFSET		0x1B8
#define VAS_NX_UTIL_SE			PPC_BITMASK(0, 63)
#define VAS_NX_UTIL_ADDER_OFFSET	0x180
#define VAS_NX_UTIL_ADDER		PPC_BITMASK(32, 63)
#define VREG_SFX(n, s)	__stringify(n), VAS_##n##s
#define VREG(r)		VREG_SFX(r, _OFFSET)
enum vas_notify_scope {
	VAS_SCOPE_LOCAL,
	VAS_SCOPE_GROUP,
	VAS_SCOPE_VECTORED_GROUP,
	VAS_SCOPE_UNUSED,
};
enum vas_dma_type {
	VAS_DMA_TYPE_INJECT,
	VAS_DMA_TYPE_WRITE,
};
enum vas_notify_after_count {
	VAS_NOTIFY_AFTER_256 = 0,
	VAS_NOTIFY_NONE,
	VAS_NOTIFY_AFTER_2
};
#define FIFO_INVALID_ENTRY	0xffffffff
#define CCW0_INVALID		1
struct vas_instance {
	int vas_id;
	struct ida ida;
	struct list_head node;
	struct platform_device *pdev;
	u64 hvwc_bar_start;
	u64 uwc_bar_start;
	u64 paste_base_addr;
	u64 paste_win_id_shift;
	u64 irq_port;
	int virq;
	int fault_crbs;
	int fault_fifo_size;
	int fifo_in_progress;	 
	spinlock_t fault_lock;	 
	void *fault_fifo;
	struct pnv_vas_window *fault_win;  
	struct mutex mutex;
	struct pnv_vas_window *rxwin[VAS_COP_TYPE_MAX];
	struct pnv_vas_window *windows[VAS_WINDOWS_PER_CHIP];
	char *name;
	char *dbgname;
	struct dentry *dbgdir;
};
struct pnv_vas_window {
	struct vas_window vas_win;
	struct vas_instance *vinst;
	bool tx_win;		 
	bool nx_win;		 
	bool user_win;		 
	void *hvwc_map;		 
	void *uwc_map;		 
	void *paste_kaddr;
	char *paste_addr_name;
	struct pnv_vas_window *rxwin;
	atomic_t num_txwins;
};
struct vas_winctx {
	u64 rx_fifo;
	int rx_fifo_size;
	int wcreds_max;
	int rsvd_txbuf_count;
	bool user_win;
	bool nx_win;
	bool fault_win;
	bool rsvd_txbuf_enable;
	bool pin_win;
	bool rej_no_credit;
	bool tx_wcred_mode;
	bool rx_wcred_mode;
	bool tx_word_mode;
	bool rx_word_mode;
	bool data_stamp;
	bool xtra_write;
	bool notify_disable;
	bool intr_disable;
	bool fifo_disable;
	bool notify_early;
	bool notify_os_intr_reg;
	int lpid;
	int pidr;		 
	int lnotify_lpid;
	int lnotify_pid;
	int lnotify_tid;
	u32 pswid;
	int rx_win_id;
	int fault_win_id;
	int tc_mode;
	u64 irq_port;
	enum vas_dma_type dma_type;
	enum vas_notify_scope min_scope;
	enum vas_notify_scope max_scope;
	enum vas_notify_after_count notify_after_count;
};
extern struct mutex vas_mutex;
extern struct vas_instance *find_vas_instance(int vasid);
extern void vas_init_dbgdir(void);
extern void vas_instance_init_dbgdir(struct vas_instance *vinst);
extern void vas_window_init_dbgdir(struct pnv_vas_window *win);
extern void vas_window_free_dbgdir(struct pnv_vas_window *win);
extern int vas_setup_fault_window(struct vas_instance *vinst);
extern irqreturn_t vas_fault_thread_fn(int irq, void *data);
extern irqreturn_t vas_fault_handler(int irq, void *dev_id);
extern void vas_return_credit(struct pnv_vas_window *window, bool tx);
extern struct pnv_vas_window *vas_pswid_to_window(struct vas_instance *vinst,
						uint32_t pswid);
extern void vas_win_paste_addr(struct pnv_vas_window *window, u64 *addr,
				int *len);
static inline int vas_window_pid(struct vas_window *window)
{
	return pid_vnr(window->task_ref.pid);
}
static inline void vas_log_write(struct pnv_vas_window *win, char *name,
			void *regptr, u64 val)
{
	if (val)
		pr_debug("%swin #%d: %s reg %p, val 0x%016llx\n",
				win->tx_win ? "Tx" : "Rx", win->vas_win.winid,
				name, regptr, val);
}
static inline void write_uwc_reg(struct pnv_vas_window *win, char *name,
			s32 reg, u64 val)
{
	void *regptr;
	regptr = win->uwc_map + reg;
	vas_log_write(win, name, regptr, val);
	out_be64(regptr, val);
}
static inline void write_hvwc_reg(struct pnv_vas_window *win, char *name,
			s32 reg, u64 val)
{
	void *regptr;
	regptr = win->hvwc_map + reg;
	vas_log_write(win, name, regptr, val);
	out_be64(regptr, val);
}
static inline u64 read_hvwc_reg(struct pnv_vas_window *win,
			char *name __maybe_unused, s32 reg)
{
	return in_be64(win->hvwc_map+reg);
}
static inline u32 encode_pswid(int vasid, int winid)
{
	return ((u32)winid | (vasid << (31 - 7)));
}
static inline void decode_pswid(u32 pswid, int *vasid, int *winid)
{
	if (vasid)
		*vasid = pswid >> (31 - 7) & 0xFF;
	if (winid)
		*winid = pswid & 0xFFFF;
}
#endif  
