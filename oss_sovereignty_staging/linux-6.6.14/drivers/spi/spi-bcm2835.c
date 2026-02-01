
 

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>  
#include <linux/gpio/driver.h>  
#include <linux/of_irq.h>
#include <linux/spi/spi.h>

 
#define BCM2835_SPI_CS			0x00
#define BCM2835_SPI_FIFO		0x04
#define BCM2835_SPI_CLK			0x08
#define BCM2835_SPI_DLEN		0x0c
#define BCM2835_SPI_LTOH		0x10
#define BCM2835_SPI_DC			0x14

 
#define BCM2835_SPI_CS_LEN_LONG		0x02000000
#define BCM2835_SPI_CS_DMA_LEN		0x01000000
#define BCM2835_SPI_CS_CSPOL2		0x00800000
#define BCM2835_SPI_CS_CSPOL1		0x00400000
#define BCM2835_SPI_CS_CSPOL0		0x00200000
#define BCM2835_SPI_CS_RXF		0x00100000
#define BCM2835_SPI_CS_RXR		0x00080000
#define BCM2835_SPI_CS_TXD		0x00040000
#define BCM2835_SPI_CS_RXD		0x00020000
#define BCM2835_SPI_CS_DONE		0x00010000
#define BCM2835_SPI_CS_LEN		0x00002000
#define BCM2835_SPI_CS_REN		0x00001000
#define BCM2835_SPI_CS_ADCS		0x00000800
#define BCM2835_SPI_CS_INTR		0x00000400
#define BCM2835_SPI_CS_INTD		0x00000200
#define BCM2835_SPI_CS_DMAEN		0x00000100
#define BCM2835_SPI_CS_TA		0x00000080
#define BCM2835_SPI_CS_CSPOL		0x00000040
#define BCM2835_SPI_CS_CLEAR_RX		0x00000020
#define BCM2835_SPI_CS_CLEAR_TX		0x00000010
#define BCM2835_SPI_CS_CPOL		0x00000008
#define BCM2835_SPI_CS_CPHA		0x00000004
#define BCM2835_SPI_CS_CS_10		0x00000002
#define BCM2835_SPI_CS_CS_01		0x00000001

#define BCM2835_SPI_FIFO_SIZE		64
#define BCM2835_SPI_FIFO_SIZE_3_4	48
#define BCM2835_SPI_DMA_MIN_LENGTH	96
#define BCM2835_SPI_MODE_BITS	(SPI_CPOL | SPI_CPHA | SPI_CS_HIGH \
				| SPI_NO_CS | SPI_3WIRE)

#define DRV_NAME	"spi-bcm2835"

 
static unsigned int polling_limit_us = 30;
module_param(polling_limit_us, uint, 0664);
MODULE_PARM_DESC(polling_limit_us,
		 "time in us to run a transfer in polling mode\n");

 
struct bcm2835_spi {
	void __iomem *regs;
	struct clk *clk;
	unsigned long clk_hz;
	int irq;
	struct spi_transfer *tfr;
	struct spi_controller *ctlr;
	const u8 *tx_buf;
	u8 *rx_buf;
	int tx_len;
	int rx_len;
	int tx_prologue;
	int rx_prologue;
	unsigned int tx_spillover;

	struct dentry *debugfs_dir;
	u64 count_transfer_polling;
	u64 count_transfer_irq;
	u64 count_transfer_irq_after_polling;
	u64 count_transfer_dma;

	struct bcm2835_spidev *target;
	unsigned int tx_dma_active;
	unsigned int rx_dma_active;
	struct dma_async_tx_descriptor *fill_tx_desc;
	dma_addr_t fill_tx_addr;
};

 
struct bcm2835_spidev {
	u32 prepare_cs;
	struct dma_async_tx_descriptor *clear_rx_desc;
	dma_addr_t clear_rx_addr;
	u32 clear_rx_cs ____cacheline_aligned;
};

#if defined(CONFIG_DEBUG_FS)
static void bcm2835_debugfs_create(struct bcm2835_spi *bs,
				   const char *dname)
{
	char name[64];
	struct dentry *dir;

	 
	snprintf(name, sizeof(name), "spi-bcm2835-%s", dname);

	 
	dir = debugfs_create_dir(name, NULL);
	bs->debugfs_dir = dir;

	 
	debugfs_create_u64("count_transfer_polling", 0444, dir,
			   &bs->count_transfer_polling);
	debugfs_create_u64("count_transfer_irq", 0444, dir,
			   &bs->count_transfer_irq);
	debugfs_create_u64("count_transfer_irq_after_polling", 0444, dir,
			   &bs->count_transfer_irq_after_polling);
	debugfs_create_u64("count_transfer_dma", 0444, dir,
			   &bs->count_transfer_dma);
}

