






#include <linux/bitfield.h>
#include <linux/debugfs.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/spi/spi.h>

#define DRIVER_NAME "spi_stm32"

 
#define STM32F4_SPI_CR1			0x00
#define STM32F4_SPI_CR2			0x04
#define STM32F4_SPI_SR			0x08
#define STM32F4_SPI_DR			0x0C
#define STM32F4_SPI_I2SCFGR		0x1C

 
#define STM32F4_SPI_CR1_CPHA		BIT(0)
#define STM32F4_SPI_CR1_CPOL		BIT(1)
#define STM32F4_SPI_CR1_MSTR		BIT(2)
#define STM32F4_SPI_CR1_BR_SHIFT	3
#define STM32F4_SPI_CR1_BR		GENMASK(5, 3)
#define STM32F4_SPI_CR1_SPE		BIT(6)
#define STM32F4_SPI_CR1_LSBFRST		BIT(7)
#define STM32F4_SPI_CR1_SSI		BIT(8)
#define STM32F4_SPI_CR1_SSM		BIT(9)
#define STM32F4_SPI_CR1_RXONLY		BIT(10)
#define STM32F4_SPI_CR1_DFF		BIT(11)
#define STM32F4_SPI_CR1_CRCNEXT		BIT(12)
#define STM32F4_SPI_CR1_CRCEN		BIT(13)
#define STM32F4_SPI_CR1_BIDIOE		BIT(14)
#define STM32F4_SPI_CR1_BIDIMODE	BIT(15)
#define STM32F4_SPI_CR1_BR_MIN		0
#define STM32F4_SPI_CR1_BR_MAX		(GENMASK(5, 3) >> 3)

 
#define STM32F4_SPI_CR2_RXDMAEN		BIT(0)
#define STM32F4_SPI_CR2_TXDMAEN		BIT(1)
#define STM32F4_SPI_CR2_SSOE		BIT(2)
#define STM32F4_SPI_CR2_FRF		BIT(4)
#define STM32F4_SPI_CR2_ERRIE		BIT(5)
#define STM32F4_SPI_CR2_RXNEIE		BIT(6)
#define STM32F4_SPI_CR2_TXEIE		BIT(7)

 
#define STM32F4_SPI_SR_RXNE		BIT(0)
#define STM32F4_SPI_SR_TXE		BIT(1)
#define STM32F4_SPI_SR_CHSIDE		BIT(2)
#define STM32F4_SPI_SR_UDR		BIT(3)
#define STM32F4_SPI_SR_CRCERR		BIT(4)
#define STM32F4_SPI_SR_MODF		BIT(5)
#define STM32F4_SPI_SR_OVR		BIT(6)
#define STM32F4_SPI_SR_BSY		BIT(7)
#define STM32F4_SPI_SR_FRE		BIT(8)

 
#define STM32F4_SPI_I2SCFGR_I2SMOD	BIT(11)

 
#define STM32F4_SPI_BR_DIV_MIN		(2 << STM32F4_SPI_CR1_BR_MIN)
#define STM32F4_SPI_BR_DIV_MAX		(2 << STM32F4_SPI_CR1_BR_MAX)

 
#define STM32H7_SPI_CR1			0x00
#define STM32H7_SPI_CR2			0x04
#define STM32H7_SPI_CFG1		0x08
#define STM32H7_SPI_CFG2		0x0C
#define STM32H7_SPI_IER			0x10
#define STM32H7_SPI_SR			0x14
#define STM32H7_SPI_IFCR		0x18
#define STM32H7_SPI_TXDR		0x20
#define STM32H7_SPI_RXDR		0x30
#define STM32H7_SPI_I2SCFGR		0x50

 
#define STM32H7_SPI_CR1_SPE		BIT(0)
#define STM32H7_SPI_CR1_MASRX		BIT(8)
#define STM32H7_SPI_CR1_CSTART		BIT(9)
#define STM32H7_SPI_CR1_CSUSP		BIT(10)
#define STM32H7_SPI_CR1_HDDIR		BIT(11)
#define STM32H7_SPI_CR1_SSI		BIT(12)

 
#define STM32H7_SPI_CR2_TSIZE		GENMASK(15, 0)
#define STM32H7_SPI_TSIZE_MAX		GENMASK(15, 0)

 
#define STM32H7_SPI_CFG1_DSIZE		GENMASK(4, 0)
#define STM32H7_SPI_CFG1_FTHLV		GENMASK(8, 5)
#define STM32H7_SPI_CFG1_RXDMAEN	BIT(14)
#define STM32H7_SPI_CFG1_TXDMAEN	BIT(15)
#define STM32H7_SPI_CFG1_MBR		GENMASK(30, 28)
#define STM32H7_SPI_CFG1_MBR_SHIFT	28
#define STM32H7_SPI_CFG1_MBR_MIN	0
#define STM32H7_SPI_CFG1_MBR_MAX	(GENMASK(30, 28) >> 28)

 
#define STM32H7_SPI_CFG2_MIDI		GENMASK(7, 4)
#define STM32H7_SPI_CFG2_COMM		GENMASK(18, 17)
#define STM32H7_SPI_CFG2_SP		GENMASK(21, 19)
#define STM32H7_SPI_CFG2_MASTER		BIT(22)
#define STM32H7_SPI_CFG2_LSBFRST	BIT(23)
#define STM32H7_SPI_CFG2_CPHA		BIT(24)
#define STM32H7_SPI_CFG2_CPOL		BIT(25)
#define STM32H7_SPI_CFG2_SSM		BIT(26)
#define STM32H7_SPI_CFG2_SSIOP		BIT(28)
#define STM32H7_SPI_CFG2_AFCNTR		BIT(31)

 
#define STM32H7_SPI_IER_RXPIE		BIT(0)
#define STM32H7_SPI_IER_TXPIE		BIT(1)
#define STM32H7_SPI_IER_DXPIE		BIT(2)
#define STM32H7_SPI_IER_EOTIE		BIT(3)
#define STM32H7_SPI_IER_TXTFIE		BIT(4)
#define STM32H7_SPI_IER_OVRIE		BIT(6)
#define STM32H7_SPI_IER_MODFIE		BIT(9)
#define STM32H7_SPI_IER_ALL		GENMASK(10, 0)

 
#define STM32H7_SPI_SR_RXP		BIT(0)
#define STM32H7_SPI_SR_TXP		BIT(1)
#define STM32H7_SPI_SR_EOT		BIT(3)
#define STM32H7_SPI_SR_OVR		BIT(6)
#define STM32H7_SPI_SR_MODF		BIT(9)
#define STM32H7_SPI_SR_SUSP		BIT(11)
#define STM32H7_SPI_SR_RXPLVL		GENMASK(14, 13)
#define STM32H7_SPI_SR_RXWNE		BIT(15)

 
#define STM32H7_SPI_IFCR_ALL		GENMASK(11, 3)

 
#define STM32H7_SPI_I2SCFGR_I2SMOD	BIT(0)

 
#define STM32H7_SPI_MBR_DIV_MIN		(2 << STM32H7_SPI_CFG1_MBR_MIN)
#define STM32H7_SPI_MBR_DIV_MAX		(2 << STM32H7_SPI_CFG1_MBR_MAX)

 
#define STM32H7_SPI_FULL_DUPLEX		0
#define STM32H7_SPI_SIMPLEX_TX		1
#define STM32H7_SPI_SIMPLEX_RX		2
#define STM32H7_SPI_HALF_DUPLEX		3

 
#define SPI_FULL_DUPLEX		0
#define SPI_SIMPLEX_TX		1
#define SPI_SIMPLEX_RX		2
#define SPI_3WIRE_TX		3
#define SPI_3WIRE_RX		4

#define STM32_SPI_AUTOSUSPEND_DELAY		1	 

 
#define SPI_DMA_MIN_BYTES	16

 
#define STM32_SPI_MASTER_MODE(stm32_spi) (!(stm32_spi)->device_mode)
#define STM32_SPI_DEVICE_MODE(stm32_spi) ((stm32_spi)->device_mode)

 
struct stm32_spi_reg {
	int reg;
	int mask;
	int shift;
};

 
struct stm32_spi_regspec {
	const struct stm32_spi_reg en;
	const struct stm32_spi_reg dma_rx_en;
	const struct stm32_spi_reg dma_tx_en;
	const struct stm32_spi_reg cpol;
	const struct stm32_spi_reg cpha;
	const struct stm32_spi_reg lsb_first;
	const struct stm32_spi_reg cs_high;
	const struct stm32_spi_reg br;
	const struct stm32_spi_reg rx;
	const struct stm32_spi_reg tx;
};

