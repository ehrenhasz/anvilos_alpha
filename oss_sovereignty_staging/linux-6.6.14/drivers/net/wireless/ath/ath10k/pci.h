 
 

#ifndef _PCI_H_
#define _PCI_H_

#include <linux/interrupt.h>
#include <linux/mutex.h>

#include "hw.h"
#include "ce.h"
#include "ahb.h"

 
#define DIAG_TRANSFER_LIMIT 2048

struct bmi_xfer {
	bool tx_done;
	bool rx_done;
	bool wait_for_resp;
	u32 resp_len;
};

 
struct pcie_state {
	 
	 
	u32 pipe_cfg_addr;

	 
	 
	u32 svc_to_pipe_map;

	 
	u32 msi_requested;

	 
	u32 msi_granted;

	 
	u32 msi_addr;

	 
	u32 msi_data;

	 
	u32 msi_fw_intr_data;

	 
	u32 power_mgmt_method;

	 
	u32 config_flags;
};

 
#define PCIE_CONFIG_FLAG_ENABLE_L1  0x0000001

 
struct ath10k_pci_pipe {
	 
	struct ath10k_ce_pipe *ce_hdl;

	 
	u8 pipe_num;

	 
	struct ath10k *hif_ce_state;

	size_t buf_sz;

	 
	spinlock_t pipe_lock;
};

struct ath10k_pci_supp_chip {
	u32 dev_id;
	u32 rev_id;
};

enum ath10k_pci_irq_mode {
	ATH10K_PCI_IRQ_AUTO = 0,
	ATH10K_PCI_IRQ_LEGACY = 1,
	ATH10K_PCI_IRQ_MSI = 2,
};

struct ath10k_pci {
	struct pci_dev *pdev;
	struct device *dev;
	struct ath10k *ar;
	void __iomem *mem;
	size_t mem_len;

	 
	enum ath10k_pci_irq_mode oper_irq_mode;

	struct ath10k_pci_pipe pipe_info[CE_COUNT_MAX];

	 
	struct ath10k_ce_pipe *ce_diag;
	 
	struct mutex ce_diag_mutex;

	struct work_struct dump_work;

	struct ath10k_ce ce;
	struct timer_list rx_post_retry;

	 
	u16 link_ctl;

	 
	spinlock_t ps_lock;

	 
	unsigned long ps_wake_refcount;

	 
	struct timer_list ps_timer;

	 
	bool ps_awake;

	 
	bool pci_ps;

	 
	int (*pci_soft_reset)(struct ath10k *ar);

	 
	int (*pci_hard_reset)(struct ath10k *ar);

	 
	u32 (*targ_cpu_to_ce_addr)(struct ath10k *ar, u32 addr);

	struct ce_attr *attr;
	struct ce_pipe_config *pipe_config;
	struct ce_service_to_pipe *serv_to_pipe;

	 
	struct ath10k_ahb ahb[];

};

static inline struct ath10k_pci *ath10k_pci_priv(struct ath10k *ar)
{
	return (struct ath10k_pci *)ar->drv_priv;
}

#define ATH10K_PCI_RX_POST_RETRY_MS 50
#define ATH_PCI_RESET_WAIT_MAX 10  
#define PCIE_WAKE_TIMEOUT 30000	 
#define PCIE_WAKE_LATE_US 10000	 

#define BAR_NUM 0

#define CDC_WAR_MAGIC_STR   0xceef0000
#define CDC_WAR_DATA_CE     4

 
#define DIAG_ACCESS_CE_TIMEOUT_US 10000  
#define DIAG_ACCESS_CE_WAIT_US	50

void ath10k_pci_write32(struct ath10k *ar, u32 offset, u32 value);
void ath10k_pci_soc_write32(struct ath10k *ar, u32 addr, u32 val);
void ath10k_pci_reg_write32(struct ath10k *ar, u32 addr, u32 val);

u32 ath10k_pci_read32(struct ath10k *ar, u32 offset);
u32 ath10k_pci_soc_read32(struct ath10k *ar, u32 addr);
u32 ath10k_pci_reg_read32(struct ath10k *ar, u32 addr);

int ath10k_pci_hif_tx_sg(struct ath10k *ar, u8 pipe_id,
			 struct ath10k_hif_sg_item *items, int n_items);
int ath10k_pci_hif_diag_read(struct ath10k *ar, u32 address, void *buf,
			     size_t buf_len);
int ath10k_pci_diag_write_mem(struct ath10k *ar, u32 address,
			      const void *data, int nbytes);
int ath10k_pci_hif_exchange_bmi_msg(struct ath10k *ar, void *req, u32 req_len,
				    void *resp, u32 *resp_len);
int ath10k_pci_hif_map_service_to_pipe(struct ath10k *ar, u16 service_id,
				       u8 *ul_pipe, u8 *dl_pipe);
void ath10k_pci_hif_get_default_pipe(struct ath10k *ar, u8 *ul_pipe,
				     u8 *dl_pipe);
void ath10k_pci_hif_send_complete_check(struct ath10k *ar, u8 pipe,
					int force);
u16 ath10k_pci_hif_get_free_queue_number(struct ath10k *ar, u8 pipe);
void ath10k_pci_hif_power_down(struct ath10k *ar);
int ath10k_pci_alloc_pipes(struct ath10k *ar);
void ath10k_pci_free_pipes(struct ath10k *ar);
void ath10k_pci_rx_replenish_retry(struct timer_list *t);
void ath10k_pci_ce_deinit(struct ath10k *ar);
void ath10k_pci_init_napi(struct ath10k *ar);
int ath10k_pci_init_pipes(struct ath10k *ar);
int ath10k_pci_init_config(struct ath10k *ar);
void ath10k_pci_rx_post(struct ath10k *ar);
void ath10k_pci_flush(struct ath10k *ar);
void ath10k_pci_enable_legacy_irq(struct ath10k *ar);
bool ath10k_pci_irq_pending(struct ath10k *ar);
void ath10k_pci_disable_and_clear_legacy_irq(struct ath10k *ar);
void ath10k_pci_irq_msi_fw_mask(struct ath10k *ar);
int ath10k_pci_wait_for_target_init(struct ath10k *ar);
int ath10k_pci_setup_resource(struct ath10k *ar);
void ath10k_pci_release_resource(struct ath10k *ar);

 
#define ATH10K_PCI_SLEEP_GRACE_PERIOD_MSEC 60

#endif  
