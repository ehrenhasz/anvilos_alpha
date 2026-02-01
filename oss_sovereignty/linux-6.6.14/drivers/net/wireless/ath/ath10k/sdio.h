 
 

#ifndef _SDIO_H_
#define _SDIO_H_

#define ATH10K_HIF_MBOX_BLOCK_SIZE              256

#define ATH10K_SDIO_MAX_BUFFER_SIZE             4096  

 
#define ATH10K_HIF_MBOX_BASE_ADDR               0x1000
#define ATH10K_HIF_MBOX_WIDTH                   0x800

#define ATH10K_HIF_MBOX_TOT_WIDTH \
	(ATH10K_HIF_MBOX_NUM_MAX * ATH10K_HIF_MBOX_WIDTH)

#define ATH10K_HIF_MBOX0_EXT_BASE_ADDR          0x5000
#define ATH10K_HIF_MBOX0_EXT_WIDTH              (36 * 1024)
#define ATH10K_HIF_MBOX0_EXT_WIDTH_ROME_2_0     (56 * 1024)
#define ATH10K_HIF_MBOX1_EXT_WIDTH              (36 * 1024)
#define ATH10K_HIF_MBOX_DUMMY_SPACE_SIZE        (2 * 1024)

#define ATH10K_HTC_MBOX_MAX_PAYLOAD_LENGTH \
	(ATH10K_SDIO_MAX_BUFFER_SIZE - sizeof(struct ath10k_htc_hdr))

#define ATH10K_HIF_MBOX_NUM_MAX                 4
#define ATH10K_SDIO_BUS_REQUEST_MAX_NUM         1024

#define ATH10K_SDIO_HIF_COMMUNICATION_TIMEOUT_HZ (100 * HZ)

 
#define ATH10K_HTC_MAILBOX                      0
#define ATH10K_HTC_MAILBOX_MASK                 BIT(ATH10K_HTC_MAILBOX)

 
#define ATH10K_HIF_GMBOX_BASE_ADDR              0x7000
#define ATH10K_HIF_GMBOX_WIDTH                  0x4000

 
#define ATH10K_SDIO_DRIVE_DTSX_MASK \
	(SDIO_DRIVE_DTSx_MASK << SDIO_DRIVE_DTSx_SHIFT)

#define ATH10K_SDIO_DRIVE_DTSX_TYPE_B           0
#define ATH10K_SDIO_DRIVE_DTSX_TYPE_A           1
#define ATH10K_SDIO_DRIVE_DTSX_TYPE_C           2
#define ATH10K_SDIO_DRIVE_DTSX_TYPE_D           3

 
#define CCCR_SDIO_IRQ_MODE_REG                  0xF0
#define CCCR_SDIO_IRQ_MODE_REG_SDIO3            0x16

#define CCCR_SDIO_DRIVER_STRENGTH_ENABLE_ADDR   0xF2

#define CCCR_SDIO_DRIVER_STRENGTH_ENABLE_A      0x02
#define CCCR_SDIO_DRIVER_STRENGTH_ENABLE_C      0x04
#define CCCR_SDIO_DRIVER_STRENGTH_ENABLE_D      0x08

#define CCCR_SDIO_ASYNC_INT_DELAY_ADDRESS       0xF0
#define CCCR_SDIO_ASYNC_INT_DELAY_MASK          0xC0

 
#define SDIO_IRQ_MODE_ASYNC_4BIT_IRQ            BIT(0)
#define SDIO_IRQ_MODE_ASYNC_4BIT_IRQ_SDIO3      BIT(1)

#define ATH10K_SDIO_TARGET_DEBUG_INTR_MASK      0x01

 
#define ATH10K_SDIO_MAX_RX_MSGS \
	(HTC_HOST_MAX_MSG_PER_RX_BUNDLE * 2)

#define ATH10K_FIFO_TIMEOUT_AND_CHIP_CONTROL   0x00000868u
#define ATH10K_FIFO_TIMEOUT_AND_CHIP_CONTROL_DISABLE_SLEEP_OFF 0xFFFEFFFF
#define ATH10K_FIFO_TIMEOUT_AND_CHIP_CONTROL_DISABLE_SLEEP_ON 0x10000