static void bcm2835_debugfs_remove(struct bcm2835_spi *bs)
{
	debugfs_remove_recursive(bs->debugfs_dir);
	bs->debugfs_dir = NULL;
}
#else
static void bcm2835_debugfs_create(struct bcm2835_spi *bs,
				   const char *dname)
{
}

static void bcm2835_debugfs_remove(struct bcm2835_spi *bs)
{
}
#endif  

static inline u32 bcm2835_rd(struct bcm2835_spi *bs, unsigned int reg)
{
	return readl(bs->regs + reg);
}

static inline void bcm2835_wr(struct bcm2835_spi *bs, unsigned int reg, u32 val)
{
	writel(val, bs->regs + reg);
}

static inline void bcm2835_rd_fifo(struct bcm2835_spi *bs)
{
	u8 byte;

	while ((bs->rx_len) &&
	       (bcm2835_rd(bs, BCM2835_SPI_CS) & BCM2835_SPI_CS_RXD)) {
		byte = bcm2835_rd(bs, BCM2835_SPI_FIFO);
		if (bs->rx_buf)
			*bs->rx_buf++ = byte;
		bs->rx_len--;
	}
}

static inline void bcm2835_wr_fifo(struct bcm2835_spi *bs)
{
	u8 byte;

	while ((bs->tx_len) &&
	       (bcm2835_rd(bs, BCM2835_SPI_CS) & BCM2835_SPI_CS_TXD)) {
		byte = bs->tx_buf ? *bs->tx_buf++ : 0;
		bcm2835_wr(bs, BCM2835_SPI_FIFO, byte);
		bs->tx_len--;
	}
}

 
static inline void bcm2835_rd_fifo_count(struct bcm2835_spi *bs, int count)
{
	u32 val;
	int len;

	bs->rx_len -= count;

	do {
		val = bcm2835_rd(bs, BCM2835_SPI_FIFO);
		len = min(count, 4);
		memcpy(bs->rx_buf, &val, len);
		bs->rx_buf += len;
		count -= 4;
	} while (count > 0);
}

 
static inline void bcm2835_wr_fifo_count(struct bcm2835_spi *bs, int count)
{
	u32 val;
	int len;

	bs->tx_len -= count;

	do {
		if (bs->tx_buf) {
			len = min(count, 4);
			memcpy(&val, bs->tx_buf, len);
			bs->tx_buf += len;
		} else {
			val = 0;
		}
		bcm2835_wr(bs, BCM2835_SPI_FIFO, val);
		count -= 4;
	} while (count > 0);
}

 
static inline void bcm2835_wait_tx_fifo_empty(struct bcm2835_spi *bs)
{
	while (!(bcm2835_rd(bs, BCM2835_SPI_CS) & BCM2835_SPI_CS_DONE))
		cpu_relax();
}

 
static inline void bcm2835_rd_fifo_blind(struct bcm2835_spi *bs, int count)
{
	u8 val;

	count = min(count, bs->rx_len);
	bs->rx_len -= count;

	do {
		val = bcm2835_rd(bs, BCM2835_SPI_FIFO);
		if (bs->rx_buf)
			*bs->rx_buf++ = val;
	} while (--count);
}

 
static inline void bcm2835_wr_fifo_blind(struct bcm2835_spi *bs, int count)
{
	u8 val;

	count = min(count, bs->tx_len);
	bs->tx_len -= count;

	do {
		val = bs->tx_buf ? *bs->tx_buf++ : 0;
		bcm2835_wr(bs, BCM2835_SPI_FIFO, val);
	} while (--count);
}

static void bcm2835_spi_reset_hw(struct bcm2835_spi *bs)
{
	u32 cs = bcm2835_rd(bs, BCM2835_SPI_CS);

	 
	cs &= ~(BCM2835_SPI_CS_INTR |
		BCM2835_SPI_CS_INTD |
		BCM2835_SPI_CS_DMAEN |
		BCM2835_SPI_CS_TA);
	 
	cs |= BCM2835_SPI_CS_DONE;
	 
	cs |= BCM2835_SPI_CS_CLEAR_RX | BCM2835_SPI_CS_CLEAR_TX;

	 
	bcm2835_wr(bs, BCM2835_SPI_CS, cs);
	 
	bcm2835_wr(bs, BCM2835_SPI_DLEN, 0);
}

static irqreturn_t bcm2835_spi_interrupt(int irq, void *dev_id)
{
	struct bcm2835_spi *bs = dev_id;
	u32 cs = bcm2835_rd(bs, BCM2835_SPI_CS);

	 
	if (!(cs & BCM2835_SPI_CS_INTR))
		return IRQ_NONE;

	 
	if (cs & BCM2835_SPI_CS_RXF)
		bcm2835_rd_fifo_blind(bs, BCM2835_SPI_FIFO_SIZE);
	else if (cs & BCM2835_SPI_CS_RXR)
		bcm2835_rd_fifo_blind(bs, BCM2835_SPI_FIFO_SIZE_3_4);

	if (bs->tx_len && cs & BCM2835_SPI_CS_DONE)
		bcm2835_wr_fifo_blind(bs, BCM2835_SPI_FIFO_SIZE);

	 
	bcm2835_rd_fifo(bs);
	 
	bcm2835_wr_fifo(bs);

	if (!bs->rx_len) {
		 
		bcm2835_spi_reset_hw(bs);
		 
		spi_finalize_current_transfer(bs->ctlr);
	}

	return IRQ_HANDLED;
}