struct stm32_spi;

 
struct stm32_spi_cfg {
	const struct stm32_spi_regspec *regs;
	int (*get_fifo_size)(struct stm32_spi *spi);
	int (*get_bpw_mask)(struct stm32_spi *spi);
	void (*disable)(struct stm32_spi *spi);
	int (*config)(struct stm32_spi *spi);
	void (*set_bpw)(struct stm32_spi *spi);
	int (*set_mode)(struct stm32_spi *spi, unsigned int comm_type);
	void (*set_data_idleness)(struct stm32_spi *spi, u32 length);
	int (*set_number_of_data)(struct stm32_spi *spi, u32 length);
	void (*transfer_one_dma_start)(struct stm32_spi *spi);
	void (*dma_rx_cb)(void *data);
	void (*dma_tx_cb)(void *data);
	int (*transfer_one_irq)(struct stm32_spi *spi);
	irqreturn_t (*irq_handler_event)(int irq, void *dev_id);
	irqreturn_t (*irq_handler_thread)(int irq, void *dev_id);
	unsigned int baud_rate_div_min;
	unsigned int baud_rate_div_max;
	bool has_fifo;
	bool has_device_mode;
	u16 flags;
};

 
struct stm32_spi {
	struct device *dev;
	struct spi_controller *ctrl;
	const struct stm32_spi_cfg *cfg;
	void __iomem *base;
	struct clk *clk;
	u32 clk_rate;
	spinlock_t lock;  
	int irq;
	unsigned int fifo_size;

	unsigned int cur_midi;
	unsigned int cur_speed;
	unsigned int cur_half_period;
	unsigned int cur_bpw;
	unsigned int cur_fthlv;
	unsigned int cur_comm;
	unsigned int cur_xferlen;
	bool cur_usedma;

	const void *tx_buf;
	void *rx_buf;
	int tx_len;
	int rx_len;
	struct dma_chan *dma_tx;
	struct dma_chan *dma_rx;
	dma_addr_t phys_addr;

	bool device_mode;
};

static const struct stm32_spi_regspec stm32f4_spi_regspec = {
	.en = { STM32F4_SPI_CR1, STM32F4_SPI_CR1_SPE },

	.dma_rx_en = { STM32F4_SPI_CR2, STM32F4_SPI_CR2_RXDMAEN },
	.dma_tx_en = { STM32F4_SPI_CR2, STM32F4_SPI_CR2_TXDMAEN },

	.cpol = { STM32F4_SPI_CR1, STM32F4_SPI_CR1_CPOL },
	.cpha = { STM32F4_SPI_CR1, STM32F4_SPI_CR1_CPHA },
	.lsb_first = { STM32F4_SPI_CR1, STM32F4_SPI_CR1_LSBFRST },
	.cs_high = {},
	.br = { STM32F4_SPI_CR1, STM32F4_SPI_CR1_BR, STM32F4_SPI_CR1_BR_SHIFT },

	.rx = { STM32F4_SPI_DR },
	.tx = { STM32F4_SPI_DR },
};

static const struct stm32_spi_regspec stm32h7_spi_regspec = {
	 
	.en = { STM32H7_SPI_CR1, STM32H7_SPI_CR1_SPE },

	.dma_rx_en = { STM32H7_SPI_CFG1, STM32H7_SPI_CFG1_RXDMAEN },
	.dma_tx_en = { STM32H7_SPI_CFG1, STM32H7_SPI_CFG1_TXDMAEN },

	.cpol = { STM32H7_SPI_CFG2, STM32H7_SPI_CFG2_CPOL },
	.cpha = { STM32H7_SPI_CFG2, STM32H7_SPI_CFG2_CPHA },
	.lsb_first = { STM32H7_SPI_CFG2, STM32H7_SPI_CFG2_LSBFRST },
	.cs_high = { STM32H7_SPI_CFG2, STM32H7_SPI_CFG2_SSIOP },
	.br = { STM32H7_SPI_CFG1, STM32H7_SPI_CFG1_MBR,
		STM32H7_SPI_CFG1_MBR_SHIFT },

	.rx = { STM32H7_SPI_RXDR },
	.tx = { STM32H7_SPI_TXDR },
};

static inline void stm32_spi_set_bits(struct stm32_spi *spi,
				      u32 offset, u32 bits)
{
	writel_relaxed(readl_relaxed(spi->base + offset) | bits,
		       spi->base + offset);
}