enum sdio_mbox_state {
	SDIO_MBOX_UNKNOWN_STATE = 0,
	SDIO_MBOX_REQUEST_TO_SLEEP_STATE = 1,
	SDIO_MBOX_SLEEP_STATE = 2,
	SDIO_MBOX_AWAKE_STATE = 3,
};

#define ATH10K_CIS_READ_WAIT_4_RTC_CYCLE_IN_US	125
#define ATH10K_CIS_RTC_STATE_ADDR		0x1138
#define ATH10K_CIS_RTC_STATE_ON			0x01
#define ATH10K_CIS_XTAL_SETTLE_DURATION_IN_US	1500
#define ATH10K_CIS_READ_RETRY			10
#define ATH10K_MIN_SLEEP_INACTIVITY_TIME_MS	50

 
struct ath10k_sdio_bus_request {
	struct list_head list;

	 
	u32 address;

	struct sk_buff *skb;
	enum ath10k_htc_ep_id eid;
	int status;
	 
	bool htc_msg;
	 
	struct completion *comp;
};

struct ath10k_sdio_rx_data {
	struct sk_buff *skb;
	size_t alloc_len;
	size_t act_len;
	enum ath10k_htc_ep_id eid;
	bool part_of_bundle;
	bool last_in_bundle;
	bool trailer_only;
};

struct ath10k_sdio_irq_proc_regs {
	u8 host_int_status;
	u8 cpu_int_status;
	u8 error_int_status;
	u8 counter_int_status;
	u8 mbox_frame;
	u8 rx_lookahead_valid;
	u8 host_int_status2;
	u8 gmbox_rx_avail;
	__le32 rx_lookahead[2 * ATH10K_HIF_MBOX_NUM_MAX];
	__le32 int_status_enable;
};

struct ath10k_sdio_irq_enable_regs {
	u8 int_status_en;
	u8 cpu_int_status_en;
	u8 err_int_status_en;
	u8 cntr_int_status_en;
};

struct ath10k_sdio_irq_data {
	 
	struct mutex mtx;
	struct ath10k_sdio_irq_proc_regs *irq_proc_reg;
	struct ath10k_sdio_irq_enable_regs *irq_en_reg;
};

struct ath10k_mbox_ext_info {
	u32 htc_ext_addr;
	u32 htc_ext_sz;
};

struct ath10k_mbox_info {
	u32 htc_addr;
	struct ath10k_mbox_ext_info ext_info[2];
	u32 block_size;
	u32 block_mask;
	u32 gmbox_addr;
	u32 gmbox_sz;
};

struct ath10k_sdio {
	struct sdio_func *func;

	struct ath10k_mbox_info mbox_info;
	bool swap_mbox;
	u32 mbox_addr[ATH10K_HTC_EP_COUNT];
	u32 mbox_size[ATH10K_HTC_EP_COUNT];

	 
	struct ath10k_sdio_bus_request bus_req[ATH10K_SDIO_BUS_REQUEST_MAX_NUM];
	 
	struct list_head bus_req_freeq;

	struct sk_buff_head rx_head;

	 
	spinlock_t lock;

	struct ath10k_sdio_rx_data rx_pkts[ATH10K_SDIO_MAX_RX_MSGS];
	size_t n_rx_pkts;

	struct ath10k *ar;
	struct ath10k_sdio_irq_data irq_data;

	 
	u8 *vsg_buffer;

	 
	u8 *bmi_buf;

	bool is_disabled;

	struct workqueue_struct *workqueue;
	struct work_struct wr_async_work;
	struct list_head wr_asyncq;
	 
	spinlock_t wr_async_lock;

	struct work_struct async_work_rx;
	struct timer_list sleep_timer;
	enum sdio_mbox_state mbox_state;
};

static inline struct ath10k_sdio *ath10k_sdio_priv(struct ath10k *ar)
{
	return (struct ath10k_sdio *)ar->drv_priv;
}

#endif
