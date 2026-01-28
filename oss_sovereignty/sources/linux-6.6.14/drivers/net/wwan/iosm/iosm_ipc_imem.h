

#ifndef IOSM_IPC_IMEM_H
#define IOSM_IPC_IMEM_H

#include <linux/skbuff.h>

#include "iosm_ipc_mmio.h"
#include "iosm_ipc_pcie.h"
#include "iosm_ipc_uevent.h"
#include "iosm_ipc_wwan.h"
#include "iosm_ipc_task_queue.h"

struct ipc_chnl_cfg;


#define IRQ_MOD_OFF 0
#define IRQ_MOD_NET 1000
#define IRQ_MOD_TRC 4000


#define IPC_PSI_TRANSFER_TIMEOUT 3000


#define IPC_MODEM_BOOT_TIMEOUT 500


#define IPC_MODEM_UNINIT_TIMEOUT_MS 30


#define IPC_PEND_DATA_TIMEOUT 500


#define IPC_REMOTE_TS_TIMEOUT_MS 10


#define IPC_TD_ALLOC_TIMER_PERIOD_MS 100


#define IPC_HOST_SLEEP_HOST 0


#define IPC_HOST_SLEEP_DEVICE 1


#define IPC_HOST_SLEEP_ENTER_SLEEP 0


#define IPC_HOST_SLEEP_EXIT_SLEEP 1

#define IMEM_IRQ_DONT_CARE (-1)

#define IPC_MEM_MAX_CHANNELS 8

#define IPC_MEM_MUX_IP_SESSION_ENTRIES 8

#define IPC_MEM_MUX_IP_CH_IF_ID 0

#define TD_UPDATE_DEFAULT_TIMEOUT_USEC 1900

#define FORCE_UPDATE_DEFAULT_TIMEOUT_USEC 500


#define IPC_HOST_SLEEP_ENTER_SLEEP_NO_PROTOCOL 2


#define IPC_MEM_INBAND_CRASH_SIG 1


#define IPC_MEM_DL_ETH_OFFSET 16

#define IPC_CB(skb) ((struct ipc_skb_cb *)((skb)->cb))
#define IOSM_CHIP_INFO_SIZE_MAX 100

#define FULLY_FUNCTIONAL 0
#define IOSM_DEVLINK_INIT 1


enum ipc_mem_pipes {
	IPC_MEM_PIPE_0 = 0,
	IPC_MEM_PIPE_1,
	IPC_MEM_PIPE_2,
	IPC_MEM_PIPE_3,
	IPC_MEM_PIPE_4,
	IPC_MEM_PIPE_5,
	IPC_MEM_PIPE_6,
	IPC_MEM_PIPE_7,
	IPC_MEM_PIPE_8,
	IPC_MEM_PIPE_9,
	IPC_MEM_PIPE_10,
	IPC_MEM_PIPE_11,
	IPC_MEM_PIPE_12,
	IPC_MEM_PIPE_13,
	IPC_MEM_PIPE_14,
	IPC_MEM_PIPE_15,
	IPC_MEM_PIPE_16,
	IPC_MEM_PIPE_17,
	IPC_MEM_PIPE_18,
	IPC_MEM_PIPE_19,
	IPC_MEM_PIPE_20,
	IPC_MEM_PIPE_21,
	IPC_MEM_PIPE_22,
	IPC_MEM_PIPE_23,
	IPC_MEM_MAX_PIPES
};


enum ipc_channel_state {
	IMEM_CHANNEL_FREE,
	IMEM_CHANNEL_RESERVED,
	IMEM_CHANNEL_ACTIVE,
	IMEM_CHANNEL_CLOSING,
};


enum ipc_ctype {
	IPC_CTYPE_WWAN,
	IPC_CTYPE_CTRL,
};


enum ipc_mem_pipe_dir {
	IPC_MEM_DIR_UL,
	IPC_MEM_DIR_DL,
};


enum ipc_hp_identifier {
	IPC_HP_MR = 0,
	IPC_HP_PM_TRIGGER,
	IPC_HP_WAKEUP_SPEC_TMR,
	IPC_HP_TD_UPD_TMR_START,
	IPC_HP_TD_UPD_TMR,
	IPC_HP_FAST_TD_UPD_TMR,
	IPC_HP_UL_WRITE_TD,
	IPC_HP_DL_PROCESS,
	IPC_HP_NET_CHANNEL_INIT,
	IPC_HP_CDEV_OPEN,
};


struct ipc_pipe {
	struct ipc_protocol_td *tdr_start;
	struct ipc_mem_channel *channel;
	struct sk_buff **skbr_start;
	dma_addr_t phy_tdr_start;
	u32 old_head;
	u32 old_tail;
	u32 nr_of_entries;
	u32 max_nr_of_queued_entries;
	u32 accumulation_backoff;
	u32 irq_moderation;
	u32 pipe_nr;
	u32 irq;
	enum ipc_mem_pipe_dir dir;
	u32 buf_size;
	u16 nr_of_queued_entries;
	u8 is_open:1;
};


