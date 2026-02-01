
 

#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/scatterlist.h>
#include <linux/spi/spi.h>

#include <linux/platform_data/dma-ep93xx.h>
#include <linux/platform_data/spi-ep93xx.h>

#define SSPCR0			0x0000
#define SSPCR0_SPO		BIT(6)
#define SSPCR0_SPH		BIT(7)
#define SSPCR0_SCR_SHIFT	8

#define SSPCR1			0x0004
#define SSPCR1_RIE		BIT(0)
#define SSPCR1_TIE		BIT(1)
#define SSPCR1_RORIE		BIT(2)
#define SSPCR1_LBM		BIT(3)
#define SSPCR1_SSE		BIT(4)
#define SSPCR1_MS		BIT(5)
#define SSPCR1_SOD		BIT(6)

#define SSPDR			0x0008

#define SSPSR			0x000c
#define SSPSR_TFE		BIT(0)
#define SSPSR_TNF		BIT(1)
#define SSPSR_RNE		BIT(2)
#define SSPSR_RFF		BIT(3)
#define SSPSR_BSY		BIT(4)
#define SSPCPSR			0x0010

#define SSPIIR			0x0014
#define SSPIIR_RIS		BIT(0)
#define SSPIIR_TIS		BIT(1)
#define SSPIIR_RORIS		BIT(2)
#define SSPICR			SSPIIR

 
#define SPI_TIMEOUT		5
 
#define SPI_FIFO_SIZE		8

 
struct ep93xx_spi {
	struct clk			*clk;
	void __iomem			*mmio;
	unsigned long			sspdr_phys;
	size_t				tx;
	size_t				rx;
	size_t				fifo_level;
	struct dma_chan			*dma_rx;
	struct dma_chan			*dma_tx;
	struct ep93xx_dma_data		dma_rx_data;
	struct ep93xx_dma_data		dma_tx_data;
	struct sg_table			rx_sgt;
	struct sg_table			tx_sgt;
	void				*zeropage;
};

 
#define bits_per_word_to_dss(bpw)	((bpw) - 1)

 
static int ep93xx_spi_calc_divisors(struct spi_controller *host,
				    u32 rate, u8 *div_cpsr, u8 *div_scr)
{
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);
	unsigned long spi_clk_rate = clk_get_rate(espi->clk);
	int cpsr, scr;

	 
	rate = clamp(rate, host->min_speed_hz, host->max_speed_hz);

	 
	for (cpsr = 2; cpsr <= 254; cpsr += 2) {
		for (scr = 0; scr <= 255; scr++) {
			if ((spi_clk_rate / (cpsr * (scr + 1))) <= rate) {
				*div_scr = (u8)scr;
				*div_cpsr = (u8)cpsr;
				return 0;
			}
		}
	}

	return -EINVAL;
}

static int ep93xx_spi_chip_setup(struct spi_controller *host,
				 struct spi_device *spi,
				 struct spi_transfer *xfer)
{
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);
	u8 dss = bits_per_word_to_dss(xfer->bits_per_word);
	u8 div_cpsr = 0;
	u8 div_scr = 0;
	u16 cr0;
	int err;

	err = ep93xx_spi_calc_divisors(host, xfer->speed_hz,
				       &div_cpsr, &div_scr);
	if (err)
		return err;

	cr0 = div_scr << SSPCR0_SCR_SHIFT;
	if (spi->mode & SPI_CPOL)
		cr0 |= SSPCR0_SPO;
	if (spi->mode & SPI_CPHA)
		cr0 |= SSPCR0_SPH;
	cr0 |= dss;

	dev_dbg(&host->dev, "setup: mode %d, cpsr %d, scr %d, dss %d\n",
		spi->mode, div_cpsr, div_scr, dss);
	dev_dbg(&host->dev, "setup: cr0 %#x\n", cr0);

	writel(div_cpsr, espi->mmio + SSPCPSR);
	writel(cr0, espi->mmio + SSPCR0);

	return 0;
}

static void ep93xx_do_write(struct spi_controller *host)
{
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);
	struct spi_transfer *xfer = host->cur_msg->state;
	u32 val = 0;

	if (xfer->bits_per_word > 8) {
		if (xfer->tx_buf)
			val = ((u16 *)xfer->tx_buf)[espi->tx];
		espi->tx += 2;
	} else {
		if (xfer->tx_buf)
			val = ((u8 *)xfer->tx_buf)[espi->tx];
		espi->tx += 1;
	}
	writel(val, espi->mmio + SSPDR);
}

