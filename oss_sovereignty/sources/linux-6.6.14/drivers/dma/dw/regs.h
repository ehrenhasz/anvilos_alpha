


#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/dmaengine.h>

#include <linux/io-64-nonatomic-hi-lo.h>

#include "internal.h"

#define DW_DMA_MAX_NR_REQUESTS	16


enum dw_dma_fc {
	DW_DMA_FC_D_M2M,
	DW_DMA_FC_D_M2P,
	DW_DMA_FC_D_P2M,
	DW_DMA_FC_D_P2P,
	DW_DMA_FC_P_P2M,
	DW_DMA_FC_SP_P2P,
	DW_DMA_FC_P_M2P,
	DW_DMA_FC_DP_P2P,
};


#define DW_REG(name)		u32 name; u32 __pad_##name


struct dw_dma_chan_regs {
	DW_REG(SAR);		
	DW_REG(DAR);		
	DW_REG(LLP);		
	u32	CTL_LO;		
	u32	CTL_HI;		
	DW_REG(SSTAT);
	DW_REG(DSTAT);
	DW_REG(SSTATAR);
	DW_REG(DSTATAR);
	u32	CFG_LO;		
	u32	CFG_HI;		
	DW_REG(SGR);
	DW_REG(DSR);
};

struct dw_dma_irq_regs {
	DW_REG(XFER);
	DW_REG(BLOCK);
	DW_REG(SRC_TRAN);
	DW_REG(DST_TRAN);
	DW_REG(ERROR);
};

struct dw_dma_regs {
	
	struct dw_dma_chan_regs	CHAN[DW_DMA_MAX_NR_CHANNELS];

	
	struct dw_dma_irq_regs	RAW;		
	struct dw_dma_irq_regs	STATUS;		
	struct dw_dma_irq_regs	MASK;		
	struct dw_dma_irq_regs	CLEAR;		

	DW_REG(STATUS_INT);			

	
	DW_REG(REQ_SRC);
	DW_REG(REQ_DST);
	DW_REG(SGL_REQ_SRC);
	DW_REG(SGL_REQ_DST);
	DW_REG(LAST_SRC);
	DW_REG(LAST_DST);

	
	DW_REG(CFG);
	DW_REG(CH_EN);
	DW_REG(ID);
	DW_REG(TEST);

	
	DW_REG(CLASS_PRIORITY0);
	DW_REG(CLASS_PRIORITY1);

	
	u32	__reserved;

	
	u32	DWC_PARAMS[DW_DMA_MAX_NR_CHANNELS];
	u32	MULTI_BLK_TYPE;
	u32	MAX_BLK_SIZE;

	
	u32	DW_PARAMS;

	
	u32	COMP_TYPE;
	u32	COMP_VERSION;

	
	DW_REG(FIFO_PARTITION0);
	DW_REG(FIFO_PARTITION1);

	DW_REG(SAI_ERR);
	DW_REG(GLOBAL_CFG);
};


#define DW_PARAMS_NR_CHAN	8		
#define DW_PARAMS_NR_MASTER	11		
#define DW_PARAMS_DATA_WIDTH(n)	(15 + 2 * (n))
#define DW_PARAMS_DATA_WIDTH1	15		
#define DW_PARAMS_DATA_WIDTH2	17		
#define DW_PARAMS_DATA_WIDTH3	19		
#define DW_PARAMS_DATA_WIDTH4	21		
#define DW_PARAMS_EN		28		


#define DWC_PARAMS_MBLK_EN	11		
#define DWC_PARAMS_HC_LLP	13		
#define DWC_PARAMS_MSIZE	16		


enum dw_dma_msize {
	DW_DMA_MSIZE_1,
	DW_DMA_MSIZE_4,
	DW_DMA_MSIZE_8,
	DW_DMA_MSIZE_16,
	DW_DMA_MSIZE_32,
	DW_DMA_MSIZE_64,
	DW_DMA_MSIZE_128,
	DW_DMA_MSIZE_256,
};


#define DWC_LLP_LMS(x)		((x) & 3)	
#define DWC_LLP_LOC(x)		((x) & ~3)	


#define DWC_CTLL_INT_EN		(1 << 0)	
#define DWC_CTLL_DST_WIDTH(n)	((n)<<1)	
#define DWC_CTLL_SRC_WIDTH(n)	((n)<<4)
#define DWC_CTLL_DST_INC	(0<<7)		
#define DWC_CTLL_DST_DEC	(1<<7)
#define DWC_CTLL_DST_FIX	(2<<7)
#define DWC_CTLL_SRC_INC	(0<<9)		
#define DWC_CTLL_SRC_DEC	(1<<9)
#define DWC_CTLL_SRC_FIX	(2<<9)
#define DWC_CTLL_DST_MSIZE(n)	((n)<<11)	
#define DWC_CTLL_SRC_MSIZE(n)	((n)<<14)
#define DWC_CTLL_S_GATH_EN	(1 << 17)	
#define DWC_CTLL_D_SCAT_EN	(1 << 18)	
#define DWC_CTLL_FC(n)		((n) << 20)
#define DWC_CTLL_FC_M2M		(0 << 20)	
#define DWC_CTLL_FC_M2P		(1 << 20)	
#define DWC_CTLL_FC_P2M		(2 << 20)	
#define DWC_CTLL_FC_P2P		(3 << 20)	

