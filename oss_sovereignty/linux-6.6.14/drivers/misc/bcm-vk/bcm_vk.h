 
 

#ifndef BCM_VK_H
#define BCM_VK_H

#include <linux/atomic.h>
#include <linux/firmware.h>
#include <linux/irq.h>
#include <linux/kref.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/sched/signal.h>
#include <linux/tty.h>
#include <linux/uaccess.h>
#include <uapi/linux/misc/bcm_vk.h>

#include "bcm_vk_msg.h"

#define DRV_MODULE_NAME		"bcm-vk"

 
 

 
#define BAR_CODEPUSH_SBL		0x400
 
#define CODEPUSH_BOOT1_ENTRY		0x00400000
#define CODEPUSH_MASK		        0xfffff000
#define CODEPUSH_BOOTSTART		BIT(0)

 
#define BAR_BOOT_STATUS			0x404

#define SRAM_OPEN			BIT(16)
#define DDR_OPEN			BIT(17)

 
#define FW_LOADER_ACK_SEND_MORE_DATA	BIT(18)
#define FW_LOADER_ACK_IN_PROGRESS	BIT(19)
#define FW_LOADER_ACK_RCVD_ALL_DATA	BIT(20)

 
#define BOOT_STDALONE_RUNNING		BIT(21)

 
#define BOOT_STATE_MASK			(0xffffffff & \
					 ~(FW_LOADER_ACK_SEND_MORE_DATA | \
					   FW_LOADER_ACK_IN_PROGRESS | \
					   BOOT_STDALONE_RUNNING))

#define BOOT_ERR_SHIFT			4
#define BOOT_ERR_MASK			(0xf << BOOT_ERR_SHIFT)
#define BOOT_PROG_MASK			0xf

#define BROM_STATUS_NOT_RUN		0x2
#define BROM_NOT_RUN			(SRAM_OPEN | BROM_STATUS_NOT_RUN)
#define BROM_STATUS_COMPLETE		0x6
#define BROM_RUNNING			(SRAM_OPEN | BROM_STATUS_COMPLETE)
#define BOOT1_STATUS_COMPLETE		0x6
#define BOOT1_RUNNING			(DDR_OPEN | BOOT1_STATUS_COMPLETE)
#define BOOT2_STATUS_COMPLETE		0x6
#define BOOT2_RUNNING			(FW_LOADER_ACK_RCVD_ALL_DATA | \
					 BOOT2_STATUS_COMPLETE)

 
#define BAR_CODEPUSH_SBI		0x408
 
#define CODEPUSH_BOOT2_ENTRY		0x60000000

#define BAR_CARD_STATUS			0x410
 
#define CARD_STATUS_TTYVK0_READY	BIT(0)
#define CARD_STATUS_TTYVK1_READY	BIT(1)

#define BAR_BOOT1_STDALONE_PROGRESS	0x420
#define BOOT1_STDALONE_SUCCESS		(BIT(13) | BIT(14))
#define BOOT1_STDALONE_PROGRESS_MASK	BOOT1_STDALONE_SUCCESS

#define BAR_METADATA_VERSION		0x440
#define BAR_OS_UPTIME			0x444
#define BAR_CHIP_ID			0x448
#define MAJOR_SOC_REV(_chip_id)		(((_chip_id) >> 20) & 0xf)

#define BAR_CARD_TEMPERATURE		0x45c
 
#define BCM_VK_TEMP_FIELD_MASK		0xff
#define BCM_VK_CPU_TEMP_SHIFT		0
#define BCM_VK_DDR0_TEMP_SHIFT		8
#define BCM_VK_DDR1_TEMP_SHIFT		16

#define BAR_CARD_VOLTAGE		0x460
 
#define BCM_VK_VOLT_RAIL_MASK		0xffff
#define BCM_VK_3P3_VOLT_REG_SHIFT	16

#define BAR_CARD_ERR_LOG		0x464
 