static inline void stm32_spi_clr_bits(struct stm32_spi *spi,
				      u32 offset, u32 bits)
{
	writel_relaxed(readl_relaxed(spi->base + offset) & ~bits,
		       spi->base + offset);
}

 
static int stm32h7_spi_get_fifo_size(struct stm32_spi *spi)
{
	unsigned long flags;
	u32 count = 0;

	spin_lock_irqsave(&spi->lock, flags);

	stm32_spi_set_bits(spi, STM32H7_SPI_CR1, STM32H7_SPI_CR1_SPE);

	while (readl_relaxed(spi->base + STM32H7_SPI_SR) & STM32H7_SPI_SR_TXP)
		writeb_relaxed(++count, spi->base + STM32H7_SPI_TXDR);

	stm32_spi_clr_bits(spi, STM32H7_SPI_CR1, STM32H7_SPI_CR1_SPE);

	spin_unlock_irqrestore(&spi->lock, flags);

	dev_dbg(spi->dev, "%d x 8-bit fifo size\n", count);

	return count;
}

 
static int stm32f4_spi_get_bpw_mask(struct stm32_spi *spi)
{
	dev_dbg(spi->dev, "8-bit or 16-bit data frame supported\n");
	return SPI_BPW_MASK(8) | SPI_BPW_MASK(16);
}

 
static int stm32h7_spi_get_bpw_mask(struct stm32_spi *spi)
{
	unsigned long flags;
	u32 cfg1, max_bpw;

	spin_lock_irqsave(&spi->lock, flags);

	 
	stm32_spi_set_bits(spi, STM32H7_SPI_CFG1, STM32H7_SPI_CFG1_DSIZE);

	cfg1 = readl_relaxed(spi->base + STM32H7_SPI_CFG1);
	max_bpw = FIELD_GET(STM32H7_SPI_CFG1_DSIZE, cfg1) + 1;

	spin_unlock_irqrestore(&spi->lock, flags);

	dev_dbg(spi->dev, "%d-bit maximum data frame\n", max_bpw);

	return SPI_BPW_RANGE_MASK(4, max_bpw);
}

 
static int stm32_spi_prepare_mbr(struct stm32_spi *spi, u32 speed_hz,
				 u32 min_div, u32 max_div)
{
	u32 div, mbrdiv;

	 
	div = DIV_ROUND_CLOSEST(spi->clk_rate & ~0x1, speed_hz);

	 
	if ((div < min_div) || (div > max_div))
		return -EINVAL;

	 
	if (div & (div - 1))
		mbrdiv = fls(div);
	else
		mbrdiv = fls(div) - 1;

	spi->cur_speed = spi->clk_rate / (1 << mbrdiv);

	spi->cur_half_period = DIV_ROUND_CLOSEST(USEC_PER_SEC, 2 * spi->cur_speed);

	return mbrdiv - 1;
}

 
static u32 stm32h7_spi_prepare_fthlv(struct stm32_spi *spi, u32 xfer_len)
{
	u32 packet, bpw;

	 
	packet = clamp(xfer_len, 1U, spi->fifo_size / 2);

	 
	bpw = DIV_ROUND_UP(spi->cur_bpw, 8);
	return DIV_ROUND_UP(packet, bpw);
}

 
static void stm32f4_spi_write_tx(struct stm32_spi *spi)
{
	if ((spi->tx_len > 0) && (readl_relaxed(spi->base + STM32F4_SPI_SR) &
				  STM32F4_SPI_SR_TXE)) {
		u32 offs = spi->cur_xferlen - spi->tx_len;

		if (spi->cur_bpw == 16) {
			const u16 *tx_buf16 = (const u16 *)(spi->tx_buf + offs);

			writew_relaxed(*tx_buf16, spi->base + STM32F4_SPI_DR);
			spi->tx_len -= sizeof(u16);
		} else {
			const u8 *tx_buf8 = (const u8 *)(spi->tx_buf + offs);

			writeb_relaxed(*tx_buf8, spi->base + STM32F4_SPI_DR);
			spi->tx_len -= sizeof(u8);
		}
	}

	dev_dbg(spi->dev, "%s: %d bytes left\n", __func__, spi->tx_len);
}

 
static void stm32h7_spi_write_txfifo(struct stm32_spi *spi)
{
	while ((spi->tx_len > 0) &&
		       (readl_relaxed(spi->base + STM32H7_SPI_SR) &
			STM32H7_SPI_SR_TXP)) {
		u32 offs = spi->cur_xferlen - spi->tx_len;

		if (spi->tx_len >= sizeof(u32)) {
			const u32 *tx_buf32 = (const u32 *)(spi->tx_buf + offs);

			writel_relaxed(*tx_buf32, spi->base + STM32H7_SPI_TXDR);
			spi->tx_len -= sizeof(u32);
		} else if (spi->tx_len >= sizeof(u16)) {
			const u16 *tx_buf16 = (const u16 *)(spi->tx_buf + offs);

			writew_relaxed(*tx_buf16, spi->base + STM32H7_SPI_TXDR);
			spi->tx_len -= sizeof(u16);
		} else {
			const u8 *tx_buf8 = (const u8 *)(spi->tx_buf + offs);

			writeb_relaxed(*tx_buf8, spi->base + STM32H7_SPI_TXDR);
			spi->tx_len -= sizeof(u8);
		}
	}

	dev_dbg(spi->dev, "%s: %d bytes left\n", __func__, spi->tx_len);
}

 
static void stm32f4_spi_read_rx(struct stm32_spi *spi)
{
	if ((spi->rx_len > 0) && (readl_relaxed(spi->base + STM32F4_SPI_SR) &
				  STM32F4_SPI_SR_RXNE)) {
		u32 offs = spi->cur_xferlen - spi->rx_len;

		if (spi->cur_bpw == 16) {
			u16 *rx_buf16 = (u16 *)(spi->rx_buf + offs);

			*rx_buf16 = readw_relaxed(spi->base + STM32F4_SPI_DR);
			spi->rx_len -= sizeof(u16);
		} else {
			u8 *rx_buf8 = (u8 *)(spi->rx_buf + offs);

			*rx_buf8 = readb_relaxed(spi->base + STM32F4_SPI_DR);
			spi->rx_len -= sizeof(u8);
		}
	}

	dev_dbg(spi->dev, "%s: %d bytes left\n", __func__, spi->rx_len);
}

 
static void stm32h7_spi_read_rxfifo(struct stm32_spi *spi)
{
	u32 sr = readl_relaxed(spi->base + STM32H7_SPI_SR);
	u32 rxplvl = FIELD_GET(STM32H7_SPI_SR_RXPLVL, sr);

	while ((spi->rx_len > 0) &&
	       ((sr & STM32H7_SPI_SR_RXP) ||
		((sr & STM32H7_SPI_SR_EOT) &&
		 ((sr & STM32H7_SPI_SR_RXWNE) || (rxplvl > 0))))) {
		u32 offs = spi->cur_xferlen - spi->rx_len;

		if ((spi->rx_len >= sizeof(u32)) ||
		    (sr & STM32H7_SPI_SR_RXWNE)) {
			u32 *rx_buf32 = (u32 *)(spi->rx_buf + offs);

			*rx_buf32 = readl_relaxed(spi->base + STM32H7_SPI_RXDR);
			spi->rx_len -= sizeof(u32);
		} else if ((spi->rx_len >= sizeof(u16)) ||
			   (!(sr & STM32H7_SPI_SR_RXWNE) &&
			    (rxplvl >= 2 || spi->cur_bpw > 8))) {
			u16 *rx_buf16 = (u16 *)(spi->rx_buf + offs);

			*rx_buf16 = readw_relaxed(spi->base + STM32H7_SPI_RXDR);
			spi->rx_len -= sizeof(u16);
		} else {
			u8 *rx_buf8 = (u8 *)(spi->rx_buf + offs);

			*rx_buf8 = readb_relaxed(spi->base + STM32H7_SPI_RXDR);
			spi->rx_len -= sizeof(u8);
		}

		sr = readl_relaxed(spi->base + STM32H7_SPI_SR);
		rxplvl = FIELD_GET(STM32H7_SPI_SR_RXPLVL, sr);
	}

	dev_dbg(spi->dev, "%s: %d bytes left (sr=%08x)\n",
		__func__, spi->rx_len, sr);
}

 
static void stm32_spi_enable(struct stm32_spi *spi)
{
	dev_dbg(spi->dev, "enable controller\n");

	stm32_spi_set_bits(spi, spi->cfg->regs->en.reg,
			   spi->cfg->regs->en.mask);
}

 
static void stm32f4_spi_disable(struct stm32_spi *spi)
{
	unsigned long flags;
	u32 sr;

	dev_dbg(spi->dev, "disable controller\n");

	spin_lock_irqsave(&spi->lock, flags);

	if (!(readl_relaxed(spi->base + STM32F4_SPI_CR1) &
	      STM32F4_SPI_CR1_SPE)) {
		spin_unlock_irqrestore(&spi->lock, flags);
		return;
	}

	 
	stm32_spi_clr_bits(spi, STM32F4_SPI_CR2, STM32F4_SPI_CR2_TXEIE |
						 STM32F4_SPI_CR2_RXNEIE |
						 STM32F4_SPI_CR2_ERRIE);

	 
	if (readl_relaxed_poll_timeout_atomic(spi->base + STM32F4_SPI_SR,
					      sr, !(sr & STM32F4_SPI_SR_BSY),
					      10, 100000) < 0) {
		dev_warn(spi->dev, "disabling condition timeout\n");
	}

	if (spi->cur_usedma && spi->dma_tx)
		dmaengine_terminate_async(spi->dma_tx);
	if (spi->cur_usedma && spi->dma_rx)
		dmaengine_terminate_async(spi->dma_rx);

	stm32_spi_clr_bits(spi, STM32F4_SPI_CR1, STM32F4_SPI_CR1_SPE);

	stm32_spi_clr_bits(spi, STM32F4_SPI_CR2, STM32F4_SPI_CR2_TXDMAEN |
						 STM32F4_SPI_CR2_RXDMAEN);

	 
	readl_relaxed(spi->base + STM32F4_SPI_DR);
	readl_relaxed(spi->base + STM32F4_SPI_SR);

	spin_unlock_irqrestore(&spi->lock, flags);
}

 
static void stm32h7_spi_disable(struct stm32_spi *spi)
{
	unsigned long flags;
	u32 cr1;

	dev_dbg(spi->dev, "disable controller\n");

	spin_lock_irqsave(&spi->lock, flags);

	cr1 = readl_relaxed(spi->base + STM32H7_SPI_CR1);

	if (!(cr1 & STM32H7_SPI_CR1_SPE)) {
		spin_unlock_irqrestore(&spi->lock, flags);
		return;
	}

	 
	if (spi->cur_half_period)
		udelay(spi->cur_half_period);

	if (spi->cur_usedma && spi->dma_tx)
		dmaengine_terminate_async(spi->dma_tx);
	if (spi->cur_usedma && spi->dma_rx)
		dmaengine_terminate_async(spi->dma_rx);

	stm32_spi_clr_bits(spi, STM32H7_SPI_CR1, STM32H7_SPI_CR1_SPE);

	stm32_spi_clr_bits(spi, STM32H7_SPI_CFG1, STM32H7_SPI_CFG1_TXDMAEN |
						STM32H7_SPI_CFG1_RXDMAEN);

	 
	writel_relaxed(0, spi->base + STM32H7_SPI_IER);
	writel_relaxed(STM32H7_SPI_IFCR_ALL, spi->base + STM32H7_SPI_IFCR);

	spin_unlock_irqrestore(&spi->lock, flags);
}

 
static bool stm32_spi_can_dma(struct spi_controller *ctrl,
			      struct spi_device *spi_dev,
			      struct spi_transfer *transfer)
{
	unsigned int dma_size;
	struct stm32_spi *spi = spi_controller_get_devdata(ctrl);

	if (spi->cfg->has_fifo)
		dma_size = spi->fifo_size;
	else
		dma_size = SPI_DMA_MIN_BYTES;

	dev_dbg(spi->dev, "%s: %s\n", __func__,
		(transfer->len > dma_size) ? "true" : "false");

