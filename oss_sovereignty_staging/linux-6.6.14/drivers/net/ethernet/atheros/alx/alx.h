 

#ifndef _ALX_H_
#define _ALX_H_

#include <linux/types.h>
#include <linux/etherdevice.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include "hw.h"

#define ALX_WATCHDOG_TIME   (5 * HZ)

struct alx_buffer {
	struct sk_buff *skb;
	DEFINE_DMA_UNMAP_ADDR(dma);
	DEFINE_DMA_UNMAP_LEN(size);
};

struct alx_rx_queue {
	struct net_device *netdev;
	struct device *dev;
	struct alx_napi *np;

	struct alx_rrd *rrd;
	dma_addr_t rrd_dma;

	struct alx_rfd *rfd;
	dma_addr_t rfd_dma;

	struct alx_buffer *bufs;

	u16 count;
	u16 write_idx, read_idx;
	u16 rrd_read_idx;
	u16 queue_idx;
};
#define ALX_RX_ALLOC_THRESH	32

struct alx_tx_queue {
	struct net_device *netdev;
	struct device *dev;

	struct alx_txd *tpd;
	dma_addr_t tpd_dma;

	struct alx_buffer *bufs;

	u16 count;
	u16 write_idx, read_idx;
	u16 queue_idx;
	u16 p_reg, c_reg;
};

#define ALX_DEFAULT_TX_WORK 128

enum alx_device_quirks {
	ALX_DEV_QUIRK_MSI_INTX_DISABLE_BUG = BIT(0),
};

struct alx_napi {
	struct napi_struct	napi;
	struct alx_priv		*alx;
	struct alx_rx_queue	*rxq;
	struct alx_tx_queue	*txq;
	int			vec_idx;
	u32			vec_mask;
	char			irq_lbl[IFNAMSIZ + 8];
};

#define ALX_MAX_NAPIS 8

struct alx_priv {
	struct net_device *dev;

	struct alx_hw hw;

	 
	int num_vec;

	 
	struct {
		dma_addr_t dma;
		void *virt;
		unsigned int size;
	} descmem;

	struct alx_napi *qnapi[ALX_MAX_NAPIS];
	int num_txq;
	int num_rxq;
	int num_napi;

	 
	spinlock_t irq_lock;
	u32 int_mask;

	unsigned int tx_ringsz;
	unsigned int rx_ringsz;
	unsigned int rxbuf_size;

	struct work_struct link_check_wk;
	struct work_struct reset_wk;

	u16 msg_enable;

	 
	spinlock_t stats_lock;

	struct mutex mtx;
};

extern const struct ethtool_ops alx_ethtool_ops;

#endif