#define DWC_CTLL_DMS(n)		((n)<<23)	
#define DWC_CTLL_SMS(n)		((n)<<25)	
#define DWC_CTLL_LLP_D_EN	(1 << 27)	
#define DWC_CTLL_LLP_S_EN	(1 << 28)	


#define DWC_CTLH_BLOCK_TS_MASK	GENMASK(11, 0)
#define DWC_CTLH_BLOCK_TS(x)	((x) & DWC_CTLH_BLOCK_TS_MASK)
#define DWC_CTLH_DONE		(1 << 12)


#define DWC_CFGL_CH_PRIOR_MASK	(0x7 << 5)	
#define DWC_CFGL_CH_PRIOR(x)	((x) << 5)	
#define DWC_CFGL_CH_SUSP	(1 << 8)	
#define DWC_CFGL_FIFO_EMPTY	(1 << 9)	
#define DWC_CFGL_HS_DST		(1 << 10)	
#define DWC_CFGL_HS_SRC		(1 << 11)	
#define DWC_CFGL_LOCK_CH_XFER	(0 << 12)	
#define DWC_CFGL_LOCK_CH_BLOCK	(1 << 12)
#define DWC_CFGL_LOCK_CH_XACT	(2 << 12)
#define DWC_CFGL_LOCK_BUS_XFER	(0 << 14)	
#define DWC_CFGL_LOCK_BUS_BLOCK	(1 << 14)
#define DWC_CFGL_LOCK_BUS_XACT	(2 << 14)
#define DWC_CFGL_LOCK_CH	(1 << 15)	
#define DWC_CFGL_LOCK_BUS	(1 << 16)	
#define DWC_CFGL_HS_DST_POL	(1 << 18)	
#define DWC_CFGL_HS_SRC_POL	(1 << 19)	
#define DWC_CFGL_MAX_BURST(x)	((x) << 20)
#define DWC_CFGL_RELOAD_SAR	(1 << 30)
#define DWC_CFGL_RELOAD_DAR	(1 << 31)


#define DWC_CFGH_FCMODE		(1 << 0)
#define DWC_CFGH_FIFO_MODE	(1 << 1)
#define DWC_CFGH_PROTCTL(x)	((x) << 2)
#define DWC_CFGH_PROTCTL_DATA	(0 << 2)	
#define DWC_CFGH_PROTCTL_PRIV	(1 << 2)	
#define DWC_CFGH_PROTCTL_BUFFER	(2 << 2)	
#define DWC_CFGH_PROTCTL_CACHE	(4 << 2)	
#define DWC_CFGH_DS_UPD_EN	(1 << 5)
#define DWC_CFGH_SS_UPD_EN	(1 << 6)
#define DWC_CFGH_SRC_PER(x)	((x) << 7)
#define DWC_CFGH_DST_PER(x)	((x) << 11)


#define DWC_SGR_SGI(x)		((x) << 0)
#define DWC_SGR_SGC(x)		((x) << 20)


#define DWC_DSR_DSI(x)		((x) << 0)
#define DWC_DSR_DSC(x)		((x) << 20)


#define DW_CFG_DMA_EN		(1 << 0)




enum idma32_msize {
	IDMA32_MSIZE_1,
	IDMA32_MSIZE_2,
	IDMA32_MSIZE_4,
	IDMA32_MSIZE_8,
	IDMA32_MSIZE_16,
	IDMA32_MSIZE_32,
};


#define IDMA32C_CTLH_BLOCK_TS_MASK	GENMASK(16, 0)
#define IDMA32C_CTLH_BLOCK_TS(x)	((x) & IDMA32C_CTLH_BLOCK_TS_MASK)
#define IDMA32C_CTLH_DONE		(1 << 17)


#define IDMA32C_CFGL_DST_BURST_ALIGN	(1 << 0)	
#define IDMA32C_CFGL_SRC_BURST_ALIGN	(1 << 1)	
#define IDMA32C_CFGL_CH_DRAIN		(1 << 10)	
#define IDMA32C_CFGL_DST_OPT_BL		(1 << 20)	
#define IDMA32C_CFGL_SRC_OPT_BL		(1 << 21)	


#define IDMA32C_CFGH_SRC_PER(x)		((x) << 0)
#define IDMA32C_CFGH_DST_PER(x)		((x) << 4)
#define IDMA32C_CFGH_RD_ISSUE_THD(x)	((x) << 8)
#define IDMA32C_CFGH_RW_ISSUE_THD(x)	((x) << 18)
#define IDMA32C_CFGH_SRC_PER_EXT(x)	((x) << 28)	
#define IDMA32C_CFGH_DST_PER_EXT(x)	((x) << 30)	