	return (transfer->len > dma_size);
}

 
static irqreturn_t stm32f4_spi_irq_event(int irq, void *dev_id)
{
	struct spi_controller *ctrl = dev_id;
	struct stm32_spi *spi = spi_controller_get_devdata(ctrl);
	u32 sr, mask = 0;
	bool end = false;

	spin_lock(&spi->lock);

	sr = readl_relaxed(spi->base + STM32F4_SPI_SR);
	 
	sr &= ~STM32F4_SPI_SR_BSY;

	if (!spi->cur_usedma && (spi->cur_comm == SPI_SIMPLEX_TX ||
				 spi->cur_comm == SPI_3WIRE_TX)) {
		 
		sr &= ~(STM32F4_SPI_SR_OVR | STM32F4_SPI_SR_RXNE);
		mask |= STM32F4_SPI_SR_TXE;
	}

	if (!spi->cur_usedma && (spi->cur_comm == SPI_FULL_DUPLEX ||
				spi->cur_comm == SPI_SIMPLEX_RX ||
				spi->cur_comm == SPI_3WIRE_RX)) {
		 
		sr &= ~STM32F4_SPI_SR_TXE;
		mask |= STM32F4_SPI_SR_RXNE | STM32F4_SPI_SR_OVR;
	}

	if (!(sr & mask)) {
		dev_dbg(spi->dev, "spurious IT (sr=0x%08x)\n", sr);
		spin_unlock(&spi->lock);
		return IRQ_NONE;
	}

	if (sr & STM32F4_SPI_SR_OVR) {
		dev_warn(spi->dev, "Overrun: received value discarded\n");

		 
		readl_relaxed(spi->base + STM32F4_SPI_DR);
		readl_relaxed(spi->base + STM32F4_SPI_SR);

		 
		end = true;
		goto end_irq;
	}

	if (sr & STM32F4_SPI_SR_TXE) {
		if (spi->tx_buf)
			stm32f4_spi_write_tx(spi);
		if (spi->tx_len == 0)
			end = true;
	}

	if (sr & STM32F4_SPI_SR_RXNE) {
		stm32f4_spi_read_rx(spi);
		if (spi->rx_len == 0)
			end = true;
		else if (spi->tx_buf) 
			stm32f4_spi_write_tx(spi);
	}

end_irq:
	if (end) {
		 
		stm32_spi_clr_bits(spi, STM32F4_SPI_CR2,
					STM32F4_SPI_CR2_TXEIE |
					STM32F4_SPI_CR2_RXNEIE |
					STM32F4_SPI_CR2_ERRIE);
		spin_unlock(&spi->lock);
		return IRQ_WAKE_THREAD;
	}

	spin_unlock(&spi->lock);
	return IRQ_HANDLED;
}

 
static irqreturn_t stm32f4_spi_irq_thread(int irq, void *dev_id)
{
	struct spi_controller *ctrl = dev_id;
	struct stm32_spi *spi = spi_controller_get_devdata(ctrl);

	spi_finalize_current_transfer(ctrl);
	stm32f4_spi_disable(spi);

	return IRQ_HANDLED;
}

 
static irqreturn_t stm32h7_spi_irq_thread(int irq, void *dev_id)
{
	struct spi_controller *ctrl = dev_id;
	struct stm32_spi *spi = spi_controller_get_devdata(ctrl);
	u32 sr, ier, mask;
	unsigned long flags;
	bool end = false;

	spin_lock_irqsave(&spi->lock, flags);

	sr = readl_relaxed(spi->base + STM32H7_SPI_SR);
	ier = readl_relaxed(spi->base + STM32H7_SPI_IER);

	mask = ier;
	 

	mask |= STM32H7_SPI_SR_SUSP;
	 
	if ((spi->cur_comm == SPI_FULL_DUPLEX) && !spi->cur_usedma)
		mask |= STM32H7_SPI_SR_TXP | STM32H7_SPI_SR_RXP;

	if (!(sr & mask)) {
		dev_warn(spi->dev, "spurious IT (sr=0x%08x, ier=0x%08x)\n",
			 sr, ier);
		spin_unlock_irqrestore(&spi->lock, flags);
		return IRQ_NONE;
	}

	if (sr & STM32H7_SPI_SR_SUSP) {
		static DEFINE_RATELIMIT_STATE(rs,
					      DEFAULT_RATELIMIT_INTERVAL * 10,
					      1);
		ratelimit_set_flags(&rs, RATELIMIT_MSG_ON_RELEASE);
		if (__ratelimit(&rs))
			dev_dbg_ratelimited(spi->dev, "Communication suspended\n");
		if (!spi->cur_usedma && (spi->rx_buf && (spi->rx_len > 0)))
			stm32h7_spi_read_rxfifo(spi);
		 
		if (spi->cur_usedma)
			end = true;
	}

	if (sr & STM32H7_SPI_SR_MODF) {
		dev_warn(spi->dev, "Mode fault: transfer aborted\n");
		end = true;
	}

	if (sr & STM32H7_SPI_SR_OVR) {
		dev_err(spi->dev, "Overrun: RX data lost\n");
		end = true;
	}

	if (sr & STM32H7_SPI_SR_EOT) {
		if (!spi->cur_usedma && (spi->rx_buf && (spi->rx_len > 0)))
			stm32h7_spi_read_rxfifo(spi);
		if (!spi->cur_usedma ||
		    (spi->cur_comm == SPI_SIMPLEX_TX || spi->cur_comm == SPI_3WIRE_TX))
			end = true;
	}

	if (sr & STM32H7_SPI_SR_TXP)
		if (!spi->cur_usedma && (spi->tx_buf && (spi->tx_len > 0)))
			stm32h7_spi_write_txfifo(spi);

	if (sr & STM32H7_SPI_SR_RXP)
		if (!spi->cur_usedma && (spi->rx_buf && (spi->rx_len > 0)))
			stm32h7_spi_read_rxfifo(spi);

	writel_relaxed(sr & mask, spi->base + STM32H7_SPI_IFCR);

	spin_unlock_irqrestore(&spi->lock, flags);

	if (end) {
		stm32h7_spi_disable(spi);
		spi_finalize_current_transfer(ctrl);
	}

	return IRQ_HANDLED;
}

 
static int stm32_spi_prepare_msg(struct spi_controller *ctrl,
				 struct spi_message *msg)
{
	struct stm32_spi *spi = spi_controller_get_devdata(ctrl);
	struct spi_device *spi_dev = msg->spi;
	struct device_node *np = spi_dev->dev.of_node;
	unsigned long flags;
	u32 clrb = 0, setb = 0;

	 
	spi->cur_midi = 0;
	if (np && !of_property_read_u32(np, "st,spi-midi-ns", &spi->cur_midi))
		dev_dbg(spi->dev, "%dns inter-data idleness\n", spi->cur_midi);

	if (spi_dev->mode & SPI_CPOL)
		setb |= spi->cfg->regs->cpol.mask;
	else
		clrb |= spi->cfg->regs->cpol.mask;

	if (spi_dev->mode & SPI_CPHA)
		setb |= spi->cfg->regs->cpha.mask;
	else
		clrb |= spi->cfg->regs->cpha.mask;

	if (spi_dev->mode & SPI_LSB_FIRST)
		setb |= spi->cfg->regs->lsb_first.mask;
	else
		clrb |= spi->cfg->regs->lsb_first.mask;

	if (STM32_SPI_DEVICE_MODE(spi) && spi_dev->mode & SPI_CS_HIGH)
		setb |= spi->cfg->regs->cs_high.mask;
	else
		clrb |= spi->cfg->regs->cs_high.mask;

	dev_dbg(spi->dev, "cpol=%d cpha=%d lsb_first=%d cs_high=%d\n",
		!!(spi_dev->mode & SPI_CPOL),
		!!(spi_dev->mode & SPI_CPHA),
		!!(spi_dev->mode & SPI_LSB_FIRST),
		!!(spi_dev->mode & SPI_CS_HIGH));

	 
	if (spi->cfg->set_number_of_data) {
		int ret;

		ret = spi_split_transfers_maxwords(ctrl, msg,
						   STM32H7_SPI_TSIZE_MAX,
						   GFP_KERNEL | GFP_DMA);
		if (ret)
			return ret;
	}

	spin_lock_irqsave(&spi->lock, flags);

	 
	if (clrb || setb)
		writel_relaxed(
			(readl_relaxed(spi->base + spi->cfg->regs->cpol.reg) &
			 ~clrb) | setb,
			spi->base + spi->cfg->regs->cpol.reg);