static int bcm2835_spi_transfer_one_irq(struct spi_controller *ctlr,
					struct spi_device *spi,
					struct spi_transfer *tfr,
					u32 cs, bool fifo_empty)
{
	struct bcm2835_spi *bs = spi_controller_get_devdata(ctlr);

	 
	bs->count_transfer_irq++;

	 
	bcm2835_wr(bs, BCM2835_SPI_CS, cs | BCM2835_SPI_CS_TA);

	 
	if (fifo_empty)
		bcm2835_wr_fifo_blind(bs, BCM2835_SPI_FIFO_SIZE);
	bcm2835_wr_fifo(bs);

	 
	cs |= BCM2835_SPI_CS_INTR | BCM2835_SPI_CS_INTD | BCM2835_SPI_CS_TA;
	bcm2835_wr(bs, BCM2835_SPI_CS, cs);

	 
	return 1;
}

 
static void bcm2835_spi_transfer_prologue(struct spi_controller *ctlr,
					  struct spi_transfer *tfr,
					  struct bcm2835_spi *bs,
					  u32 cs)
{
	int tx_remaining;

	bs->tfr		 = tfr;
	bs->tx_prologue  = 0;
	bs->rx_prologue  = 0;
	bs->tx_spillover = false;

	if (bs->tx_buf && !sg_is_last(&tfr->tx_sg.sgl[0]))
		bs->tx_prologue = sg_dma_len(&tfr->tx_sg.sgl[0]) & 3;

	if (bs->rx_buf && !sg_is_last(&tfr->rx_sg.sgl[0])) {
		bs->rx_prologue = sg_dma_len(&tfr->rx_sg.sgl[0]) & 3;

		if (bs->rx_prologue > bs->tx_prologue) {
			if (!bs->tx_buf || sg_is_last(&tfr->tx_sg.sgl[0])) {
				bs->tx_prologue  = bs->rx_prologue;
			} else {
				bs->tx_prologue += 4;
				bs->tx_spillover =
					!(sg_dma_len(&tfr->tx_sg.sgl[0]) & ~3);
			}
		}
	}

	 
	if (!bs->tx_prologue)
		return;

	 
	if (bs->rx_prologue) {
		bcm2835_wr(bs, BCM2835_SPI_DLEN, bs->rx_prologue);
		bcm2835_wr(bs, BCM2835_SPI_CS, cs | BCM2835_SPI_CS_TA
						  | BCM2835_SPI_CS_DMAEN);
		bcm2835_wr_fifo_count(bs, bs->rx_prologue);
		bcm2835_wait_tx_fifo_empty(bs);
		bcm2835_rd_fifo_count(bs, bs->rx_prologue);
		bcm2835_wr(bs, BCM2835_SPI_CS, cs | BCM2835_SPI_CS_CLEAR_RX
						  | BCM2835_SPI_CS_CLEAR_TX
						  | BCM2835_SPI_CS_DONE);

		dma_sync_single_for_device(ctlr->dma_rx->device->dev,
					   sg_dma_address(&tfr->rx_sg.sgl[0]),
					   bs->rx_prologue, DMA_FROM_DEVICE);

		sg_dma_address(&tfr->rx_sg.sgl[0]) += bs->rx_prologue;
		sg_dma_len(&tfr->rx_sg.sgl[0])     -= bs->rx_prologue;
	}

	if (!bs->tx_buf)
		return;

	 
	tx_remaining = bs->tx_prologue - bs->rx_prologue;
	if (tx_remaining) {
		bcm2835_wr(bs, BCM2835_SPI_DLEN, tx_remaining);
		bcm2835_wr(bs, BCM2835_SPI_CS, cs | BCM2835_SPI_CS_TA
						  | BCM2835_SPI_CS_DMAEN);
		bcm2835_wr_fifo_count(bs, tx_remaining);
		bcm2835_wait_tx_fifo_empty(bs);
		bcm2835_wr(bs, BCM2835_SPI_CS, cs | BCM2835_SPI_CS_CLEAR_TX
						  | BCM2835_SPI_CS_DONE);
	}

	if (likely(!bs->tx_spillover)) {
		sg_dma_address(&tfr->tx_sg.sgl[0]) += bs->tx_prologue;
		sg_dma_len(&tfr->tx_sg.sgl[0])     -= bs->tx_prologue;
	} else {
		sg_dma_len(&tfr->tx_sg.sgl[0])      = 0;
		sg_dma_address(&tfr->tx_sg.sgl[1]) += 4;
		sg_dma_len(&tfr->tx_sg.sgl[1])     -= 4;
	}
}

 
static void bcm2835_spi_undo_prologue(struct bcm2835_spi *bs)
{
	struct spi_transfer *tfr = bs->tfr;

	if (!bs->tx_prologue)
		return;

	if (bs->rx_prologue) {
		sg_dma_address(&tfr->rx_sg.sgl[0]) -= bs->rx_prologue;
		sg_dma_len(&tfr->rx_sg.sgl[0])     += bs->rx_prologue;
	}

	if (!bs->tx_buf)
		goto out;

	if (likely(!bs->tx_spillover)) {
		sg_dma_address(&tfr->tx_sg.sgl[0]) -= bs->tx_prologue;
		sg_dma_len(&tfr->tx_sg.sgl[0])     += bs->tx_prologue;
	} else {
		sg_dma_len(&tfr->tx_sg.sgl[0])      = bs->tx_prologue - 4;
		sg_dma_address(&tfr->tx_sg.sgl[1]) -= 4;
		sg_dma_len(&tfr->tx_sg.sgl[1])     += 4;
	}
out:
	bs->tx_prologue = 0;
}

 
static void bcm2835_spi_dma_rx_done(void *data)
{
	struct spi_controller *ctlr = data;
	struct bcm2835_spi *bs = spi_controller_get_devdata(ctlr);

	 
	dmaengine_terminate_async(ctlr->dma_tx);
	bs->tx_dma_active = false;
	bs->rx_dma_active = false;
	bcm2835_spi_undo_prologue(bs);

	 
	bcm2835_spi_reset_hw(bs);

	 ;
	spi_finalize_current_transfer(ctlr);
}

 
static void bcm2835_spi_dma_tx_done(void *data)
{
	struct spi_controller *ctlr = data;
	struct bcm2835_spi *bs = spi_controller_get_devdata(ctlr);

	 
	while (!(bcm2835_rd(bs, BCM2835_SPI_CS) & BCM2835_SPI_CS_DONE))
		bcm2835_wr(bs, BCM2835_SPI_CS, bs->target->clear_rx_cs);

	bs->tx_dma_active = false;
	smp_wmb();

	 
	if (cmpxchg(&bs->rx_dma_active, true, false))
		dmaengine_terminate_async(ctlr->dma_rx);

	bcm2835_spi_undo_prologue(bs);
	bcm2835_spi_reset_hw(bs);
	spi_finalize_current_transfer(ctlr);
}

 
static int bcm2835_spi_prepare_sg(struct spi_controller *ctlr,
				  struct spi_transfer *tfr,
				  struct bcm2835_spi *bs,
				  struct bcm2835_spidev *target,
				  bool is_tx)
{
	struct dma_chan *chan;
	struct scatterlist *sgl;
	unsigned int nents;
	enum dma_transfer_direction dir;
	unsigned long flags;

	struct dma_async_tx_descriptor *desc;
	dma_cookie_t cookie;

	if (is_tx) {
		dir   = DMA_MEM_TO_DEV;
		chan  = ctlr->dma_tx;
		nents = tfr->tx_sg.nents;
		sgl   = tfr->tx_sg.sgl;
		flags = tfr->rx_buf ? 0 : DMA_PREP_INTERRUPT;
	} else {
		dir   = DMA_DEV_TO_MEM;
		chan  = ctlr->dma_rx;
		nents = tfr->rx_sg.nents;
		sgl   = tfr->rx_sg.sgl;
		flags = DMA_PREP_INTERRUPT;
	}
	 
	desc = dmaengine_prep_slave_sg(chan, sgl, nents, dir, flags);
	if (!desc)
		return -EINVAL;

	 
	if (!is_tx) {
		desc->callback = bcm2835_spi_dma_rx_done;
		desc->callback_param = ctlr;
	} else if (!tfr->rx_buf) {
		desc->callback = bcm2835_spi_dma_tx_done;
		desc->callback_param = ctlr;
		bs->target = target;
	}

	 
	cookie = dmaengine_submit(desc);

	return dma_submit_error(cookie);
}

 
static int bcm2835_spi_transfer_one_dma(struct spi_controller *ctlr,
					struct spi_transfer *tfr,
					struct bcm2835_spidev *target,
					u32 cs)
{
	struct bcm2835_spi *bs = spi_controller_get_devdata(ctlr);
	dma_cookie_t cookie;
	int ret;

	 
	bs->count_transfer_dma++;

	 
	bcm2835_spi_transfer_prologue(ctlr, tfr, bs, cs);

	 
	if (bs->tx_buf) {
		ret = bcm2835_spi_prepare_sg(ctlr, tfr, bs, target, true);
	} else {
		cookie = dmaengine_submit(bs->fill_tx_desc);
		ret = dma_submit_error(cookie);
	}
	if (ret)
		goto err_reset_hw;

	 
	bcm2835_wr(bs, BCM2835_SPI_DLEN, bs->tx_len);

	 
	bcm2835_wr(bs, BCM2835_SPI_CS,
		   cs | BCM2835_SPI_CS_TA | BCM2835_SPI_CS_DMAEN);

	bs->tx_dma_active = true;
	smp_wmb();

	 
	dma_async_issue_pending(ctlr->dma_tx);

	 
	if (bs->rx_buf) {
		ret = bcm2835_spi_prepare_sg(ctlr, tfr, bs, target, false);
	} else {
		cookie = dmaengine_submit(target->clear_rx_desc);
		ret = dma_submit_error(cookie);
	}
	if (ret) {
		 
		dmaengine_terminate_sync(ctlr->dma_tx);
		bs->tx_dma_active = false;
		goto err_reset_hw;
	}

	 
	dma_async_issue_pending(ctlr->dma_rx);
	bs->rx_dma_active = true;
	smp_mb();

	 
	if (!bs->rx_buf && !bs->tx_dma_active &&
	    cmpxchg(&bs->rx_dma_active, true, false)) {
		dmaengine_terminate_async(ctlr->dma_rx);
		bcm2835_spi_reset_hw(bs);
	}

	 
	return 1;

err_reset_hw:
	bcm2835_spi_reset_hw(bs);
	bcm2835_spi_undo_prologue(bs);
	return ret;
}