#define ERR_LOG_UECC			BIT(0)
#define ERR_LOG_SSIM_BUSY		BIT(1)
#define ERR_LOG_AFBC_BUSY		BIT(2)
#define ERR_LOG_HIGH_TEMP_ERR		BIT(3)
#define ERR_LOG_WDOG_TIMEOUT		BIT(4)
#define ERR_LOG_SYS_FAULT		BIT(5)
#define ERR_LOG_RAMDUMP			BIT(6)
#define ERR_LOG_COP_WDOG_TIMEOUT	BIT(7)
 
#define ERR_LOG_MEM_ALLOC_FAIL		BIT(8)
#define ERR_LOG_LOW_TEMP_WARN		BIT(9)
#define ERR_LOG_ECC			BIT(10)
#define ERR_LOG_IPC_DWN			BIT(11)

 
#define ERR_LOG_HOST_INTF_V_FAIL	BIT(13)
#define ERR_LOG_HOST_HB_FAIL		BIT(14)
#define ERR_LOG_HOST_PCIE_DWN		BIT(15)

#define BAR_CARD_ERR_MEM		0x468
 
#define BCM_VK_MEM_ERR_FIELD_MASK	0xff
#define BCM_VK_ECC_MEM_ERR_SHIFT	0
#define BCM_VK_UECC_MEM_ERR_SHIFT	8
 
#define BCM_VK_ECC_THRESHOLD		10
#define BCM_VK_UECC_THRESHOLD		1

#define BAR_CARD_PWR_AND_THRE		0x46c
 
#define BCM_VK_PWR_AND_THRE_FIELD_MASK	0xff
#define BCM_VK_LOW_TEMP_THRE_SHIFT	0
#define BCM_VK_HIGH_TEMP_THRE_SHIFT	8
#define BCM_VK_PWR_STATE_SHIFT		16

#define BAR_CARD_STATIC_INFO		0x470

#define BAR_INTF_VER			0x47c
#define BAR_INTF_VER_MAJOR_SHIFT	16
#define BAR_INTF_VER_MASK		0xffff
 
#define SEMANTIC_MAJOR			1
#define SEMANTIC_MINOR			0

 
#define VK_BAR0_REGSEG_DB_BASE		0x484
#define VK_BAR0_REGSEG_DB_REG_GAP	8  

 
#define VK_BAR0_RESET_DB_NUM		3
#define VK_BAR0_RESET_DB_SOFT		0xffffffff
#define VK_BAR0_RESET_DB_HARD		0xfffffffd
#define VK_BAR0_RESET_RAMPDUMP		0xa0000000

#define VK_BAR0_Q_DB_BASE(q_num)	(VK_BAR0_REGSEG_DB_BASE + \
					 ((q_num) * VK_BAR0_REGSEG_DB_REG_GAP))
#define VK_BAR0_RESET_DB_BASE		(VK_BAR0_REGSEG_DB_BASE + \
					 (VK_BAR0_RESET_DB_NUM * VK_BAR0_REGSEG_DB_REG_GAP))

#define BAR_BOOTSRC_SELECT		0xc78
 
#define BOOTSRC_SOFT_ENABLE		BIT(14)

 
#define BAR_FIRMWARE_TAG_SIZE		50
#define FIRMWARE_STATUS_PRE_INIT_DONE	0x1f

 
#define VK_MSG_ID_BITMAP_SIZE		4096
#define VK_MSG_ID_BITMAP_MASK		(VK_MSG_ID_BITMAP_SIZE - 1)
#define VK_MSG_ID_OVERFLOW		0xffff

 

 

 
#define VK_BAR1_MSGQ_DEF_RDY		0x60c0
 
#define VK_BAR1_MSGQ_RDY_MARKER		0xbeefcafe
 
#define VK_BAR1_DIAG_RDY_MARKER		0xdeadcafe
 
#define VK_BAR1_MSGQ_NR			0x60c4
 