	spin_unlock_irqrestore(&spi->lock, flags);

	return 0;
}

 
static void stm32f4_spi_dma_tx_cb(void *data)
{
	struct stm32_spi *spi = data;

	if (spi->cur_comm == SPI_SIMPLEX_TX || spi->cur_comm == SPI_3WIRE_TX) {
		spi_finalize_current_transfer(spi->ctrl);
		stm32f4_spi_disable(spi);
	}
}

 
static void stm32_spi_dma_rx_cb(void *data)
{
	struct stm32_spi *spi = data;

	spi_finalize_current_transfer(spi->ctrl);
	spi->cfg->disable(spi);
}

 
static void stm32_spi_dma_config(struct stm32_spi *spi,
				 struct dma_slave_config *dma_conf,
				 enum dma_transfer_direction dir)
{
	enum dma_slave_buswidth buswidth;
	u32 maxburst;

	if (spi->cur_bpw <= 8)
		buswidth = DMA_SLAVE_BUSWIDTH_1_BYTE;
	else if (spi->cur_bpw <= 16)
		buswidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
	else
		buswidth = DMA_SLAVE_BUSWIDTH_4_BYTES;

	if (spi->cfg->has_fifo) {
		 
		if (spi->cur_fthlv == 2)
			maxburst = 1;
		else
			maxburst = spi->cur_fthlv;
	} else {
		maxburst = 1;
	}

	memset(dma_conf, 0, sizeof(struct dma_slave_config));
	dma_conf->direction = dir;
	if (dma_conf->direction == DMA_DEV_TO_MEM) {  
		dma_conf->src_addr = spi->phys_addr + spi->cfg->regs->rx.reg;
		dma_conf->src_addr_width = buswidth;
		dma_conf->src_maxburst = maxburst;

		dev_dbg(spi->dev, "Rx DMA config buswidth=%d, maxburst=%d\n",
			buswidth, maxburst);
	} else if (dma_conf->direction == DMA_MEM_TO_DEV) {  
		dma_conf->dst_addr = spi->phys_addr + spi->cfg->regs->tx.reg;
		dma_conf->dst_addr_width = buswidth;
		dma_conf->dst_maxburst = maxburst;

		dev_dbg(spi->dev, "Tx DMA config buswidth=%d, maxburst=%d\n",
			buswidth, maxburst);
	}
}

 
static int stm32f4_spi_transfer_one_irq(struct stm32_spi *spi)
{
	unsigned long flags;
	u32 cr2 = 0;

	 
	if (spi->cur_comm == SPI_SIMPLEX_TX || spi->cur_comm == SPI_3WIRE_TX) {
		cr2 |= STM32F4_SPI_CR2_TXEIE;
	} else if (spi->cur_comm == SPI_FULL_DUPLEX ||
				spi->cur_comm == SPI_SIMPLEX_RX ||
				spi->cur_comm == SPI_3WIRE_RX) {
		 
		cr2 |= STM32F4_SPI_CR2_RXNEIE | STM32F4_SPI_CR2_ERRIE;
	} else {
		return -EINVAL;
	}

	spin_lock_irqsave(&spi->lock, flags);

	stm32_spi_set_bits(spi, STM32F4_SPI_CR2, cr2);

	stm32_spi_enable(spi);

	 
	if (spi->tx_buf)
		stm32f4_spi_write_tx(spi);

	spin_unlock_irqrestore(&spi->lock, flags);

	return 1;
}

 
static int stm32h7_spi_transfer_one_irq(struct stm32_spi *spi)
{
	unsigned long flags;
	u32 ier = 0;

	 
	if (spi->tx_buf && spi->rx_buf)	 
		ier |= STM32H7_SPI_IER_DXPIE;
	else if (spi->tx_buf)		 
		ier |= STM32H7_SPI_IER_TXPIE;
	else if (spi->rx_buf)		 
		ier |= STM32H7_SPI_IER_RXPIE;

	 
	ier |= STM32H7_SPI_IER_EOTIE | STM32H7_SPI_IER_TXTFIE |
	       STM32H7_SPI_IER_OVRIE | STM32H7_SPI_IER_MODFIE;

	spin_lock_irqsave(&spi->lock, flags);

	stm32_spi_enable(spi);

	 
	if (spi->tx_buf)
		stm32h7_spi_write_txfifo(spi);

	if (STM32_SPI_MASTER_MODE(spi))
		stm32_spi_set_bits(spi, STM32H7_SPI_CR1, STM32H7_SPI_CR1_CSTART);

	writel_relaxed(ier, spi->base + STM32H7_SPI_IER);

	spin_unlock_irqrestore(&spi->lock, flags);

	return 1;
}

 
static void stm32f4_spi_transfer_one_dma_start(struct stm32_spi *spi)
{
	 
	if (spi->cur_comm == SPI_SIMPLEX_RX || spi->cur_comm == SPI_3WIRE_RX ||
	    spi->cur_comm == SPI_FULL_DUPLEX) {
		 
		stm32_spi_set_bits(spi, STM32F4_SPI_CR2, STM32F4_SPI_CR2_ERRIE);
	}

	stm32_spi_enable(spi);
}

 
static void stm32h7_spi_transfer_one_dma_start(struct stm32_spi *spi)
{
	uint32_t ier = STM32H7_SPI_IER_OVRIE | STM32H7_SPI_IER_MODFIE;

	 
	if (spi->cur_comm == SPI_SIMPLEX_TX || spi->cur_comm == SPI_3WIRE_TX)
		ier |= STM32H7_SPI_IER_EOTIE | STM32H7_SPI_IER_TXTFIE;

	stm32_spi_set_bits(spi, STM32H7_SPI_IER, ier);

	stm32_spi_enable(spi);

	if (STM32_SPI_MASTER_MODE(spi))
		stm32_spi_set_bits(spi, STM32H7_SPI_CR1, STM32H7_SPI_CR1_CSTART);
}

 
static int stm32_spi_transfer_one_dma(struct stm32_spi *spi,
				      struct spi_transfer *xfer)
{
	struct dma_slave_config tx_dma_conf, rx_dma_conf;
	struct dma_async_tx_descriptor *tx_dma_desc, *rx_dma_desc;
	unsigned long flags;

	spin_lock_irqsave(&spi->lock, flags);

	rx_dma_desc = NULL;
	if (spi->rx_buf && spi->dma_rx) {
		stm32_spi_dma_config(spi, &rx_dma_conf, DMA_DEV_TO_MEM);
		dmaengine_slave_config(spi->dma_rx, &rx_dma_conf);

		 
		stm32_spi_set_bits(spi, spi->cfg->regs->dma_rx_en.reg,
				   spi->cfg->regs->dma_rx_en.mask);

		rx_dma_desc = dmaengine_prep_slave_sg(
					spi->dma_rx, xfer->rx_sg.sgl,
					xfer->rx_sg.nents,
					rx_dma_conf.direction,
					DMA_PREP_INTERRUPT);
	}

	tx_dma_desc = NULL;
	if (spi->tx_buf && spi->dma_tx) {
		stm32_spi_dma_config(spi, &tx_dma_conf, DMA_MEM_TO_DEV);
		dmaengine_slave_config(spi->dma_tx, &tx_dma_conf);

		tx_dma_desc = dmaengine_prep_slave_sg(
					spi->dma_tx, xfer->tx_sg.sgl,
					xfer->tx_sg.nents,
					tx_dma_conf.direction,
					DMA_PREP_INTERRUPT);
	}

	if ((spi->tx_buf && spi->dma_tx && !tx_dma_desc) ||
	    (spi->rx_buf && spi->dma_rx && !rx_dma_desc))
		goto dma_desc_error;

	if (spi->cur_comm == SPI_FULL_DUPLEX && (!tx_dma_desc || !rx_dma_desc))
		goto dma_desc_error;

	if (rx_dma_desc) {
		rx_dma_desc->callback = spi->cfg->dma_rx_cb;
		rx_dma_desc->callback_param = spi;

		if (dma_submit_error(dmaengine_submit(rx_dma_desc))) {
			dev_err(spi->dev, "Rx DMA submit failed\n");
			goto dma_desc_error;
		}
		 
		dma_async_issue_pending(spi->dma_rx);
	}