#define IDMA32C_FP_PSIZE_CH0(x)		((x) << 0)
#define IDMA32C_FP_PSIZE_CH1(x)		((x) << 13)
#define IDMA32C_FP_UPDATE		(1 << 26)

enum dw_dmac_flags {
	DW_DMA_IS_CYCLIC = 0,
	DW_DMA_IS_SOFT_LLP = 1,
	DW_DMA_IS_PAUSED = 2,
	DW_DMA_IS_INITIALIZED = 3,
};

struct dw_dma_chan {
	struct dma_chan			chan;
	void __iomem			*ch_regs;
	u8				mask;
	u8				priority;
	enum dma_transfer_direction	direction;

	
	struct list_head	*tx_node_active;

	spinlock_t		lock;

	
	unsigned long		flags;
	struct list_head	active_list;
	struct list_head	queue;

	unsigned int		descs_allocated;

	
	unsigned int		block_size;
	bool			nollp;
	u32			max_burst;

	
	struct dw_dma_slave	dws;

	
	struct dma_slave_config dma_sconfig;
};

static inline struct dw_dma_chan_regs __iomem *
__dwc_regs(struct dw_dma_chan *dwc)
{
	return dwc->ch_regs;
}

#define channel_readl(dwc, name) \
	readl(&(__dwc_regs(dwc)->name))
#define channel_writel(dwc, name, val) \
	writel((val), &(__dwc_regs(dwc)->name))

static inline struct dw_dma_chan *to_dw_dma_chan(struct dma_chan *chan)
{
	return container_of(chan, struct dw_dma_chan, chan);
}

struct dw_dma {
	struct dma_device	dma;
	char			name[20];
	void __iomem		*regs;
	struct dma_pool		*desc_pool;
	struct tasklet_struct	tasklet;

	
	struct dw_dma_chan	*chan;
	u8			all_chan_mask;
	u8			in_use;

	
	void	(*initialize_chan)(struct dw_dma_chan *dwc);
	void	(*suspend_chan)(struct dw_dma_chan *dwc, bool drain);
	void	(*resume_chan)(struct dw_dma_chan *dwc, bool drain);
	u32	(*prepare_ctllo)(struct dw_dma_chan *dwc);
	void	(*encode_maxburst)(struct dw_dma_chan *dwc, u32 *maxburst);
	u32	(*bytes2block)(struct dw_dma_chan *dwc, size_t bytes,
			       unsigned int width, size_t *len);
	size_t	(*block2bytes)(struct dw_dma_chan *dwc, u32 block, u32 width);

	
	void (*set_device_name)(struct dw_dma *dw, int id);
	void (*disable)(struct dw_dma *dw);
	void (*enable)(struct dw_dma *dw);

	
	struct dw_dma_platform_data	*pdata;
};

static inline struct dw_dma_regs __iomem *__dw_regs(struct dw_dma *dw)
{
	return dw->regs;
}

#define dma_readl(dw, name) \
	readl(&(__dw_regs(dw)->name))
#define dma_writel(dw, name, val) \
	writel((val), &(__dw_regs(dw)->name))

#define idma32_readq(dw, name)				\
	hi_lo_readq(&(__dw_regs(dw)->name))
#define idma32_writeq(dw, name, val)			\
	hi_lo_writeq((val), &(__dw_regs(dw)->name))

#define channel_set_bit(dw, reg, mask) \
	dma_writel(dw, reg, ((mask) << 8) | (mask))
#define channel_clear_bit(dw, reg, mask) \
	dma_writel(dw, reg, ((mask) << 8) | 0)

static inline struct dw_dma *to_dw_dma(struct dma_device *ddev)
{
	return container_of(ddev, struct dw_dma, dma);
}


struct dw_lli {
	
	__le32		sar;
	__le32		dar;
	__le32		llp;		
	__le32		ctllo;
	
	__le32		ctlhi;
	
	__le32		sstat;
	__le32		dstat;
};

struct dw_desc {
	
	struct dw_lli			lli;

#define lli_set(d, reg, v)		((d)->lli.reg |= cpu_to_le32(v))
#define lli_clear(d, reg, v)		((d)->lli.reg &= ~cpu_to_le32(v))
#define lli_read(d, reg)		le32_to_cpu((d)->lli.reg)
#define lli_write(d, reg, v)		((d)->lli.reg = cpu_to_le32(v))

	
	struct list_head		desc_node;
	struct list_head		tx_list;
	struct dma_async_tx_descriptor	txd;
	size_t				len;
	size_t				total_len;
	u32				residue;
};

#define to_dw_desc(h)	list_entry(h, struct dw_desc, desc_node)

static inline struct dw_desc *
txd_to_dw_desc(struct dma_async_tx_descriptor *txd)
{
	return container_of(txd, struct dw_desc, txd);
}