#define VK_BAR1_MSGQ_CTRL_OFF		0x60c8

 
#define VK_BAR1_UCODE_VER_TAG		0x6170
#define VK_BAR1_BOOT1_VER_TAG		0x61b0
#define VK_BAR1_VER_TAG_SIZE		64

 
#define VK_BAR1_DMA_BUF_OFF_HI		0x61e0
#define VK_BAR1_DMA_BUF_OFF_LO		(VK_BAR1_DMA_BUF_OFF_HI + 4)
#define VK_BAR1_DMA_BUF_SZ		(VK_BAR1_DMA_BUF_OFF_HI + 8)

 
#define VK_BAR1_SCRATCH_OFF_HI		0x61f0
#define VK_BAR1_SCRATCH_OFF_LO		(VK_BAR1_SCRATCH_OFF_HI + 4)
#define VK_BAR1_SCRATCH_SZ_ADDR		(VK_BAR1_SCRATCH_OFF_HI + 8)
#define VK_BAR1_SCRATCH_DEF_NR_PAGES	32

 
#define VK_BAR1_DAUTH_BASE_ADDR		0x6200
#define VK_BAR1_DAUTH_STORE_SIZE	0x48
#define VK_BAR1_DAUTH_VALID_SIZE	0x8
#define VK_BAR1_DAUTH_MAX		4
#define VK_BAR1_DAUTH_STORE_ADDR(x) \
		(VK_BAR1_DAUTH_BASE_ADDR + \
		 (x) * (VK_BAR1_DAUTH_STORE_SIZE + VK_BAR1_DAUTH_VALID_SIZE))
#define VK_BAR1_DAUTH_VALID_ADDR(x) \
		(VK_BAR1_DAUTH_STORE_ADDR(x) + VK_BAR1_DAUTH_STORE_SIZE)

 
#define VK_BAR1_SOTP_REVID_BASE_ADDR	0x6340
#define VK_BAR1_SOTP_REVID_SIZE		0x10
#define VK_BAR1_SOTP_REVID_MAX		2
#define VK_BAR1_SOTP_REVID_ADDR(x) \
		(VK_BAR1_SOTP_REVID_BASE_ADDR + (x) * VK_BAR1_SOTP_REVID_SIZE)

 
#define MAX_BAR	3

 
#define BCM_VK_DEF_IB_SGL_BLK_LEN	 16
#define BCM_VK_IB_SGL_BLK_MAX		 24

enum pci_barno {
	BAR_0 = 0,
	BAR_1,
	BAR_2
};

#ifdef CONFIG_BCM_VK_TTY
#define BCM_VK_NUM_TTY 2
#else
#define BCM_VK_NUM_TTY 0
#endif

struct bcm_vk_tty {
	struct tty_port port;
	u32 to_offset;	 
	u32 to_size;	 
	u32 wr;		 
	u32 from_offset;	 
	u32 from_size;	 
	u32 rd;		 
	pid_t pid;
	bool irq_enabled;
	bool is_opened;		 
};

 
#define MAX_OPP 3
#define MAX_CARD_INFO_TAG_SIZE 64

struct bcm_vk_card_info {
	u32 version;
	char os_tag[MAX_CARD_INFO_TAG_SIZE];
	char cmpt_tag[MAX_CARD_INFO_TAG_SIZE];
	u32 cpu_freq_mhz;
	u32 cpu_scale[MAX_OPP];
	u32 ddr_freq_mhz;
	u32 ddr_size_MB;
	u32 video_core_freq_mhz;
};

 
struct bcm_vk_dauth_key {
	char store[VK_BAR1_DAUTH_STORE_SIZE];
	char valid[VK_BAR1_DAUTH_VALID_SIZE];
};

struct bcm_vk_dauth_info {
	struct bcm_vk_dauth_key keys[VK_BAR1_DAUTH_MAX];
};

 
struct bcm_vk_peer_log {
	u32 rd_idx;
	u32 wr_idx;
	u32 buf_size;
	u32 mask;
	char data[];
};

 
#define BCM_VK_PEER_LOG_BUF_MAX SZ_16K
 