	if (tx_dma_desc) {
		if (spi->cur_comm == SPI_SIMPLEX_TX ||
		    spi->cur_comm == SPI_3WIRE_TX) {
			tx_dma_desc->callback = spi->cfg->dma_tx_cb;
			tx_dma_desc->callback_param = spi;
		}

		if (dma_submit_error(dmaengine_submit(tx_dma_desc))) {
			dev_err(spi->dev, "Tx DMA submit failed\n");
			goto dma_submit_error;
		}
		 
		dma_async_issue_pending(spi->dma_tx);

		 
		stm32_spi_set_bits(spi, spi->cfg->regs->dma_tx_en.reg,
				   spi->cfg->regs->dma_tx_en.mask);
	}

	spi->cfg->transfer_one_dma_start(spi);

	spin_unlock_irqrestore(&spi->lock, flags);

	return 1;

dma_submit_error:
	if (spi->dma_rx)
		dmaengine_terminate_sync(spi->dma_rx);

dma_desc_error:
	stm32_spi_clr_bits(spi, spi->cfg->regs->dma_rx_en.reg,
			   spi->cfg->regs->dma_rx_en.mask);

	spin_unlock_irqrestore(&spi->lock, flags);

	dev_info(spi->dev, "DMA issue: fall back to irq transfer\n");

	spi->cur_usedma = false;
	return spi->cfg->transfer_one_irq(spi);
}

 
static void stm32f4_spi_set_bpw(struct stm32_spi *spi)
{
	if (spi->cur_bpw == 16)
		stm32_spi_set_bits(spi, STM32F4_SPI_CR1, STM32F4_SPI_CR1_DFF);
	else
		stm32_spi_clr_bits(spi, STM32F4_SPI_CR1, STM32F4_SPI_CR1_DFF);
}

 
static void stm32h7_spi_set_bpw(struct stm32_spi *spi)
{
	u32 bpw, fthlv;
	u32 cfg1_clrb = 0, cfg1_setb = 0;

	bpw = spi->cur_bpw - 1;

	cfg1_clrb |= STM32H7_SPI_CFG1_DSIZE;
	cfg1_setb |= FIELD_PREP(STM32H7_SPI_CFG1_DSIZE, bpw);

	spi->cur_fthlv = stm32h7_spi_prepare_fthlv(spi, spi->cur_xferlen);
	fthlv = spi->cur_fthlv - 1;

	cfg1_clrb |= STM32H7_SPI_CFG1_FTHLV;
	cfg1_setb |= FIELD_PREP(STM32H7_SPI_CFG1_FTHLV, fthlv);

	writel_relaxed(
		(readl_relaxed(spi->base + STM32H7_SPI_CFG1) &
		 ~cfg1_clrb) | cfg1_setb,
		spi->base + STM32H7_SPI_CFG1);
}

 
static void stm32_spi_set_mbr(struct stm32_spi *spi, u32 mbrdiv)
{
	u32 clrb = 0, setb = 0;

	clrb |= spi->cfg->regs->br.mask;
	setb |= (mbrdiv << spi->cfg->regs->br.shift) & spi->cfg->regs->br.mask;

	writel_relaxed((readl_relaxed(spi->base + spi->cfg->regs->br.reg) &
			~clrb) | setb,
		       spi->base + spi->cfg->regs->br.reg);
}

 
static unsigned int stm32_spi_communication_type(struct spi_device *spi_dev,
						 struct spi_transfer *transfer)
{
	unsigned int type = SPI_FULL_DUPLEX;

	if (spi_dev->mode & SPI_3WIRE) {  
		 
		if (!transfer->tx_buf)
			type = SPI_3WIRE_RX;
		else
			type = SPI_3WIRE_TX;
	} else {
		if (!transfer->tx_buf)
			type = SPI_SIMPLEX_RX;
		else if (!transfer->rx_buf)
			type = SPI_SIMPLEX_TX;
	}

	return type;
}

 
static int stm32f4_spi_set_mode(struct stm32_spi *spi, unsigned int comm_type)
{
	if (comm_type == SPI_3WIRE_TX || comm_type == SPI_SIMPLEX_TX) {
		stm32_spi_set_bits(spi, STM32F4_SPI_CR1,
					STM32F4_SPI_CR1_BIDIMODE |
					STM32F4_SPI_CR1_BIDIOE);
	} else if (comm_type == SPI_FULL_DUPLEX ||
				comm_type == SPI_SIMPLEX_RX) {
		stm32_spi_clr_bits(spi, STM32F4_SPI_CR1,
					STM32F4_SPI_CR1_BIDIMODE |
					STM32F4_SPI_CR1_BIDIOE);
	} else if (comm_type == SPI_3WIRE_RX) {
		stm32_spi_set_bits(spi, STM32F4_SPI_CR1,
					STM32F4_SPI_CR1_BIDIMODE);
		stm32_spi_clr_bits(spi, STM32F4_SPI_CR1,
					STM32F4_SPI_CR1_BIDIOE);
	} else {
		return -EINVAL;
	}

	return 0;
}

 
static int stm32h7_spi_set_mode(struct stm32_spi *spi, unsigned int comm_type)
{
	u32 mode;
	u32 cfg2_clrb = 0, cfg2_setb = 0;

	if (comm_type == SPI_3WIRE_RX) {
		mode = STM32H7_SPI_HALF_DUPLEX;
		stm32_spi_clr_bits(spi, STM32H7_SPI_CR1, STM32H7_SPI_CR1_HDDIR);
	} else if (comm_type == SPI_3WIRE_TX) {
		mode = STM32H7_SPI_HALF_DUPLEX;
		stm32_spi_set_bits(spi, STM32H7_SPI_CR1, STM32H7_SPI_CR1_HDDIR);
	} else if (comm_type == SPI_SIMPLEX_RX) {
		mode = STM32H7_SPI_SIMPLEX_RX;
	} else if (comm_type == SPI_SIMPLEX_TX) {
		mode = STM32H7_SPI_SIMPLEX_TX;
	} else {
		mode = STM32H7_SPI_FULL_DUPLEX;
	}

	cfg2_clrb |= STM32H7_SPI_CFG2_COMM;
	cfg2_setb |= FIELD_PREP(STM32H7_SPI_CFG2_COMM, mode);

	writel_relaxed(
		(readl_relaxed(spi->base + STM32H7_SPI_CFG2) &
		 ~cfg2_clrb) | cfg2_setb,
		spi->base + STM32H7_SPI_CFG2);

	return 0;
}

 
static void stm32h7_spi_data_idleness(struct stm32_spi *spi, u32 len)
{
	u32 cfg2_clrb = 0, cfg2_setb = 0;

	cfg2_clrb |= STM32H7_SPI_CFG2_MIDI;
	if ((len > 1) && (spi->cur_midi > 0)) {
		u32 sck_period_ns = DIV_ROUND_UP(NSEC_PER_SEC, spi->cur_speed);
		u32 midi = min_t(u32,
				 DIV_ROUND_UP(spi->cur_midi, sck_period_ns),
				 FIELD_GET(STM32H7_SPI_CFG2_MIDI,
				 STM32H7_SPI_CFG2_MIDI));


		dev_dbg(spi->dev, "period=%dns, midi=%d(=%dns)\n",
			sck_period_ns, midi, midi * sck_period_ns);
		cfg2_setb |= FIELD_PREP(STM32H7_SPI_CFG2_MIDI, midi);
	}

	writel_relaxed((readl_relaxed(spi->base + STM32H7_SPI_CFG2) &
			~cfg2_clrb) | cfg2_setb,
		       spi->base + STM32H7_SPI_CFG2);
}

 
static int stm32h7_spi_number_of_data(struct stm32_spi *spi, u32 nb_words)
{
	if (nb_words <= STM32H7_SPI_TSIZE_MAX) {
		writel_relaxed(FIELD_PREP(STM32H7_SPI_CR2_TSIZE, nb_words),
			       spi->base + STM32H7_SPI_CR2);
	} else {
		return -EMSGSIZE;
	}

	return 0;
}

 
static int stm32_spi_transfer_one_setup(struct stm32_spi *spi,
					struct spi_device *spi_dev,
					struct spi_transfer *transfer)
{
	unsigned long flags;
	unsigned int comm_type;
	int nb_words, ret = 0;
	int mbr;

	spin_lock_irqsave(&spi->lock, flags);

	spi->cur_xferlen = transfer->len;

	spi->cur_bpw = transfer->bits_per_word;
	spi->cfg->set_bpw(spi);

	 
	if (STM32_SPI_MASTER_MODE(spi)) {
		mbr = stm32_spi_prepare_mbr(spi, transfer->speed_hz,
					    spi->cfg->baud_rate_div_min,
					    spi->cfg->baud_rate_div_max);
		if (mbr < 0) {
			ret = mbr;
			goto out;
		}

		transfer->speed_hz = spi->cur_speed;
		stm32_spi_set_mbr(spi, mbr);
	}