static bool bcm2835_spi_can_dma(struct spi_controller *ctlr,
				struct spi_device *spi,
				struct spi_transfer *tfr)
{
	 
	if (tfr->len < BCM2835_SPI_DMA_MIN_LENGTH)
		return false;

	 
	return true;
}

static void bcm2835_dma_release(struct spi_controller *ctlr,
				struct bcm2835_spi *bs)
{
	if (ctlr->dma_tx) {
		dmaengine_terminate_sync(ctlr->dma_tx);

		if (bs->fill_tx_desc)
			dmaengine_desc_free(bs->fill_tx_desc);

		if (bs->fill_tx_addr)
			dma_unmap_page_attrs(ctlr->dma_tx->device->dev,
					     bs->fill_tx_addr, sizeof(u32),
					     DMA_TO_DEVICE,
					     DMA_ATTR_SKIP_CPU_SYNC);

		dma_release_channel(ctlr->dma_tx);
		ctlr->dma_tx = NULL;
	}

	if (ctlr->dma_rx) {
		dmaengine_terminate_sync(ctlr->dma_rx);
		dma_release_channel(ctlr->dma_rx);
		ctlr->dma_rx = NULL;
	}
}

static int bcm2835_dma_init(struct spi_controller *ctlr, struct device *dev,
			    struct bcm2835_spi *bs)
{
	struct dma_slave_config slave_config;
	const __be32 *addr;
	dma_addr_t dma_reg_base;
	int ret;

	 
	addr = of_get_address(ctlr->dev.of_node, 0, NULL, NULL);
	if (!addr) {
		dev_err(dev, "could not get DMA-register address - not using dma mode\n");
		 
		return 0;
	}
	dma_reg_base = be32_to_cpup(addr);

	 
	ctlr->dma_tx = dma_request_chan(dev, "tx");
	if (IS_ERR(ctlr->dma_tx)) {
		ret = dev_err_probe(dev, PTR_ERR(ctlr->dma_tx),
			"no tx-dma configuration found - not using dma mode\n");
		ctlr->dma_tx = NULL;
		goto err;
	}
	ctlr->dma_rx = dma_request_chan(dev, "rx");
	if (IS_ERR(ctlr->dma_rx)) {
		ret = dev_err_probe(dev, PTR_ERR(ctlr->dma_rx),
			"no rx-dma configuration found - not using dma mode\n");
		ctlr->dma_rx = NULL;
		goto err_release;
	}

	 
	slave_config.dst_addr = (u32)(dma_reg_base + BCM2835_SPI_FIFO);
	slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;

