#ifndef _AXI_DMA_PLATFORM_H
#define _AXI_DMA_PLATFORM_H
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/types.h>
#include "../virt-dma.h"
#define DMAC_MAX_CHANNELS	16
#define DMAC_MAX_MASTERS	2
#define DMAC_MAX_BLK_SIZE	0x200000
struct dw_axi_dma_hcfg {
	u32	nr_channels;
	u32	nr_masters;
	u32	m_data_width;
	u32	block_size[DMAC_MAX_CHANNELS];
	u32	priority[DMAC_MAX_CHANNELS];
	u32	axi_rw_burst_len;
	bool	reg_map_8_channels;
	bool	restrict_axi_burst_len;
	bool	use_cfg2;
};
struct axi_dma_chan {
	struct axi_dma_chip		*chip;
	void __iomem			*chan_regs;
	u8				id;
	u8				hw_handshake_num;
	atomic_t			descs_allocated;
	struct dma_pool			*desc_pool;
	struct virt_dma_chan		vc;
	struct axi_dma_desc		*desc;
	struct dma_slave_config		config;
	enum dma_transfer_direction	direction;
	bool				cyclic;
	bool				is_paused;
};
struct dw_axi_dma {
	struct dma_device	dma;
	struct dw_axi_dma_hcfg	*hdata;
	struct device_dma_parameters	dma_parms;
	struct axi_dma_chan	*chan;
};
struct axi_dma_chip {
	struct device		*dev;
	int			irq;
	void __iomem		*regs;
	void __iomem		*apb_regs;
	struct clk		*core_clk;
	struct clk		*cfgr_clk;
	struct dw_axi_dma	*dw;
};
struct __packed axi_dma_lli {
	__le64		sar;
	__le64		dar;
	__le32		block_ts_lo;
	__le32		block_ts_hi;
	__le64		llp;
	__le32		ctl_lo;
	__le32		ctl_hi;
	__le32		sstat;
	__le32		dstat;
	__le32		status_lo;
	__le32		status_hi;
	__le32		reserved_lo;
	__le32		reserved_hi;
};
struct axi_dma_hw_desc {
	struct axi_dma_lli	*lli;
	dma_addr_t		llp;
	u32			len;
};
struct axi_dma_desc {
	struct axi_dma_hw_desc	*hw_desc;
	struct virt_dma_desc		vd;
	struct axi_dma_chan		*chan;
	u32				completed_blocks;
	u32				length;
	u32				period_len;
};
struct axi_dma_chan_config {
	u8 dst_multblk_type;
	u8 src_multblk_type;
	u8 dst_per;
	u8 src_per;
	u8 tt_fc;
	u8 prior;
	u8 hs_sel_dst;
	u8 hs_sel_src;
};
static inline struct device *dchan2dev(struct dma_chan *dchan)
{
	return &dchan->dev->device;
}
static inline struct device *chan2dev(struct axi_dma_chan *chan)
{
	return &chan->vc.chan.dev->device;
}
static inline struct axi_dma_desc *vd_to_axi_desc(struct virt_dma_desc *vd)
{
	return container_of(vd, struct axi_dma_desc, vd);
}
static inline struct axi_dma_chan *vc_to_axi_dma_chan(struct virt_dma_chan *vc)
{
	return container_of(vc, struct axi_dma_chan, vc);
}
static inline struct axi_dma_chan *dchan_to_axi_dma_chan(struct dma_chan *dchan)
{
	return vc_to_axi_dma_chan(to_virt_chan(dchan));
}
#define COMMON_REG_LEN		0x100
#define CHAN_REG_LEN		0x100
#define DMAC_ID			0x000  
#define DMAC_COMPVER		0x008  
#define DMAC_CFG		0x010  
#define DMAC_CHEN		0x018  
#define DMAC_CHEN_L		0x018  
#define DMAC_CHEN_H		0x01C  
#define DMAC_CHSUSPREG		0x020  
#define DMAC_CHABORTREG		0x028  
#define DMAC_INTSTATUS		0x030  
#define DMAC_COMMON_INTCLEAR	0x038  
#define DMAC_COMMON_INTSTATUS_ENA 0x040  
#define DMAC_COMMON_INTSIGNAL_ENA 0x048  
#define DMAC_COMMON_INTSTATUS	0x050  
#define DMAC_RESET		0x058  
#define CH_SAR			0x000  
#define CH_DAR			0x008  
#define CH_BLOCK_TS		0x010  
#define CH_CTL			0x018  
#define CH_CTL_L		0x018  
#define CH_CTL_H		0x01C  
#define CH_CFG			0x020  
#define CH_CFG_L		0x020  
#define CH_CFG_H		0x024  
#define CH_LLP			0x028  
#define CH_STATUS		0x030  
#define CH_SWHSSRC		0x038  
#define CH_SWHSDST		0x040  
#define CH_BLK_TFR_RESUMEREQ	0x048  
#define CH_AXI_ID		0x050  
#define CH_AXI_QOS		0x058  
#define CH_SSTAT		0x060  
#define CH_DSTAT		0x068  
#define CH_SSTATAR		0x070  
#define CH_DSTATAR		0x078  
#define CH_INTSTATUS_ENA	0x080  
#define CH_INTSTATUS		0x088  
#define CH_INTSIGNAL_ENA	0x090  
#define CH_INTCLEAR		0x098  
#define DMAC_APB_CFG		0x000  
#define DMAC_APB_STAT		0x004  
#define DMAC_APB_DEBUG_STAT_0	0x008  
#define DMAC_APB_DEBUG_STAT_1	0x00C  
#define DMAC_APB_HW_HS_SEL_0	0x010  
#define DMAC_APB_HW_HS_SEL_1	0x014  
#define DMAC_APB_LPI		0x018  
#define DMAC_APB_BYTE_WR_CH_EN	0x01C  
#define DMAC_APB_HALFWORD_WR_CH_EN	0x020  
#define UNUSED_CHANNEL		0x3F  
#define DMA_APB_HS_SEL_BIT_SIZE	0x08  
#define DMA_APB_HS_SEL_MASK	0xFF  
#define MAX_BLOCK_SIZE		0x1000  
#define DMA_REG_MAP_CH_REF	0x08  
#define DMAC_EN_POS			0
#define DMAC_EN_MASK			BIT(DMAC_EN_POS)
#define INT_EN_POS			1
#define INT_EN_MASK			BIT(INT_EN_POS)
#define DMAC_CHAN_EN_SHIFT		0
#define DMAC_CHAN_EN_WE_SHIFT		8
#define DMAC_CHAN_SUSP_SHIFT		16
#define DMAC_CHAN_SUSP_WE_SHIFT		24
#define DMAC_CHAN_EN2_WE_SHIFT		16
#define DMAC_CHAN_SUSP2_SHIFT		0
#define DMAC_CHAN_SUSP2_WE_SHIFT	16
#define CH_CTL_H_ARLEN_EN		BIT(6)
#define CH_CTL_H_ARLEN_POS		7
#define CH_CTL_H_AWLEN_EN		BIT(15)
#define CH_CTL_H_AWLEN_POS		16
enum {
	DWAXIDMAC_ARWLEN_1		= 0,
	DWAXIDMAC_ARWLEN_2		= 1,
	DWAXIDMAC_ARWLEN_4		= 3,
	DWAXIDMAC_ARWLEN_8		= 7,
	DWAXIDMAC_ARWLEN_16		= 15,
	DWAXIDMAC_ARWLEN_32		= 31,
	DWAXIDMAC_ARWLEN_64		= 63,
	DWAXIDMAC_ARWLEN_128		= 127,
	DWAXIDMAC_ARWLEN_256		= 255,
	DWAXIDMAC_ARWLEN_MIN		= DWAXIDMAC_ARWLEN_1,
	DWAXIDMAC_ARWLEN_MAX		= DWAXIDMAC_ARWLEN_256
};
#define CH_CTL_H_LLI_LAST		BIT(30)
#define CH_CTL_H_LLI_VALID		BIT(31)
#define CH_CTL_L_LAST_WRITE_EN		BIT(30)
#define CH_CTL_L_DST_MSIZE_POS		18
#define CH_CTL_L_SRC_MSIZE_POS		14
enum {
	DWAXIDMAC_BURST_TRANS_LEN_1	= 0,
	DWAXIDMAC_BURST_TRANS_LEN_4,
	DWAXIDMAC_BURST_TRANS_LEN_8,
	DWAXIDMAC_BURST_TRANS_LEN_16,
	DWAXIDMAC_BURST_TRANS_LEN_32,
	DWAXIDMAC_BURST_TRANS_LEN_64,
	DWAXIDMAC_BURST_TRANS_LEN_128,
	DWAXIDMAC_BURST_TRANS_LEN_256,
	DWAXIDMAC_BURST_TRANS_LEN_512,
	DWAXIDMAC_BURST_TRANS_LEN_1024
};
#define CH_CTL_L_DST_WIDTH_POS		11
#define CH_CTL_L_SRC_WIDTH_POS		8
#define CH_CTL_L_DST_INC_POS		6
#define CH_CTL_L_SRC_INC_POS		4
enum {
	DWAXIDMAC_CH_CTL_L_INC		= 0,
	DWAXIDMAC_CH_CTL_L_NOINC
};
#define CH_CTL_L_DST_MAST		BIT(2)
#define CH_CTL_L_SRC_MAST		BIT(0)
#define CH_CFG_H_PRIORITY_POS		17
#define CH_CFG_H_DST_PER_POS		12
#define CH_CFG_H_SRC_PER_POS		7
#define CH_CFG_H_HS_SEL_DST_POS		4
#define CH_CFG_H_HS_SEL_SRC_POS		3
enum {
	DWAXIDMAC_HS_SEL_HW		= 0,
	DWAXIDMAC_HS_SEL_SW
};
#define CH_CFG_H_TT_FC_POS		0
enum {
	DWAXIDMAC_TT_FC_MEM_TO_MEM_DMAC	= 0,
	DWAXIDMAC_TT_FC_MEM_TO_PER_DMAC,
	DWAXIDMAC_TT_FC_PER_TO_MEM_DMAC,
	DWAXIDMAC_TT_FC_PER_TO_PER_DMAC,
	DWAXIDMAC_TT_FC_PER_TO_MEM_SRC,
	DWAXIDMAC_TT_FC_PER_TO_PER_SRC,
	DWAXIDMAC_TT_FC_MEM_TO_PER_DST,
	DWAXIDMAC_TT_FC_PER_TO_PER_DST
};
#define CH_CFG_L_DST_MULTBLK_TYPE_POS	2
#define CH_CFG_L_SRC_MULTBLK_TYPE_POS	0
enum {
	DWAXIDMAC_MBLK_TYPE_CONTIGUOUS	= 0,
	DWAXIDMAC_MBLK_TYPE_RELOAD,
	DWAXIDMAC_MBLK_TYPE_SHADOW_REG,
	DWAXIDMAC_MBLK_TYPE_LL
};
#define CH_CFG2_L_SRC_PER_POS		4
#define CH_CFG2_L_DST_PER_POS		11
#define CH_CFG2_H_TT_FC_POS		0
#define CH_CFG2_H_HS_SEL_SRC_POS	3
#define CH_CFG2_H_HS_SEL_DST_POS	4
#define CH_CFG2_H_PRIORITY_POS		20
enum {
	DWAXIDMAC_IRQ_NONE		= 0,
	DWAXIDMAC_IRQ_BLOCK_TRF		= BIT(0),
	DWAXIDMAC_IRQ_DMA_TRF		= BIT(1),
	DWAXIDMAC_IRQ_SRC_TRAN		= BIT(3),
	DWAXIDMAC_IRQ_DST_TRAN		= BIT(4),
	DWAXIDMAC_IRQ_SRC_DEC_ERR	= BIT(5),
	DWAXIDMAC_IRQ_DST_DEC_ERR	= BIT(6),
	DWAXIDMAC_IRQ_SRC_SLV_ERR	= BIT(7),
	DWAXIDMAC_IRQ_DST_SLV_ERR	= BIT(8),
	DWAXIDMAC_IRQ_LLI_RD_DEC_ERR	= BIT(9),
	DWAXIDMAC_IRQ_LLI_WR_DEC_ERR	= BIT(10),
	DWAXIDMAC_IRQ_LLI_RD_SLV_ERR	= BIT(11),
	DWAXIDMAC_IRQ_LLI_WR_SLV_ERR	= BIT(12),
	DWAXIDMAC_IRQ_INVALID_ERR	= BIT(13),
	DWAXIDMAC_IRQ_MULTIBLKTYPE_ERR	= BIT(14),
	DWAXIDMAC_IRQ_DEC_ERR		= BIT(16),
	DWAXIDMAC_IRQ_WR2RO_ERR		= BIT(17),
	DWAXIDMAC_IRQ_RD2RWO_ERR	= BIT(18),
	DWAXIDMAC_IRQ_WRONCHEN_ERR	= BIT(19),
	DWAXIDMAC_IRQ_SHADOWREG_ERR	= BIT(20),
	DWAXIDMAC_IRQ_WRONHOLD_ERR	= BIT(21),
	DWAXIDMAC_IRQ_LOCK_CLEARED	= BIT(27),
	DWAXIDMAC_IRQ_SRC_SUSPENDED	= BIT(28),
	DWAXIDMAC_IRQ_SUSPENDED		= BIT(29),
	DWAXIDMAC_IRQ_DISABLED		= BIT(30),
	DWAXIDMAC_IRQ_ABORTED		= BIT(31),
	DWAXIDMAC_IRQ_ALL_ERR		= (GENMASK(21, 16) | GENMASK(14, 5)),
	DWAXIDMAC_IRQ_ALL		= GENMASK(31, 0)
};
enum {
	DWAXIDMAC_TRANS_WIDTH_8		= 0,
	DWAXIDMAC_TRANS_WIDTH_16,
	DWAXIDMAC_TRANS_WIDTH_32,
	DWAXIDMAC_TRANS_WIDTH_64,
	DWAXIDMAC_TRANS_WIDTH_128,
	DWAXIDMAC_TRANS_WIDTH_256,
	DWAXIDMAC_TRANS_WIDTH_512,
	DWAXIDMAC_TRANS_WIDTH_MAX	= DWAXIDMAC_TRANS_WIDTH_512
};
#endif  