	comm_type = stm32_spi_communication_type(spi_dev, transfer);
	ret = spi->cfg->set_mode(spi, comm_type);
	if (ret < 0)
		goto out;

	spi->cur_comm = comm_type;

	if (STM32_SPI_MASTER_MODE(spi) && spi->cfg->set_data_idleness)
		spi->cfg->set_data_idleness(spi, transfer->len);

	if (spi->cur_bpw <= 8)
		nb_words = transfer->len;
	else if (spi->cur_bpw <= 16)
		nb_words = DIV_ROUND_UP(transfer->len * 8, 16);
	else
		nb_words = DIV_ROUND_UP(transfer->len * 8, 32);

	if (spi->cfg->set_number_of_data) {
		ret = spi->cfg->set_number_of_data(spi, nb_words);
		if (ret < 0)
			goto out;
	}

	dev_dbg(spi->dev, "transfer communication mode set to %d\n",
		spi->cur_comm);
	dev_dbg(spi->dev,
		"data frame of %d-bit, data packet of %d data frames\n",
		spi->cur_bpw, spi->cur_fthlv);
	if (STM32_SPI_MASTER_MODE(spi))
		dev_dbg(spi->dev, "speed set to %dHz\n", spi->cur_speed);
	dev_dbg(spi->dev, "transfer of %d bytes (%d data frames)\n",
		spi->cur_xferlen, nb_words);
	dev_dbg(spi->dev, "dma %s\n",
		(spi->cur_usedma) ? "enabled" : "disabled");

out:
	spin_unlock_irqrestore(&spi->lock, flags);

	return ret;
}

 
static int stm32_spi_transfer_one(struct spi_controller *ctrl,
				  struct spi_device *spi_dev,
				  struct spi_transfer *transfer)
{
	struct stm32_spi *spi = spi_controller_get_devdata(ctrl);
	int ret;

	spi->tx_buf = transfer->tx_buf;
	spi->rx_buf = transfer->rx_buf;
	spi->tx_len = spi->tx_buf ? transfer->len : 0;
	spi->rx_len = spi->rx_buf ? transfer->len : 0;

	spi->cur_usedma = (ctrl->can_dma &&
			   ctrl->can_dma(ctrl, spi_dev, transfer));

	ret = stm32_spi_transfer_one_setup(spi, spi_dev, transfer);
	if (ret) {
		dev_err(spi->dev, "SPI transfer setup failed\n");
		return ret;
	}

	if (spi->cur_usedma)
		return stm32_spi_transfer_one_dma(spi, transfer);
	else
		return spi->cfg->transfer_one_irq(spi);
}

 
static int stm32_spi_unprepare_msg(struct spi_controller *ctrl,
				   struct spi_message *msg)
{
	struct stm32_spi *spi = spi_controller_get_devdata(ctrl);

	spi->cfg->disable(spi);

	return 0;
}

 
static int stm32f4_spi_config(struct stm32_spi *spi)
{
	unsigned long flags;

	spin_lock_irqsave(&spi->lock, flags);

	 
	stm32_spi_clr_bits(spi, STM32F4_SPI_I2SCFGR,
			   STM32F4_SPI_I2SCFGR_I2SMOD);

	 
	stm32_spi_set_bits(spi, STM32F4_SPI_CR1, STM32F4_SPI_CR1_SSI |
						 STM32F4_SPI_CR1_BIDIOE |
						 STM32F4_SPI_CR1_MSTR |
						 STM32F4_SPI_CR1_SSM);

	spin_unlock_irqrestore(&spi->lock, flags);

	return 0;
}

 
static int stm32h7_spi_config(struct stm32_spi *spi)
{
	unsigned long flags;
	u32 cr1 = 0, cfg2 = 0;

	spin_lock_irqsave(&spi->lock, flags);

	 
	stm32_spi_clr_bits(spi, STM32H7_SPI_I2SCFGR,
			   STM32H7_SPI_I2SCFGR_I2SMOD);

	if (STM32_SPI_DEVICE_MODE(spi)) {
		 
		cfg2 &= ~STM32H7_SPI_CFG2_SSM;
	} else {
		 
		cr1 |= STM32H7_SPI_CR1_HDDIR | STM32H7_SPI_CR1_MASRX | STM32H7_SPI_CR1_SSI;

		 
		cfg2 |= STM32H7_SPI_CFG2_MASTER | STM32H7_SPI_CFG2_SSM | STM32H7_SPI_CFG2_AFCNTR;
	}

	stm32_spi_set_bits(spi, STM32H7_SPI_CR1, cr1);
	stm32_spi_set_bits(spi, STM32H7_SPI_CFG2, cfg2);

	spin_unlock_irqrestore(&spi->lock, flags);

	return 0;
}

static const struct stm32_spi_cfg stm32f4_spi_cfg = {
	.regs = &stm32f4_spi_regspec,
	.get_bpw_mask = stm32f4_spi_get_bpw_mask,
	.disable = stm32f4_spi_disable,
	.config = stm32f4_spi_config,
	.set_bpw = stm32f4_spi_set_bpw,
	.set_mode = stm32f4_spi_set_mode,
	.transfer_one_dma_start = stm32f4_spi_transfer_one_dma_start,
	.dma_tx_cb = stm32f4_spi_dma_tx_cb,
	.dma_rx_cb = stm32_spi_dma_rx_cb,
	.transfer_one_irq = stm32f4_spi_transfer_one_irq,
	.irq_handler_event = stm32f4_spi_irq_event,
	.irq_handler_thread = stm32f4_spi_irq_thread,
	.baud_rate_div_min = STM32F4_SPI_BR_DIV_MIN,
	.baud_rate_div_max = STM32F4_SPI_BR_DIV_MAX,
	.has_fifo = false,
	.has_device_mode = false,
	.flags = SPI_CONTROLLER_MUST_TX,
};

static const struct stm32_spi_cfg stm32h7_spi_cfg = {
	.regs = &stm32h7_spi_regspec,
	.get_fifo_size = stm32h7_spi_get_fifo_size,
	.get_bpw_mask = stm32h7_spi_get_bpw_mask,
	.disable = stm32h7_spi_disable,
	.config = stm32h7_spi_config,
	.set_bpw = stm32h7_spi_set_bpw,
	.set_mode = stm32h7_spi_set_mode,
	.set_data_idleness = stm32h7_spi_data_idleness,
	.set_number_of_data = stm32h7_spi_number_of_data,
	.transfer_one_dma_start = stm32h7_spi_transfer_one_dma_start,
	.dma_rx_cb = stm32_spi_dma_rx_cb,
	 
	.transfer_one_irq = stm32h7_spi_transfer_one_irq,
	.irq_handler_thread = stm32h7_spi_irq_thread,
	.baud_rate_div_min = STM32H7_SPI_MBR_DIV_MIN,
	.baud_rate_div_max = STM32H7_SPI_MBR_DIV_MAX,
	.has_fifo = true,
	.has_device_mode = true,
};

static const struct of_device_id stm32_spi_of_match[] = {
	{ .compatible = "st,stm32h7-spi", .data = (void *)&stm32h7_spi_cfg },
	{ .compatible = "st,stm32f4-spi", .data = (void *)&stm32f4_spi_cfg },
	{},
};
MODULE_DEVICE_TABLE(of, stm32_spi_of_match);

static int stm32h7_spi_device_abort(struct spi_controller *ctrl)
{
	spi_finalize_current_transfer(ctrl);
	return 0;
}