	ret = dmaengine_slave_config(ctlr->dma_tx, &slave_config);
	if (ret)
		goto err_config;

	bs->fill_tx_addr = dma_map_page_attrs(ctlr->dma_tx->device->dev,
					      ZERO_PAGE(0), 0, sizeof(u32),
					      DMA_TO_DEVICE,
					      DMA_ATTR_SKIP_CPU_SYNC);
	if (dma_mapping_error(ctlr->dma_tx->device->dev, bs->fill_tx_addr)) {
		dev_err(dev, "cannot map zero page - not using DMA mode\n");
		bs->fill_tx_addr = 0;
		ret = -ENOMEM;
		goto err_release;
	}

	bs->fill_tx_desc = dmaengine_prep_dma_cyclic(ctlr->dma_tx,
						     bs->fill_tx_addr,
						     sizeof(u32), 0,
						     DMA_MEM_TO_DEV, 0);
	if (!bs->fill_tx_desc) {
		dev_err(dev, "cannot prepare fill_tx_desc - not using DMA mode\n");
		ret = -ENOMEM;
		goto err_release;
	}

	ret = dmaengine_desc_set_reuse(bs->fill_tx_desc);
	if (ret) {
		dev_err(dev, "cannot reuse fill_tx_desc - not using DMA mode\n");
		goto err_release;
	}

	 
	slave_config.src_addr = (u32)(dma_reg_base + BCM2835_SPI_FIFO);
	slave_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	slave_config.dst_addr = (u32)(dma_reg_base + BCM2835_SPI_CS);
	slave_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;