static void ep93xx_do_read(struct spi_controller *host)
{
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);
	struct spi_transfer *xfer = host->cur_msg->state;
	u32 val;

	val = readl(espi->mmio + SSPDR);
	if (xfer->bits_per_word > 8) {
		if (xfer->rx_buf)
			((u16 *)xfer->rx_buf)[espi->rx] = val;
		espi->rx += 2;
	} else {
		if (xfer->rx_buf)
			((u8 *)xfer->rx_buf)[espi->rx] = val;
		espi->rx += 1;
	}
}

 
static int ep93xx_spi_read_write(struct spi_controller *host)
{
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);
	struct spi_transfer *xfer = host->cur_msg->state;

	 
	while ((readl(espi->mmio + SSPSR) & SSPSR_RNE)) {
		ep93xx_do_read(host);
		espi->fifo_level--;
	}

	 
	while (espi->fifo_level < SPI_FIFO_SIZE && espi->tx < xfer->len) {
		ep93xx_do_write(host);
		espi->fifo_level++;
	}

	if (espi->rx == xfer->len)
		return 0;

	return -EINPROGRESS;
}

static enum dma_transfer_direction
ep93xx_dma_data_to_trans_dir(enum dma_data_direction dir)
{
	switch (dir) {
	case DMA_TO_DEVICE:
		return DMA_MEM_TO_DEV;
	case DMA_FROM_DEVICE:
		return DMA_DEV_TO_MEM;
	default:
		return DMA_TRANS_NONE;
	}
}

 
static struct dma_async_tx_descriptor *
ep93xx_spi_dma_prepare(struct spi_controller *host,
		       enum dma_data_direction dir)
{
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);
	struct spi_transfer *xfer = host->cur_msg->state;
	struct dma_async_tx_descriptor *txd;
	enum dma_slave_buswidth buswidth;
	struct dma_slave_config conf;
	struct scatterlist *sg;
	struct sg_table *sgt;
	struct dma_chan *chan;
	const void *buf, *pbuf;
	size_t len = xfer->len;
	int i, ret, nents;

	if (xfer->bits_per_word > 8)
		buswidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
	else
		buswidth = DMA_SLAVE_BUSWIDTH_1_BYTE;

	memset(&conf, 0, sizeof(conf));
	conf.direction = ep93xx_dma_data_to_trans_dir(dir);

	if (dir == DMA_FROM_DEVICE) {
		chan = espi->dma_rx;
		buf = xfer->rx_buf;
		sgt = &espi->rx_sgt;

		conf.src_addr = espi->sspdr_phys;
		conf.src_addr_width = buswidth;
	} else {
		chan = espi->dma_tx;
		buf = xfer->tx_buf;
		sgt = &espi->tx_sgt;

		conf.dst_addr = espi->sspdr_phys;
		conf.dst_addr_width = buswidth;
	}

	ret = dmaengine_slave_config(chan, &conf);
	if (ret)
		return ERR_PTR(ret);

	 

	nents = DIV_ROUND_UP(len, PAGE_SIZE);
	if (nents != sgt->nents) {
		sg_free_table(sgt);

		ret = sg_alloc_table(sgt, nents, GFP_KERNEL);
		if (ret)
			return ERR_PTR(ret);
	}

	pbuf = buf;
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		size_t bytes = min_t(size_t, len, PAGE_SIZE);

		if (buf) {
			sg_set_page(sg, virt_to_page(pbuf), bytes,
				    offset_in_page(pbuf));
		} else {
			sg_set_page(sg, virt_to_page(espi->zeropage),
				    bytes, 0);
		}

		pbuf += bytes;
		len -= bytes;
	}

	if (WARN_ON(len)) {
		dev_warn(&host->dev, "len = %zu expected 0!\n", len);
		return ERR_PTR(-EINVAL);
	}

	nents = dma_map_sg(chan->device->dev, sgt->sgl, sgt->nents, dir);
	if (!nents)
		return ERR_PTR(-ENOMEM);

	txd = dmaengine_prep_slave_sg(chan, sgt->sgl, nents, conf.direction,
				      DMA_CTRL_ACK);
	if (!txd) {
		dma_unmap_sg(chan->device->dev, sgt->sgl, sgt->nents, dir);
		return ERR_PTR(-ENOMEM);
	}
	return txd;
}

 
static void ep93xx_spi_dma_finish(struct spi_controller *host,
				  enum dma_data_direction dir)
{
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);
	struct dma_chan *chan;
	struct sg_table *sgt;

	if (dir == DMA_FROM_DEVICE) {
		chan = espi->dma_rx;
		sgt = &espi->rx_sgt;
	} else {
		chan = espi->dma_tx;
		sgt = &espi->tx_sgt;
	}

	dma_unmap_sg(chan->device->dev, sgt->sgl, sgt->nents, dir);
}

static void ep93xx_spi_dma_callback(void *callback_param)
{
	struct spi_controller *host = callback_param;

	ep93xx_spi_dma_finish(host, DMA_TO_DEVICE);
	ep93xx_spi_dma_finish(host, DMA_FROM_DEVICE);

	spi_finalize_current_transfer(host);
}