#define BCM_VK_PEER_LOG_LINE_MAX  256

 
#define BCM_VK_PROC_TYPE_TAG_LEN 8
struct bcm_vk_proc_mon_entry_t {
	char tag[BCM_VK_PROC_TYPE_TAG_LEN];
	u32 used;
	u32 max;  
};

 
#define BCM_VK_PROC_MON_MAX 8  
struct bcm_vk_proc_mon_info {
	u32 num;  
	u32 entry_size;  
	struct bcm_vk_proc_mon_entry_t entries[BCM_VK_PROC_MON_MAX];
};

struct bcm_vk_hb_ctrl {
	struct delayed_work work;
	u32 last_uptime;
	u32 lost_cnt;
};

struct bcm_vk_alert {
	u16 flags;
	u16 notfs;
};

 
struct bcm_vk_alert_cnts {
	u16 ecc;
	u16 uecc;
};

struct bcm_vk {
	struct pci_dev *pdev;
	void __iomem *bar[MAX_BAR];
	int num_irqs;

	struct bcm_vk_card_info card_info;
	struct bcm_vk_proc_mon_info proc_mon_info;
	struct bcm_vk_dauth_info dauth_info;

	 
	struct mutex mutex;
	struct miscdevice miscdev;
	int devid;  

#ifdef CONFIG_BCM_VK_TTY
	struct tty_driver *tty_drv;
	struct timer_list serial_timer;
	struct bcm_vk_tty tty[BCM_VK_NUM_TTY];
	struct workqueue_struct *tty_wq_thread;
	struct work_struct tty_wq_work;
#endif

	 
	struct kref kref;

	spinlock_t msg_id_lock;  
	u16 msg_id;
	DECLARE_BITMAP(bmap, VK_MSG_ID_BITMAP_SIZE);
	spinlock_t ctx_lock;  
	struct bcm_vk_ctx ctx[VK_CMPT_CTX_MAX];
	struct bcm_vk_ht_entry pid_ht[VK_PID_HT_SZ];
	pid_t reset_pid;  

	atomic_t msgq_inited;  
	struct bcm_vk_msg_chan to_v_msg_chan;
	struct bcm_vk_msg_chan to_h_msg_chan;

	struct workqueue_struct *wq_thread;
	struct work_struct wq_work;  
	unsigned long wq_offload[1];  
	void *tdma_vaddr;  
	dma_addr_t tdma_addr;  

	struct notifier_block panic_nb;
	u32 ib_sgl_size;  

	 
	struct bcm_vk_hb_ctrl hb_ctrl;
	 
	spinlock_t host_alert_lock;  
	struct bcm_vk_alert host_alert;
	struct bcm_vk_alert peer_alert;  
	struct bcm_vk_alert_cnts alert_cnts;

	 
	u32 peerlog_off;
	struct bcm_vk_peer_log peerlog_info;  
	 
	u32 proc_mon_off;
};

 
enum bcm_vk_wq_offload_flags {
	BCM_VK_WQ_DWNLD_PEND = 0,
	BCM_VK_WQ_DWNLD_AUTO = 1,
	BCM_VK_WQ_NOTF_PEND  = 2,
};

 
#define BCM_VK_EXTRACT_FIELD(_field, _reg, _mask, _shift) \
		(_field = (((_reg) >> (_shift)) & (_mask)))

struct bcm_vk_entry {
	const u32 mask;
	const u32 exp_val;
	const char *str;
};

 
#define BCM_VK_PEER_ERR_NUM 12
extern struct bcm_vk_entry const bcm_vk_peer_err[BCM_VK_PEER_ERR_NUM];
 
#define BCM_VK_HOST_ERR_NUM 3
extern struct bcm_vk_entry const bcm_vk_host_err[BCM_VK_HOST_ERR_NUM];

 
#define BCM_VK_INTF_IS_DOWN(val) ((val) == 0xffffffff)

static inline u32 vkread32(struct bcm_vk *vk, enum pci_barno bar, u64 offset)
{
	return readl(vk->bar[bar] + offset);
}