	ret = dmaengine_slave_config(ctlr->dma_rx, &slave_config);
	if (ret)
		goto err_config;

	 
	ctlr->can_dma = bcm2835_spi_can_dma;

	return 0;

err_config:
	dev_err(dev, "issue configuring dma: %d - not using DMA mode\n",
		ret);
err_release:
	bcm2835_dma_release(ctlr, bs);
err:
	 
	if (ret != -EPROBE_DEFER)
		ret = 0;

	return ret;
}

static int bcm2835_spi_transfer_one_poll(struct spi_controller *ctlr,
					 struct spi_device *spi,
					 struct spi_transfer *tfr,
					 u32 cs)
{
	struct bcm2835_spi *bs = spi_controller_get_devdata(ctlr);
	unsigned long timeout;

	 
	bs->count_transfer_polling++;

	 
	bcm2835_wr(bs, BCM2835_SPI_CS, cs | BCM2835_SPI_CS_TA);

	 
	bcm2835_wr_fifo_blind(bs, BCM2835_SPI_FIFO_SIZE);

	 
	timeout = jiffies + 2 + HZ * polling_limit_us / 1000000;

	 
	while (bs->rx_len) {
		 
		bcm2835_wr_fifo(bs);

		 
		bcm2835_rd_fifo(bs);

		 
		if (bs->rx_len && time_after(jiffies, timeout)) {
			dev_dbg_ratelimited(&spi->dev,
					    "timeout period reached: jiffies: %lu remaining tx/rx: %d/%d - falling back to interrupt mode\n",
					    jiffies - timeout,
					    bs->tx_len, bs->rx_len);
			 

			 
			bs->count_transfer_irq_after_polling++;

			return bcm2835_spi_transfer_one_irq(ctlr, spi,
							    tfr, cs, false);
		}
	}

	 
	bcm2835_spi_reset_hw(bs);
	 
	return 0;
}

static int bcm2835_spi_transfer_one(struct spi_controller *ctlr,
				    struct spi_device *spi,
				    struct spi_transfer *tfr)
{
	struct bcm2835_spi *bs = spi_controller_get_devdata(ctlr);
	struct bcm2835_spidev *target = spi_get_ctldata(spi);
	unsigned long spi_hz, cdiv;
	unsigned long hz_per_byte, byte_limit;
	u32 cs = target->prepare_cs;

	 
	spi_hz = tfr->speed_hz;

	if (spi_hz >= bs->clk_hz / 2) {
		cdiv = 2;  
	} else if (spi_hz) {
		 
		cdiv = DIV_ROUND_UP(bs->clk_hz, spi_hz);
		cdiv += (cdiv % 2);

		if (cdiv >= 65536)
			cdiv = 0;  
	} else {
		cdiv = 0;  
	}
	tfr->effective_speed_hz = cdiv ? (bs->clk_hz / cdiv) : (bs->clk_hz / 65536);
	bcm2835_wr(bs, BCM2835_SPI_CLK, cdiv);

	 
	if (spi->mode & SPI_3WIRE && tfr->rx_buf)
		cs |= BCM2835_SPI_CS_REN;

	 
	bs->tx_buf = tfr->tx_buf;
	bs->rx_buf = tfr->rx_buf;
	bs->tx_len = tfr->len;
	bs->rx_len = tfr->len;

	 
	hz_per_byte = polling_limit_us ? (9 * 1000000) / polling_limit_us : 0;
	byte_limit = hz_per_byte ? tfr->effective_speed_hz / hz_per_byte : 1;

	 
	if (tfr->len < byte_limit)
		return bcm2835_spi_transfer_one_poll(ctlr, spi, tfr, cs);

	 
	if (ctlr->can_dma && bcm2835_spi_can_dma(ctlr, spi, tfr))
		return bcm2835_spi_transfer_one_dma(ctlr, tfr, target, cs);

	 
	return bcm2835_spi_transfer_one_irq(ctlr, spi, tfr, cs, true);
}

static int bcm2835_spi_prepare_message(struct spi_controller *ctlr,
				       struct spi_message *msg)
{
	struct spi_device *spi = msg->spi;
	struct bcm2835_spi *bs = spi_controller_get_devdata(ctlr);
	struct bcm2835_spidev *target = spi_get_ctldata(spi);
	int ret;