struct ipc_mem_channel {
	int channel_id;
	enum ipc_ctype ctype;
	int index;
	struct ipc_pipe ul_pipe;
	struct ipc_pipe dl_pipe;
	int if_id;
	u32 net_err_count;
	enum ipc_channel_state state;
	struct completion ul_sem;
	struct sk_buff_head ul_list;
};


enum ipc_phase {
	IPC_P_OFF,
	IPC_P_OFF_REQ,
	IPC_P_CRASH,
	IPC_P_CD_READY,
	IPC_P_ROM,
	IPC_P_PSI,
	IPC_P_EBL,
	IPC_P_RUN,
};


struct iosm_imem {
	struct iosm_mmio *mmio;
	struct iosm_protocol *ipc_protocol;
	struct ipc_task *ipc_task;
	struct iosm_wwan *wwan;
	struct iosm_mux *mux;
	struct iosm_cdev *ipc_port[IPC_MEM_MAX_CHANNELS];
	struct iosm_pcie *pcie;
#ifdef CONFIG_WWAN_DEBUGFS
	struct iosm_trace *trace;
#endif
	struct device *dev;
	enum ipc_mem_device_ipc_state ipc_requested_state;
	struct ipc_mem_channel channels[IPC_MEM_MAX_CHANNELS];
	struct iosm_devlink *ipc_devlink;
	u32 ipc_status;
	u32 nr_of_channels;
	struct hrtimer startup_timer;
	ktime_t hrtimer_period;
	struct hrtimer tdupdate_timer;
	struct hrtimer fast_update_timer;
	struct hrtimer td_alloc_timer;
	struct hrtimer adb_timer;
	enum rom_exit_code rom_exit_code;
	u32 enter_runtime;
	struct completion ul_pend_sem;
	u32 app_notify_ul_pend;
	struct completion dl_pend_sem;
	u32 app_notify_dl_pend;
	enum ipc_phase phase;
	u16 pci_device_id;
	int cp_version;
	int device_sleep;
	struct work_struct run_state_worker;
	u8 ev_irq_pending[IPC_IRQ_VECTORS];
	unsigned long flag;
	u8 td_update_timer_suspended:1,
	   ev_mux_net_transmit_pending:1,
	   reset_det_n:1,
	   pcie_wake_n:1;
#ifdef CONFIG_WWAN_DEBUGFS
	struct dentry *debugfs_wwan_dir;
	struct dentry *debugfs_dir;
#endif
};


struct iosm_imem *ipc_imem_init(struct iosm_pcie *pcie, unsigned int device_id,
				void __iomem *mmio, struct device *dev);


void ipc_imem_pm_s2idle_sleep(struct iosm_imem *ipc_imem, bool sleep);


void ipc_imem_pm_suspend(struct iosm_imem *ipc_imem);


void ipc_imem_pm_resume(struct iosm_imem *ipc_imem);


void ipc_imem_cleanup(struct iosm_imem *ipc_imem);


void ipc_imem_irq_process(struct iosm_imem *ipc_imem, int irq);


int imem_get_device_sleep_state(struct iosm_imem *ipc_imem);


void ipc_imem_td_update_timer_suspend(struct iosm_imem *ipc_imem, bool suspend);


void ipc_imem_channel_close(struct iosm_imem *ipc_imem, int channel_id);


int ipc_imem_channel_alloc(struct iosm_imem *ipc_imem, int index,
			   enum ipc_ctype ctype);


struct ipc_mem_channel *ipc_imem_channel_open(struct iosm_imem *ipc_imem,
					      int channel_id, u32 db_id);


void ipc_imem_td_update_timer_start(struct iosm_imem *ipc_imem);


bool ipc_imem_ul_write_td(struct iosm_imem *ipc_imem);


void ipc_imem_ul_send(struct iosm_imem *ipc_imem);


void ipc_imem_channel_update(struct iosm_imem *ipc_imem, int id,
			     struct ipc_chnl_cfg chnl_cfg, u32 irq_moderation);


void ipc_imem_channel_free(struct ipc_mem_channel *channel);


void ipc_imem_hrtimer_stop(struct hrtimer *hr_timer);


void ipc_imem_pipe_cleanup(struct iosm_imem *ipc_imem, struct ipc_pipe *pipe);


void ipc_imem_pipe_close(struct iosm_imem *ipc_imem, struct ipc_pipe *pipe);


enum ipc_phase ipc_imem_phase_update(struct iosm_imem *ipc_imem);


const char *ipc_imem_phase_get_string(enum ipc_phase phase);


void ipc_imem_msg_send_feature_set(struct iosm_imem *ipc_imem,
				   unsigned int reset_enable, bool atomic_ctx);


void ipc_imem_ipc_init_check(struct iosm_imem *ipc_imem);


void ipc_imem_channel_init(struct iosm_imem *ipc_imem, enum ipc_ctype ctype,
			   struct ipc_chnl_cfg chnl_cfg, u32 irq_moderation);


int ipc_imem_devlink_trigger_chip_info(struct iosm_imem *ipc_imem);

void ipc_imem_adb_timer_start(struct iosm_imem *ipc_imem);

#endif