static inline void vkwrite32(struct bcm_vk *vk,
			     u32 value,
			     enum pci_barno bar,
			     u64 offset)
{
	writel(value, vk->bar[bar] + offset);
}

static inline u8 vkread8(struct bcm_vk *vk, enum pci_barno bar, u64 offset)
{
	return readb(vk->bar[bar] + offset);
}

static inline void vkwrite8(struct bcm_vk *vk,
			    u8 value,
			    enum pci_barno bar,
			    u64 offset)
{
	writeb(value, vk->bar[bar] + offset);
}

static inline bool bcm_vk_msgq_marker_valid(struct bcm_vk *vk)
{
	u32 rdy_marker = 0;
	u32 fw_status;

	fw_status = vkread32(vk, BAR_0, VK_BAR_FWSTS);

	if ((fw_status & VK_FWSTS_READY) == VK_FWSTS_READY)
		rdy_marker = vkread32(vk, BAR_1, VK_BAR1_MSGQ_DEF_RDY);

	return (rdy_marker == VK_BAR1_MSGQ_RDY_MARKER);
}

int bcm_vk_open(struct inode *inode, struct file *p_file);
ssize_t bcm_vk_read(struct file *p_file, char __user *buf, size_t count,
		    loff_t *f_pos);
ssize_t bcm_vk_write(struct file *p_file, const char __user *buf,
		     size_t count, loff_t *f_pos);
__poll_t bcm_vk_poll(struct file *p_file, struct poll_table_struct *wait);
int bcm_vk_release(struct inode *inode, struct file *p_file);
void bcm_vk_release_data(struct kref *kref);
irqreturn_t bcm_vk_msgq_irqhandler(int irq, void *dev_id);
irqreturn_t bcm_vk_notf_irqhandler(int irq, void *dev_id);
irqreturn_t bcm_vk_tty_irqhandler(int irq, void *dev_id);
int bcm_vk_msg_init(struct bcm_vk *vk);
void bcm_vk_msg_remove(struct bcm_vk *vk);
void bcm_vk_drain_msg_on_reset(struct bcm_vk *vk);
int bcm_vk_sync_msgq(struct bcm_vk *vk, bool force_sync);
void bcm_vk_blk_drv_access(struct bcm_vk *vk);
s32 bcm_to_h_msg_dequeue(struct bcm_vk *vk);
int bcm_vk_send_shutdown_msg(struct bcm_vk *vk, u32 shut_type,
			     const pid_t pid, const u32 q_num);
void bcm_to_v_q_doorbell(struct bcm_vk *vk, u32 q_num, u32 db_val);
int bcm_vk_auto_load_all_images(struct bcm_vk *vk);
void bcm_vk_hb_init(struct bcm_vk *vk);
void bcm_vk_hb_deinit(struct bcm_vk *vk);
void bcm_vk_handle_notf(struct bcm_vk *vk);
bool bcm_vk_drv_access_ok(struct bcm_vk *vk);
void bcm_vk_set_host_alert(struct bcm_vk *vk, u32 bit_mask);

#ifdef CONFIG_BCM_VK_TTY
int bcm_vk_tty_init(struct bcm_vk *vk, char *name);
void bcm_vk_tty_exit(struct bcm_vk *vk);
void bcm_vk_tty_terminate_tty_user(struct bcm_vk *vk);
void bcm_vk_tty_wq_exit(struct bcm_vk *vk);

static inline void bcm_vk_tty_set_irq_enabled(struct bcm_vk *vk, int index)
{
	vk->tty[index].irq_enabled = true;
}
#else
static inline int bcm_vk_tty_init(struct bcm_vk *vk, char *name)
{
	return 0;
}

static inline void bcm_vk_tty_exit(struct bcm_vk *vk)
{
}

static inline void bcm_vk_tty_terminate_tty_user(struct bcm_vk *vk)
{
}

static inline void bcm_vk_tty_wq_exit(struct bcm_vk *vk)
{
}

static inline void bcm_vk_tty_set_irq_enabled(struct bcm_vk *vk, int index)
{
}
#endif  

#endif
