#ifndef KMB_PLATFORM_H_
#define KMB_PLATFORM_H_
#include <linux/bits.h>
#include <linux/bitfield.h>
#include <linux/types.h>
#include <sound/dmaengine_pcm.h>
#define IER		0x000
#define IRER		0x004
#define ITER		0x008
#define CER		0x00C
#define CCR		0x010
#define RXFFR		0x014
#define TXFFR		0x018
#define ISR_TXFO	BIT(5)
#define ISR_TXFE	BIT(4)
#define ISR_RXFO	BIT(1)
#define ISR_RXDA	BIT(0)
#define LRBR_LTHR(x)	(0x40 * (x) + 0x020)
#define RRBR_RTHR(x)	(0x40 * (x) + 0x024)
#define RER(x)		(0x40 * (x) + 0x028)
#define TER(x)		(0x40 * (x) + 0x02C)
#define RCR(x)		(0x40 * (x) + 0x030)
#define TCR(x)		(0x40 * (x) + 0x034)
#define ISR(x)		(0x40 * (x) + 0x038)
#define IMR(x)		(0x40 * (x) + 0x03C)
#define ROR(x)		(0x40 * (x) + 0x040)
#define TOR(x)		(0x40 * (x) + 0x044)
#define RFCR(x)		(0x40 * (x) + 0x048)
#define TFCR(x)		(0x40 * (x) + 0x04C)
#define RFF(x)		(0x40 * (x) + 0x050)
#define TFF(x)		(0x40 * (x) + 0x054)
#define I2S_COMP_PARAM_2	0x01F0
#define I2S_COMP_PARAM_1	0x01F4
#define I2S_COMP_VERSION	0x01F8
#define I2S_COMP_TYPE		0x01FC
#define I2S_GEN_CFG_0		0x000
#define PSS_CPR_RST_EN		0x010
#define PSS_CPR_RST_SET		0x014
#define PSS_CPR_CLK_CLR		0x000
#define PSS_CPR_AUX_RST_EN	0x070
#define CLOCK_PROVIDER_MODE	BIT(13)
#define TX_INT_FLAG		GENMASK(5, 4)
#define RX_INT_FLAG		GENMASK(1, 0)
#define	COMP1_TX_WORDSIZE_3(r)		FIELD_GET(GENMASK(27, 25), (r))
#define	COMP1_TX_WORDSIZE_2(r)		FIELD_GET(GENMASK(24, 22), (r))
#define	COMP1_TX_WORDSIZE_1(r)		FIELD_GET(GENMASK(21, 19), (r))
#define	COMP1_TX_WORDSIZE_0(r)		FIELD_GET(GENMASK(18, 16), (r))
#define	COMP1_RX_ENABLED(r)		FIELD_GET(BIT(6), (r))
#define	COMP1_TX_ENABLED(r)		FIELD_GET(BIT(5), (r))
#define	COMP1_MODE_EN(r)		FIELD_GET(BIT(4), (r))
#define	COMP1_APB_DATA_WIDTH(r)		FIELD_GET(GENMASK(1, 0), (r))
#define	COMP2_RX_WORDSIZE_3(r)		FIELD_GET(GENMASK(12, 10), (r))
#define	COMP2_RX_WORDSIZE_2(r)		FIELD_GET(GENMASK(9, 7), (r))
#define	COMP2_RX_WORDSIZE_1(r)		FIELD_GET(GENMASK(5, 3), (r))
#define	COMP2_RX_WORDSIZE_0(r)		FIELD_GET(GENMASK(2, 0), (r))
#define	COMP1_TX_CHANNELS(r)	(FIELD_GET(GENMASK(10, 9), (r)) + 1)
#define	COMP1_RX_CHANNELS(r)	(FIELD_GET(GENMASK(8, 7), (r)) + 1)
#define	COMP1_FIFO_DEPTH(r)	(FIELD_GET(GENMASK(3, 2), (r)) + 1)
#define	COMP_MAX_WORDSIZE	8	 
#define MAX_CHANNEL_NUM		8
#define MIN_CHANNEL_NUM		2
#define MAX_ISR			4
#define TWO_CHANNEL_SUPPORT	2	 
#define FOUR_CHANNEL_SUPPORT	4	 
#define SIX_CHANNEL_SUPPORT	6	 
#define EIGHT_CHANNEL_SUPPORT	8	 
#define DWC_I2S_PLAY	BIT(0)
#define DWC_I2S_RECORD	BIT(1)
#define DW_I2S_CONSUMER	BIT(2)
#define DW_I2S_PROVIDER	BIT(3)
#define I2S_RXDMA	0x01C0
#define I2S_RRXDMA	0x01C4
#define I2S_TXDMA	0x01C8
#define I2S_RTXDMA	0x01CC
#define I2S_DMACR	0x0200
#define I2S_DMAEN_RXBLOCK	(1 << 16)
#define I2S_DMAEN_TXBLOCK	(1 << 17)
struct i2s_clk_config_data {
	int chan_nr;
	u32 data_width;
	u32 sample_rate;
};
struct kmb_i2s_info {
	void __iomem *i2s_base;
	void __iomem *pss_base;
	struct clk *clk_i2s;
	struct clk *clk_apb;
	int active;
	unsigned int capability;
	unsigned int i2s_reg_comp1;
	unsigned int i2s_reg_comp2;
	struct device *dev;
	u32 ccr;
	u32 xfer_resolution;
	u32 fifo_th;
	bool clock_provider;
	struct snd_dmaengine_dai_dma_data play_dma_data;
	struct snd_dmaengine_dai_dma_data capture_dma_data;
	struct i2s_clk_config_data config;
	int (*i2s_clk_cfg)(struct i2s_clk_config_data *config);
	bool use_pio;
	struct snd_pcm_substream *tx_substream;
	struct snd_pcm_substream *rx_substream;
	unsigned int tx_ptr;
	unsigned int rx_ptr;
	bool iec958_fmt;
};
#endif  