static int ep93xx_spi_dma_transfer(struct spi_controller *host)
{
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);
	struct dma_async_tx_descriptor *rxd, *txd;

	rxd = ep93xx_spi_dma_prepare(host, DMA_FROM_DEVICE);
	if (IS_ERR(rxd)) {
		dev_err(&host->dev, "DMA RX failed: %ld\n", PTR_ERR(rxd));
		return PTR_ERR(rxd);
	}

	txd = ep93xx_spi_dma_prepare(host, DMA_TO_DEVICE);
	if (IS_ERR(txd)) {
		ep93xx_spi_dma_finish(host, DMA_FROM_DEVICE);
		dev_err(&host->dev, "DMA TX failed: %ld\n", PTR_ERR(txd));
		return PTR_ERR(txd);
	}

	 
	rxd->callback = ep93xx_spi_dma_callback;
	rxd->callback_param = host;

	 
	dmaengine_submit(rxd);
	dmaengine_submit(txd);

	dma_async_issue_pending(espi->dma_rx);
	dma_async_issue_pending(espi->dma_tx);

	 
	return 1;
}

static irqreturn_t ep93xx_spi_interrupt(int irq, void *dev_id)
{
	struct spi_controller *host = dev_id;
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);
	u32 val;

	 
	if (readl(espi->mmio + SSPIIR) & SSPIIR_RORIS) {
		 
		writel(0, espi->mmio + SSPICR);
		dev_warn(&host->dev,
			 "receive overrun, aborting the message\n");
		host->cur_msg->status = -EIO;
	} else {
		 
		if (ep93xx_spi_read_write(host)) {
			 
			return IRQ_HANDLED;
		}
	}

	 
	val = readl(espi->mmio + SSPCR1);
	val &= ~(SSPCR1_RORIE | SSPCR1_TIE | SSPCR1_RIE);
	writel(val, espi->mmio + SSPCR1);

	spi_finalize_current_transfer(host);

	return IRQ_HANDLED;
}

static int ep93xx_spi_transfer_one(struct spi_controller *host,
				   struct spi_device *spi,
				   struct spi_transfer *xfer)
{
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);
	u32 val;
	int ret;

	ret = ep93xx_spi_chip_setup(host, spi, xfer);
	if (ret) {
		dev_err(&host->dev, "failed to setup chip for transfer\n");
		return ret;
	}

	host->cur_msg->state = xfer;
	espi->rx = 0;
	espi->tx = 0;

	 
	if (espi->dma_rx && xfer->len > SPI_FIFO_SIZE)
		return ep93xx_spi_dma_transfer(host);

	 
	ep93xx_spi_read_write(host);

	val = readl(espi->mmio + SSPCR1);
	val |= (SSPCR1_RORIE | SSPCR1_TIE | SSPCR1_RIE);
	writel(val, espi->mmio + SSPCR1);

	 
	return 1;
}

static int ep93xx_spi_prepare_message(struct spi_controller *host,
				      struct spi_message *msg)
{
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);
	unsigned long timeout;

	 
	timeout = jiffies + msecs_to_jiffies(SPI_TIMEOUT);
	while (readl(espi->mmio + SSPSR) & SSPSR_RNE) {
		if (time_after(jiffies, timeout)) {
			dev_warn(&host->dev,
				 "timeout while flushing RX FIFO\n");
			return -ETIMEDOUT;
		}
		readl(espi->mmio + SSPDR);
	}

	 
	espi->fifo_level = 0;

	return 0;
}

static int ep93xx_spi_prepare_hardware(struct spi_controller *host)
{
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);
	u32 val;
	int ret;

	ret = clk_prepare_enable(espi->clk);
	if (ret)
		return ret;

	val = readl(espi->mmio + SSPCR1);
	val |= SSPCR1_SSE;
	writel(val, espi->mmio + SSPCR1);

	return 0;
}

static int ep93xx_spi_unprepare_hardware(struct spi_controller *host)
{
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);
	u32 val;

	val = readl(espi->mmio + SSPCR1);
	val &= ~SSPCR1_SSE;
	writel(val, espi->mmio + SSPCR1);

	clk_disable_unprepare(espi->clk);

	return 0;
}

static bool ep93xx_spi_dma_filter(struct dma_chan *chan, void *filter_param)
{
	if (ep93xx_dma_chan_is_m2p(chan))
		return false;

	chan->private = filter_param;
	return true;
}

