

#ifndef __SXGBE_DMA_H__
#define __SXGBE_DMA_H__


struct sxgbe_extra_stats;

#define SXGBE_DMA_BLENMAP_LSHIFT	1
#define SXGBE_DMA_TXPBL_LSHIFT		16
#define SXGBE_DMA_RXPBL_LSHIFT		16
#define DEFAULT_DMA_PBL			8

struct sxgbe_dma_ops {
	
	int (*init)(void __iomem *ioaddr, int fix_burst, int burst_map);
	void (*cha_init)(void __iomem *ioaddr, int cha_num, int fix_burst,
			 int pbl, dma_addr_t dma_tx, dma_addr_t dma_rx,
			 int t_rzie, int r_rsize);
	void (*enable_dma_transmission)(void __iomem *ioaddr, int dma_cnum);
	void (*enable_dma_irq)(void __iomem *ioaddr, int dma_cnum);
	void (*disable_dma_irq)(void __iomem *ioaddr, int dma_cnum);
	void (*start_tx)(void __iomem *ioaddr, int tchannels);
	void (*start_tx_queue)(void __iomem *ioaddr, int dma_cnum);
	void (*stop_tx)(void __iomem *ioaddr, int tchannels);
	void (*stop_tx_queue)(void __iomem *ioaddr, int dma_cnum);
	void (*start_rx)(void __iomem *ioaddr, int rchannels);
	void (*stop_rx)(void __iomem *ioaddr, int rchannels);
	int (*tx_dma_int_status)(void __iomem *ioaddr, int channel_no,
				 struct sxgbe_extra_stats *x);
	int (*rx_dma_int_status)(void __iomem *ioaddr, int channel_no,
				 struct sxgbe_extra_stats *x);
	
	void (*rx_watchdog)(void __iomem *ioaddr, u32 riwt);
	
	void (*enable_tso)(void __iomem *ioaddr, u8 chan_num);
};

const struct sxgbe_dma_ops *sxgbe_get_dma_ops(void);

#endif 