	if (ctlr->can_dma) {
		 
		ret = spi_split_transfers_maxsize(ctlr, msg, 65532,
						  GFP_KERNEL | GFP_DMA);
		if (ret)
			return ret;
	}

	 
	bcm2835_wr(bs, BCM2835_SPI_CS, target->prepare_cs);

	return 0;
}

static void bcm2835_spi_handle_err(struct spi_controller *ctlr,
				   struct spi_message *msg)
{
	struct bcm2835_spi *bs = spi_controller_get_devdata(ctlr);

	 
	if (ctlr->dma_tx) {
		dmaengine_terminate_sync(ctlr->dma_tx);
		bs->tx_dma_active = false;
	}
	if (ctlr->dma_rx) {
		dmaengine_terminate_sync(ctlr->dma_rx);
		bs->rx_dma_active = false;
	}
	bcm2835_spi_undo_prologue(bs);

	 
	bcm2835_spi_reset_hw(bs);
}

static int chip_match_name(struct gpio_chip *chip, void *data)
{
	return !strcmp(chip->label, data);
}

static void bcm2835_spi_cleanup(struct spi_device *spi)
{
	struct bcm2835_spidev *target = spi_get_ctldata(spi);
	struct spi_controller *ctlr = spi->controller;

	if (target->clear_rx_desc)
		dmaengine_desc_free(target->clear_rx_desc);

	if (target->clear_rx_addr)
		dma_unmap_single(ctlr->dma_rx->device->dev,
				 target->clear_rx_addr,
				 sizeof(u32),
				 DMA_TO_DEVICE);

	kfree(target);
}

static int bcm2835_spi_setup_dma(struct spi_controller *ctlr,
				 struct spi_device *spi,
				 struct bcm2835_spi *bs,
				 struct bcm2835_spidev *target)
{
	int ret;

	if (!ctlr->dma_rx)
		return 0;

	target->clear_rx_addr = dma_map_single(ctlr->dma_rx->device->dev,
					       &target->clear_rx_cs,
					       sizeof(u32),
					       DMA_TO_DEVICE);
	if (dma_mapping_error(ctlr->dma_rx->device->dev, target->clear_rx_addr)) {
		dev_err(&spi->dev, "cannot map clear_rx_cs\n");
		target->clear_rx_addr = 0;
		return -ENOMEM;
	}

	target->clear_rx_desc = dmaengine_prep_dma_cyclic(ctlr->dma_rx,
						          target->clear_rx_addr,
						          sizeof(u32), 0,
						          DMA_MEM_TO_DEV, 0);
	if (!target->clear_rx_desc) {
		dev_err(&spi->dev, "cannot prepare clear_rx_desc\n");
		return -ENOMEM;
	}

	ret = dmaengine_desc_set_reuse(target->clear_rx_desc);
	if (ret) {
		dev_err(&spi->dev, "cannot reuse clear_rx_desc\n");
		return ret;
	}

	return 0;
}

static int bcm2835_spi_setup(struct spi_device *spi)
{
	struct spi_controller *ctlr = spi->controller;
	struct bcm2835_spi *bs = spi_controller_get_devdata(ctlr);
	struct bcm2835_spidev *target = spi_get_ctldata(spi);
	struct gpio_chip *chip;
	int ret;
	u32 cs;

	if (!target) {
		target = kzalloc(ALIGN(sizeof(*target), dma_get_cache_alignment()),
			      GFP_KERNEL);
		if (!target)
			return -ENOMEM;

		spi_set_ctldata(spi, target);

		ret = bcm2835_spi_setup_dma(ctlr, spi, bs, target);
		if (ret)
			goto err_cleanup;
	}

	 
	cs = BCM2835_SPI_CS_CS_10 | BCM2835_SPI_CS_CS_01;
	if (spi->mode & SPI_CPOL)
		cs |= BCM2835_SPI_CS_CPOL;
	if (spi->mode & SPI_CPHA)
		cs |= BCM2835_SPI_CS_CPHA;
	target->prepare_cs = cs;

	 
	if (ctlr->dma_rx) {
		target->clear_rx_cs = cs | BCM2835_SPI_CS_TA |
					BCM2835_SPI_CS_DMAEN |
					BCM2835_SPI_CS_CLEAR_RX;
		dma_sync_single_for_device(ctlr->dma_rx->device->dev,
					   target->clear_rx_addr,
					   sizeof(u32),
					   DMA_TO_DEVICE);
	}

	 
	if (spi->mode & SPI_NO_CS)
		return 0;
	 
	if (spi_get_csgpiod(spi, 0))
		return 0;
	if (spi_get_chipselect(spi, 0) > 1) {
		 
		dev_err(&spi->dev,
			"setup: only two native chip-selects are supported\n");
		ret = -EINVAL;
		goto err_cleanup;
	}

	 

	 
	chip = gpiochip_find("pinctrl-bcm2835", chip_match_name);
	if (!chip)
		return 0;

	spi_set_csgpiod(spi, 0, gpiochip_request_own_desc(chip,
							  8 - (spi_get_chipselect(spi, 0)),
							  DRV_NAME,
							  GPIO_LOOKUP_FLAGS_DEFAULT,
							  GPIOD_OUT_LOW));
	if (IS_ERR(spi_get_csgpiod(spi, 0))) {
		ret = PTR_ERR(spi_get_csgpiod(spi, 0));
		goto err_cleanup;
	}

	 
	dev_info(&spi->dev, "setting up native-CS%i to use GPIO\n",
		 spi_get_chipselect(spi, 0));

	return 0;

err_cleanup:
	bcm2835_spi_cleanup(spi);
	return ret;
}

