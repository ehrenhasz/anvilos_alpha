

#ifndef __T7XX_HIF_CLDMA_H__
#define __T7XX_HIF_CLDMA_H__

#include <linux/bits.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/pci.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/types.h>

#include "t7xx_cldma.h"
#include "t7xx_pci.h"


enum cldma_id {
	CLDMA_ID_MD,
	CLDMA_ID_AP,
	CLDMA_NUM
};

struct cldma_gpd {
	u8 flags;
	u8 not_used1;
	__le16 rx_data_allow_len;
	__le32 next_gpd_ptr_h;
	__le32 next_gpd_ptr_l;
	__le32 data_buff_bd_ptr_h;
	__le32 data_buff_bd_ptr_l;
	__le16 data_buff_len;
	__le16 not_used2;
};

struct cldma_request {
	struct cldma_gpd *gpd;	
	dma_addr_t gpd_addr;	
	struct sk_buff *skb;
	dma_addr_t mapped_buff;
	struct list_head entry;
};

struct cldma_ring {
	struct list_head gpd_ring;	
	unsigned int length;		
	int pkt_size;
};

struct cldma_queue {
	struct cldma_ctrl *md_ctrl;
	enum mtk_txrx dir;
	unsigned int index;
	struct cldma_ring *tr_ring;
	struct cldma_request *tr_done;
	struct cldma_request *rx_refill;
	struct cldma_request *tx_next;
	int budget;			
	spinlock_t ring_lock;
	wait_queue_head_t req_wq;	
	struct workqueue_struct *worker;
	struct work_struct cldma_work;
};

struct cldma_ctrl {
	enum cldma_id hif_id;
	struct device *dev;
	struct t7xx_pci_dev *t7xx_dev;
	struct cldma_queue txq[CLDMA_TXQ_NUM];
	struct cldma_queue rxq[CLDMA_RXQ_NUM];
	unsigned short txq_active;
	unsigned short rxq_active;
	unsigned short txq_started;
	spinlock_t cldma_lock; 
	
	struct dma_pool *gpd_dmapool;
	struct cldma_ring tx_ring[CLDMA_TXQ_NUM];
	struct cldma_ring rx_ring[CLDMA_RXQ_NUM];
	struct md_pm_entity *pm_entity;
	struct t7xx_cldma_hw hw_info;
	bool is_late_init;
	int (*recv_skb)(struct cldma_queue *queue, struct sk_buff *skb);
};

#define GPD_FLAGS_HWO		BIT(0)
#define GPD_FLAGS_IOC		BIT(7)
#define GPD_DMAPOOL_ALIGN	16

#define CLDMA_MTU		3584	

int t7xx_cldma_alloc(enum cldma_id hif_id, struct t7xx_pci_dev *t7xx_dev);
void t7xx_cldma_hif_hw_init(struct cldma_ctrl *md_ctrl);
int t7xx_cldma_init(struct cldma_ctrl *md_ctrl);
void t7xx_cldma_exit(struct cldma_ctrl *md_ctrl);
void t7xx_cldma_switch_cfg(struct cldma_ctrl *md_ctrl);
void t7xx_cldma_start(struct cldma_ctrl *md_ctrl);
int t7xx_cldma_stop(struct cldma_ctrl *md_ctrl);
void t7xx_cldma_reset(struct cldma_ctrl *md_ctrl);
void t7xx_cldma_set_recv_skb(struct cldma_ctrl *md_ctrl,
			     int (*recv_skb)(struct cldma_queue *queue, struct sk_buff *skb));
int t7xx_cldma_send_skb(struct cldma_ctrl *md_ctrl, int qno, struct sk_buff *skb);
void t7xx_cldma_stop_all_qs(struct cldma_ctrl *md_ctrl, enum mtk_txrx tx_rx);
void t7xx_cldma_clear_all_qs(struct cldma_ctrl *md_ctrl, enum mtk_txrx tx_rx);

#endif 