static int ep93xx_spi_setup_dma(struct ep93xx_spi *espi)
{
	dma_cap_mask_t mask;
	int ret;

	espi->zeropage = (void *)get_zeroed_page(GFP_KERNEL);
	if (!espi->zeropage)
		return -ENOMEM;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	espi->dma_rx_data.port = EP93XX_DMA_SSP;
	espi->dma_rx_data.direction = DMA_DEV_TO_MEM;
	espi->dma_rx_data.name = "ep93xx-spi-rx";

	espi->dma_rx = dma_request_channel(mask, ep93xx_spi_dma_filter,
					   &espi->dma_rx_data);
	if (!espi->dma_rx) {
		ret = -ENODEV;
		goto fail_free_page;
	}

	espi->dma_tx_data.port = EP93XX_DMA_SSP;
	espi->dma_tx_data.direction = DMA_MEM_TO_DEV;
	espi->dma_tx_data.name = "ep93xx-spi-tx";

	espi->dma_tx = dma_request_channel(mask, ep93xx_spi_dma_filter,
					   &espi->dma_tx_data);
	if (!espi->dma_tx) {
		ret = -ENODEV;
		goto fail_release_rx;
	}

	return 0;

fail_release_rx:
	dma_release_channel(espi->dma_rx);
	espi->dma_rx = NULL;
fail_free_page:
	free_page((unsigned long)espi->zeropage);

	return ret;
}

static void ep93xx_spi_release_dma(struct ep93xx_spi *espi)
{
	if (espi->dma_rx) {
		dma_release_channel(espi->dma_rx);
		sg_free_table(&espi->rx_sgt);
	}
	if (espi->dma_tx) {
		dma_release_channel(espi->dma_tx);
		sg_free_table(&espi->tx_sgt);
	}

	if (espi->zeropage)
		free_page((unsigned long)espi->zeropage);
}

static int ep93xx_spi_probe(struct platform_device *pdev)
{
	struct spi_controller *host;
	struct ep93xx_spi_info *info;
	struct ep93xx_spi *espi;
	struct resource *res;
	int irq;
	int error;

	info = dev_get_platdata(&pdev->dev);
	if (!info) {
		dev_err(&pdev->dev, "missing platform data\n");
		return -EINVAL;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	host = spi_alloc_host(&pdev->dev, sizeof(*espi));
	if (!host)
		return -ENOMEM;

	host->use_gpio_descriptors = true;
	host->prepare_transfer_hardware = ep93xx_spi_prepare_hardware;
	host->unprepare_transfer_hardware = ep93xx_spi_unprepare_hardware;
	host->prepare_message = ep93xx_spi_prepare_message;
	host->transfer_one = ep93xx_spi_transfer_one;
	host->bus_num = pdev->id;
	host->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH;
	host->bits_per_word_mask = SPI_BPW_RANGE_MASK(4, 16);
	 
	host->num_chipselect = 0;

	platform_set_drvdata(pdev, host);

	espi = spi_controller_get_devdata(host);

	espi->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(espi->clk)) {
		dev_err(&pdev->dev, "unable to get spi clock\n");
		error = PTR_ERR(espi->clk);
		goto fail_release_host;
	}

	 
	host->max_speed_hz = clk_get_rate(espi->clk) / 2;
	host->min_speed_hz = clk_get_rate(espi->clk) / (254 * 256);

	espi->mmio = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(espi->mmio)) {
		error = PTR_ERR(espi->mmio);
		goto fail_release_host;
	}
	espi->sspdr_phys = res->start + SSPDR;

	error = devm_request_irq(&pdev->dev, irq, ep93xx_spi_interrupt,
				0, "ep93xx-spi", host);
	if (error) {
		dev_err(&pdev->dev, "failed to request irq\n");
		goto fail_release_host;
	}

	if (info->use_dma && ep93xx_spi_setup_dma(espi))
		dev_warn(&pdev->dev, "DMA setup failed. Falling back to PIO\n");

	 
	writel(0, espi->mmio + SSPCR1);

	error = devm_spi_register_controller(&pdev->dev, host);
	if (error) {
		dev_err(&pdev->dev, "failed to register SPI host\n");
		goto fail_free_dma;
	}

	dev_info(&pdev->dev, "EP93xx SPI Controller at 0x%08lx irq %d\n",
		 (unsigned long)res->start, irq);

	return 0;

fail_free_dma:
	ep93xx_spi_release_dma(espi);
fail_release_host:
	spi_controller_put(host);

	return error;
}

static void ep93xx_spi_remove(struct platform_device *pdev)
{
	struct spi_controller *host = platform_get_drvdata(pdev);
	struct ep93xx_spi *espi = spi_controller_get_devdata(host);

	ep93xx_spi_release_dma(espi);
}

static struct platform_driver ep93xx_spi_driver = {
	.driver		= {
		.name	= "ep93xx-spi",
	},
	.probe		= ep93xx_spi_probe,
	.remove_new	= ep93xx_spi_remove,
};
module_platform_driver(ep93xx_spi_driver);

MODULE_DESCRIPTION("EP93xx SPI Controller driver");
MODULE_AUTHOR("Mika Westerberg <mika.westerberg@iki.fi>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ep93xx-spi");