static int bcm2835_spi_probe(struct platform_device *pdev)
{
	struct spi_controller *ctlr;
	struct bcm2835_spi *bs;
	int err;

	ctlr = devm_spi_alloc_host(&pdev->dev, sizeof(*bs));
	if (!ctlr)
		return -ENOMEM;

	platform_set_drvdata(pdev, ctlr);

	ctlr->use_gpio_descriptors = true;
	ctlr->mode_bits = BCM2835_SPI_MODE_BITS;
	ctlr->bits_per_word_mask = SPI_BPW_MASK(8);
	ctlr->num_chipselect = 3;
	ctlr->setup = bcm2835_spi_setup;
	ctlr->cleanup = bcm2835_spi_cleanup;
	ctlr->transfer_one = bcm2835_spi_transfer_one;
	ctlr->handle_err = bcm2835_spi_handle_err;
	ctlr->prepare_message = bcm2835_spi_prepare_message;
	ctlr->dev.of_node = pdev->dev.of_node;

	bs = spi_controller_get_devdata(ctlr);
	bs->ctlr = ctlr;

	bs->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(bs->regs))
		return PTR_ERR(bs->regs);

	bs->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(bs->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(bs->clk),
				     "could not get clk\n");

	ctlr->max_speed_hz = clk_get_rate(bs->clk) / 2;

	bs->irq = platform_get_irq(pdev, 0);
	if (bs->irq < 0)
		return bs->irq;

	err = clk_prepare_enable(bs->clk);
	if (err)
		return err;
	bs->clk_hz = clk_get_rate(bs->clk);

	err = bcm2835_dma_init(ctlr, &pdev->dev, bs);
	if (err)
		goto out_clk_disable;

	 
	bcm2835_wr(bs, BCM2835_SPI_CS,
		   BCM2835_SPI_CS_CLEAR_RX | BCM2835_SPI_CS_CLEAR_TX);

	err = devm_request_irq(&pdev->dev, bs->irq, bcm2835_spi_interrupt,
			       IRQF_SHARED, dev_name(&pdev->dev), bs);
	if (err) {
		dev_err(&pdev->dev, "could not request IRQ: %d\n", err);
		goto out_dma_release;
	}

	err = spi_register_controller(ctlr);
	if (err) {
		dev_err(&pdev->dev, "could not register SPI controller: %d\n",
			err);
		goto out_dma_release;
	}

	bcm2835_debugfs_create(bs, dev_name(&pdev->dev));

	return 0;

out_dma_release:
	bcm2835_dma_release(ctlr, bs);
out_clk_disable:
	clk_disable_unprepare(bs->clk);
	return err;
}

static void bcm2835_spi_remove(struct platform_device *pdev)
{
	struct spi_controller *ctlr = platform_get_drvdata(pdev);
	struct bcm2835_spi *bs = spi_controller_get_devdata(ctlr);

	bcm2835_debugfs_remove(bs);

	spi_unregister_controller(ctlr);

	bcm2835_dma_release(ctlr, bs);

	 
	bcm2835_wr(bs, BCM2835_SPI_CS,
		   BCM2835_SPI_CS_CLEAR_RX | BCM2835_SPI_CS_CLEAR_TX);

	clk_disable_unprepare(bs->clk);
}

static const struct of_device_id bcm2835_spi_match[] = {
	{ .compatible = "brcm,bcm2835-spi", },
	{}
};
MODULE_DEVICE_TABLE(of, bcm2835_spi_match);

static struct platform_driver bcm2835_spi_driver = {
	.driver		= {
		.name		= DRV_NAME,
		.of_match_table	= bcm2835_spi_match,
	},
	.probe		= bcm2835_spi_probe,
	.remove_new	= bcm2835_spi_remove,
	.shutdown	= bcm2835_spi_remove,
};
module_platform_driver(bcm2835_spi_driver);

MODULE_DESCRIPTION("SPI controller driver for Broadcom BCM2835");
MODULE_AUTHOR("Chris Boot <bootc@bootc.net>");
MODULE_LICENSE("GPL");