static int stm32_spi_probe(struct platform_device *pdev)
{
	struct spi_controller *ctrl;
	struct stm32_spi *spi;
	struct resource *res;
	struct reset_control *rst;
	struct device_node *np = pdev->dev.of_node;
	bool device_mode;
	int ret;
	const struct stm32_spi_cfg *cfg = of_device_get_match_data(&pdev->dev);

	device_mode = of_property_read_bool(np, "spi-slave");
	if (!cfg->has_device_mode && device_mode) {
		dev_err(&pdev->dev, "spi-slave not supported\n");
		return -EPERM;
	}

	if (device_mode)
		ctrl = devm_spi_alloc_slave(&pdev->dev, sizeof(struct stm32_spi));
	else
		ctrl = devm_spi_alloc_master(&pdev->dev, sizeof(struct stm32_spi));
	if (!ctrl) {
		dev_err(&pdev->dev, "spi controller allocation failed\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, ctrl);

	spi = spi_controller_get_devdata(ctrl);
	spi->dev = &pdev->dev;
	spi->ctrl = ctrl;
	spi->device_mode = device_mode;
	spin_lock_init(&spi->lock);

	spi->cfg = cfg;

	spi->base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(spi->base))
		return PTR_ERR(spi->base);

	spi->phys_addr = (dma_addr_t)res->start;

	spi->irq = platform_get_irq(pdev, 0);
	if (spi->irq <= 0)
		return spi->irq;

	ret = devm_request_threaded_irq(&pdev->dev, spi->irq,
					spi->cfg->irq_handler_event,
					spi->cfg->irq_handler_thread,
					IRQF_ONESHOT, pdev->name, ctrl);
	if (ret) {
		dev_err(&pdev->dev, "irq%d request failed: %d\n", spi->irq,
			ret);
		return ret;
	}

	spi->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(spi->clk)) {
		ret = PTR_ERR(spi->clk);
		dev_err(&pdev->dev, "clk get failed: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(spi->clk);
	if (ret) {
		dev_err(&pdev->dev, "clk enable failed: %d\n", ret);
		return ret;
	}
	spi->clk_rate = clk_get_rate(spi->clk);
	if (!spi->clk_rate) {
		dev_err(&pdev->dev, "clk rate = 0\n");
		ret = -EINVAL;
		goto err_clk_disable;
	}

	rst = devm_reset_control_get_optional_exclusive(&pdev->dev, NULL);
	if (rst) {
		if (IS_ERR(rst)) {
			ret = dev_err_probe(&pdev->dev, PTR_ERR(rst),
					    "failed to get reset\n");
			goto err_clk_disable;
		}

		reset_control_assert(rst);
		udelay(2);
		reset_control_deassert(rst);
	}

	if (spi->cfg->has_fifo)
		spi->fifo_size = spi->cfg->get_fifo_size(spi);

	ret = spi->cfg->config(spi);
	if (ret) {
		dev_err(&pdev->dev, "controller configuration failed: %d\n",
			ret);
		goto err_clk_disable;
	}

	ctrl->dev.of_node = pdev->dev.of_node;
	ctrl->auto_runtime_pm = true;
	ctrl->bus_num = pdev->id;
	ctrl->mode_bits = SPI_CPHA | SPI_CPOL | SPI_CS_HIGH | SPI_LSB_FIRST |
			  SPI_3WIRE;
	ctrl->bits_per_word_mask = spi->cfg->get_bpw_mask(spi);
	ctrl->max_speed_hz = spi->clk_rate / spi->cfg->baud_rate_div_min;
	ctrl->min_speed_hz = spi->clk_rate / spi->cfg->baud_rate_div_max;
	ctrl->use_gpio_descriptors = true;
	ctrl->prepare_message = stm32_spi_prepare_msg;
	ctrl->transfer_one = stm32_spi_transfer_one;
	ctrl->unprepare_message = stm32_spi_unprepare_msg;
	ctrl->flags = spi->cfg->flags;
	if (STM32_SPI_DEVICE_MODE(spi))
		ctrl->slave_abort = stm32h7_spi_device_abort;

	spi->dma_tx = dma_request_chan(spi->dev, "tx");
	if (IS_ERR(spi->dma_tx)) {
		ret = PTR_ERR(spi->dma_tx);
		spi->dma_tx = NULL;
		if (ret == -EPROBE_DEFER)
			goto err_clk_disable;

		dev_warn(&pdev->dev, "failed to request tx dma channel\n");
	} else {
		ctrl->dma_tx = spi->dma_tx;
	}

	spi->dma_rx = dma_request_chan(spi->dev, "rx");
	if (IS_ERR(spi->dma_rx)) {
		ret = PTR_ERR(spi->dma_rx);
		spi->dma_rx = NULL;
		if (ret == -EPROBE_DEFER)
			goto err_dma_release;

		dev_warn(&pdev->dev, "failed to request rx dma channel\n");
	} else {
		ctrl->dma_rx = spi->dma_rx;
	}

	if (spi->dma_tx || spi->dma_rx)
		ctrl->can_dma = stm32_spi_can_dma;

	pm_runtime_set_autosuspend_delay(&pdev->dev,
					 STM32_SPI_AUTOSUSPEND_DELAY);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ret = spi_register_controller(ctrl);
	if (ret) {
		dev_err(&pdev->dev, "spi controller registration failed: %d\n",
			ret);
		goto err_pm_disable;
	}

	pm_runtime_mark_last_busy(&pdev->dev);
	pm_runtime_put_autosuspend(&pdev->dev);

	dev_info(&pdev->dev, "driver initialized (%s mode)\n",
		 STM32_SPI_MASTER_MODE(spi) ? "master" : "device");

	return 0;

err_pm_disable:
	pm_runtime_disable(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_dont_use_autosuspend(&pdev->dev);
err_dma_release:
	if (spi->dma_tx)
		dma_release_channel(spi->dma_tx);
	if (spi->dma_rx)
		dma_release_channel(spi->dma_rx);
err_clk_disable:
	clk_disable_unprepare(spi->clk);

	return ret;
}

static void stm32_spi_remove(struct platform_device *pdev)
{
	struct spi_controller *ctrl = platform_get_drvdata(pdev);
	struct stm32_spi *spi = spi_controller_get_devdata(ctrl);

	pm_runtime_get_sync(&pdev->dev);

	spi_unregister_controller(ctrl);
	spi->cfg->disable(spi);

	pm_runtime_disable(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_dont_use_autosuspend(&pdev->dev);

	if (ctrl->dma_tx)
		dma_release_channel(ctrl->dma_tx);
	if (ctrl->dma_rx)
		dma_release_channel(ctrl->dma_rx);

	clk_disable_unprepare(spi->clk);


	pinctrl_pm_select_sleep_state(&pdev->dev);
}

static int __maybe_unused stm32_spi_runtime_suspend(struct device *dev)
{
	struct spi_controller *ctrl = dev_get_drvdata(dev);
	struct stm32_spi *spi = spi_controller_get_devdata(ctrl);

	clk_disable_unprepare(spi->clk);

	return pinctrl_pm_select_sleep_state(dev);
}

static int __maybe_unused stm32_spi_runtime_resume(struct device *dev)
{
	struct spi_controller *ctrl = dev_get_drvdata(dev);
	struct stm32_spi *spi = spi_controller_get_devdata(ctrl);
	int ret;

	ret = pinctrl_pm_select_default_state(dev);
	if (ret)
		return ret;

	return clk_prepare_enable(spi->clk);
}

static int __maybe_unused stm32_spi_suspend(struct device *dev)
{
	struct spi_controller *ctrl = dev_get_drvdata(dev);
	int ret;

	ret = spi_controller_suspend(ctrl);
	if (ret)
		return ret;

	return pm_runtime_force_suspend(dev);
}

static int __maybe_unused stm32_spi_resume(struct device *dev)
{
	struct spi_controller *ctrl = dev_get_drvdata(dev);
	struct stm32_spi *spi = spi_controller_get_devdata(ctrl);
	int ret;

	ret = pm_runtime_force_resume(dev);
	if (ret)
		return ret;

	ret = spi_controller_resume(ctrl);
	if (ret) {
		clk_disable_unprepare(spi->clk);
		return ret;
	}

	ret = pm_runtime_resume_and_get(dev);
	if (ret < 0) {
		dev_err(dev, "Unable to power device:%d\n", ret);
		return ret;
	}

	spi->cfg->config(spi);

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;
}

static const struct dev_pm_ops stm32_spi_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(stm32_spi_suspend, stm32_spi_resume)
	SET_RUNTIME_PM_OPS(stm32_spi_runtime_suspend,
			   stm32_spi_runtime_resume, NULL)
};

static struct platform_driver stm32_spi_driver = {
	.probe = stm32_spi_probe,
	.remove_new = stm32_spi_remove,
	.driver = {
		.name = DRIVER_NAME,
		.pm = &stm32_spi_pm_ops,
		.of_match_table = stm32_spi_of_match,
	},
};

module_platform_driver(stm32_spi_driver);

MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_DESCRIPTION("STMicroelectronics STM32 SPI Controller driver");
MODULE_AUTHOR("Amelie Delaunay <amelie.delaunay@st.com>");
MODULE_LICENSE("GPL v2");
