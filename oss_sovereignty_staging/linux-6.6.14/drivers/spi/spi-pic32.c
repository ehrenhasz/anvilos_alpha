
 

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>

 
struct pic32_spi_regs {
	u32 ctrl;
	u32 ctrl_clr;
	u32 ctrl_set;
	u32 ctrl_inv;
	u32 status;
	u32 status_clr;
	u32 status_set;
	u32 status_inv;
	u32 buf;
	u32 dontuse[3];
	u32 baud;
	u32 dontuse2[3];
	u32 ctrl2;
	u32 ctrl2_clr;
	u32 ctrl2_set;
	u32 ctrl2_inv;
};

 
#define CTRL_RX_INT_SHIFT	0   
#define  RX_FIFO_EMPTY		0
#define  RX_FIFO_NOT_EMPTY	1  
#define  RX_FIFO_HALF_FULL	2  
#define  RX_FIFO_FULL		3  

#define CTRL_TX_INT_SHIFT	2   
#define  TX_FIFO_ALL_EMPTY	0  
#define  TX_FIFO_EMPTY		1  
#define  TX_FIFO_HALF_EMPTY	2  
#define  TX_FIFO_NOT_FULL	3  

#define CTRL_MSTEN	BIT(5)  
#define CTRL_CKP	BIT(6)  
#define CTRL_CKE	BIT(8)  
#define CTRL_SMP	BIT(9)  
#define CTRL_BPW_MASK	0x03    
#define CTRL_BPW_SHIFT	10
#define  PIC32_BPW_8	0
#define  PIC32_BPW_16	1
#define  PIC32_BPW_32	2
#define CTRL_SIDL	BIT(13)  
#define CTRL_ON		BIT(15)  
#define CTRL_ENHBUF	BIT(16)  
#define CTRL_MCLKSEL	BIT(23)  
#define CTRL_MSSEN	BIT(28)  
#define CTRL_FRMEN	BIT(31)  

 
#define STAT_RF_EMPTY	BIT(5)  
#define STAT_RX_OV	BIT(6)  
#define STAT_TX_UR	BIT(8)  
#define STAT_FRM_ERR	BIT(12)  
#define STAT_TF_LVL_MASK	0x1F
#define STAT_TF_LVL_SHIFT	16
#define STAT_RF_LVL_MASK	0x1F
#define STAT_RF_LVL_SHIFT	24

 
#define BAUD_MASK		0x1ff

 
#define CTRL2_TX_UR_EN		BIT(10)  
#define CTRL2_RX_OV_EN		BIT(11)  
#define CTRL2_FRM_ERR_EN	BIT(12)  

 
#define PIC32_DMA_LEN_MIN	64

struct pic32_spi {
	dma_addr_t		dma_base;
	struct pic32_spi_regs __iomem *regs;
	int			fault_irq;
	int			rx_irq;
	int			tx_irq;
	u32			fifo_n_byte;  
	struct clk		*clk;
	struct spi_controller	*host;
	 
	u32			speed_hz;  
	u32			mode;
	u32			bits_per_word;
	u32			fifo_n_elm;  
#define PIC32F_DMA_PREP		0  
	unsigned long		flags;
	 
	struct completion	xfer_done;
	 
	const void		*tx;
	const void		*tx_end;
	const void		*rx;
	const void		*rx_end;
	int			len;
	void (*rx_fifo)(struct pic32_spi *);
	void (*tx_fifo)(struct pic32_spi *);
};

static inline void pic32_spi_enable(struct pic32_spi *pic32s)
{
	writel(CTRL_ON | CTRL_SIDL, &pic32s->regs->ctrl_set);
}

static inline void pic32_spi_disable(struct pic32_spi *pic32s)
{
	writel(CTRL_ON | CTRL_SIDL, &pic32s->regs->ctrl_clr);

	 
	ndelay(20);
}

static void pic32_spi_set_clk_rate(struct pic32_spi *pic32s, u32 spi_ck)
{
	u32 div;

	 
	div = DIV_ROUND_CLOSEST(clk_get_rate(pic32s->clk), 2 * spi_ck) - 1;

	writel(div & BAUD_MASK, &pic32s->regs->baud);
}

static inline u32 pic32_rx_fifo_level(struct pic32_spi *pic32s)
{
	u32 sr = readl(&pic32s->regs->status);

	return (sr >> STAT_RF_LVL_SHIFT) & STAT_RF_LVL_MASK;
}

static inline u32 pic32_tx_fifo_level(struct pic32_spi *pic32s)
{
	u32 sr = readl(&pic32s->regs->status);

	return (sr >> STAT_TF_LVL_SHIFT) & STAT_TF_LVL_MASK;
}

 
static u32 pic32_tx_max(struct pic32_spi *pic32s, int n_bytes)
{
	u32 tx_left, tx_room, rxtx_gap;

	tx_left = (pic32s->tx_end - pic32s->tx) / n_bytes;
	tx_room = pic32s->fifo_n_elm - pic32_tx_fifo_level(pic32s);

	 
	rxtx_gap = ((pic32s->rx_end - pic32s->rx) -
		    (pic32s->tx_end - pic32s->tx)) / n_bytes;
	return min3(tx_left, tx_room, (u32)(pic32s->fifo_n_elm - rxtx_gap));
}

 
static u32 pic32_rx_max(struct pic32_spi *pic32s, int n_bytes)
{
	u32 rx_left = (pic32s->rx_end - pic32s->rx) / n_bytes;

	return min_t(u32, rx_left, pic32_rx_fifo_level(pic32s));
}

#define BUILD_SPI_FIFO_RW(__name, __type, __bwl)		\
static void pic32_spi_rx_##__name(struct pic32_spi *pic32s)	\
{								\
	__type v;						\
	u32 mx = pic32_rx_max(pic32s, sizeof(__type));		\
	for (; mx; mx--) {					\
		v = read##__bwl(&pic32s->regs->buf);		\
		if (pic32s->rx_end - pic32s->len)		\
			*(__type *)(pic32s->rx) = v;		\
		pic32s->rx += sizeof(__type);			\
	}							\
}								\
								\
static void pic32_spi_tx_##__name(struct pic32_spi *pic32s)	\
{								\
	__type v;						\
	u32 mx = pic32_tx_max(pic32s, sizeof(__type));		\
	for (; mx ; mx--) {					\
		v = (__type)~0U;				\
		if (pic32s->tx_end - pic32s->len)		\
			v = *(__type *)(pic32s->tx);		\
		write##__bwl(v, &pic32s->regs->buf);		\
		pic32s->tx += sizeof(__type);			\
	}							\
}

BUILD_SPI_FIFO_RW(byte, u8, b);
BUILD_SPI_FIFO_RW(word, u16, w);
BUILD_SPI_FIFO_RW(dword, u32, l);

static void pic32_err_stop(struct pic32_spi *pic32s, const char *msg)
{
	 
	disable_irq_nosync(pic32s->fault_irq);
	disable_irq_nosync(pic32s->rx_irq);
	disable_irq_nosync(pic32s->tx_irq);

	 
	dev_err(&pic32s->host->dev, "%s\n", msg);
	if (pic32s->host->cur_msg)
		pic32s->host->cur_msg->status = -EIO;
	complete(&pic32s->xfer_done);
}

static irqreturn_t pic32_spi_fault_irq(int irq, void *dev_id)
{
	struct pic32_spi *pic32s = dev_id;
	u32 status;

	status = readl(&pic32s->regs->status);

	 
	if (status & (STAT_RX_OV | STAT_TX_UR)) {
		writel(STAT_RX_OV, &pic32s->regs->status_clr);
		writel(STAT_TX_UR, &pic32s->regs->status_clr);
		pic32_err_stop(pic32s, "err_irq: fifo ov/ur-run\n");
		return IRQ_HANDLED;
	}

	if (status & STAT_FRM_ERR) {
		pic32_err_stop(pic32s, "err_irq: frame error");
		return IRQ_HANDLED;
	}

	if (!pic32s->host->cur_msg) {
		pic32_err_stop(pic32s, "err_irq: no mesg");
		return IRQ_NONE;
	}

	return IRQ_NONE;
}

static irqreturn_t pic32_spi_rx_irq(int irq, void *dev_id)
{
	struct pic32_spi *pic32s = dev_id;

	pic32s->rx_fifo(pic32s);

	 
	if (pic32s->rx_end == pic32s->rx) {
		 
		disable_irq_nosync(pic32s->fault_irq);
		disable_irq_nosync(pic32s->rx_irq);

		 
		complete(&pic32s->xfer_done);
	}

	return IRQ_HANDLED;
}

static irqreturn_t pic32_spi_tx_irq(int irq, void *dev_id)
{
	struct pic32_spi *pic32s = dev_id;

	pic32s->tx_fifo(pic32s);

	 
	if (pic32s->tx_end == pic32s->tx)
		disable_irq_nosync(pic32s->tx_irq);

	return IRQ_HANDLED;
}

static void pic32_spi_dma_rx_notify(void *data)
{
	struct pic32_spi *pic32s = data;

	complete(&pic32s->xfer_done);
}

static int pic32_spi_dma_transfer(struct pic32_spi *pic32s,
				  struct spi_transfer *xfer)
{
	struct spi_controller *host = pic32s->host;
	struct dma_async_tx_descriptor *desc_rx;
	struct dma_async_tx_descriptor *desc_tx;
	dma_cookie_t cookie;
	int ret;

	if (!host->dma_rx || !host->dma_tx)
		return -ENODEV;

	desc_rx = dmaengine_prep_slave_sg(host->dma_rx,
					  xfer->rx_sg.sgl,
					  xfer->rx_sg.nents,
					  DMA_DEV_TO_MEM,
					  DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc_rx) {
		ret = -EINVAL;
		goto err_dma;
	}

	desc_tx = dmaengine_prep_slave_sg(host->dma_tx,
					  xfer->tx_sg.sgl,
					  xfer->tx_sg.nents,
					  DMA_MEM_TO_DEV,
					  DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc_tx) {
		ret = -EINVAL;
		goto err_dma;
	}

	 
	desc_rx->callback = pic32_spi_dma_rx_notify;
	desc_rx->callback_param = pic32s;

	cookie = dmaengine_submit(desc_rx);
	ret = dma_submit_error(cookie);
	if (ret)
		goto err_dma;

	cookie = dmaengine_submit(desc_tx);
	ret = dma_submit_error(cookie);
	if (ret)
		goto err_dma_tx;

	dma_async_issue_pending(host->dma_rx);
	dma_async_issue_pending(host->dma_tx);

	return 0;

err_dma_tx:
	dmaengine_terminate_all(host->dma_rx);
err_dma:
	return ret;
}

static int pic32_spi_dma_config(struct pic32_spi *pic32s, u32 dma_width)
{
	int buf_offset = offsetof(struct pic32_spi_regs, buf);
	struct spi_controller *host = pic32s->host;
	struct dma_slave_config cfg;
	int ret;

	memset(&cfg, 0, sizeof(cfg));
	cfg.device_fc = true;
	cfg.src_addr = pic32s->dma_base + buf_offset;
	cfg.dst_addr = pic32s->dma_base + buf_offset;
	cfg.src_maxburst = pic32s->fifo_n_elm / 2;  
	cfg.dst_maxburst = pic32s->fifo_n_elm / 2;  
	cfg.src_addr_width = dma_width;
	cfg.dst_addr_width = dma_width;
	 
	cfg.direction = DMA_MEM_TO_DEV;
	ret = dmaengine_slave_config(host->dma_tx, &cfg);
	if (ret) {
		dev_err(&host->dev, "tx channel setup failed\n");
		return ret;
	}
	 
	cfg.direction = DMA_DEV_TO_MEM;
	ret = dmaengine_slave_config(host->dma_rx, &cfg);
	if (ret)
		dev_err(&host->dev, "rx channel setup failed\n");

	return ret;
}

static int pic32_spi_set_word_size(struct pic32_spi *pic32s, u8 bits_per_word)
{
	enum dma_slave_buswidth dmawidth;
	u32 buswidth, v;

	switch (bits_per_word) {
	case 8:
		pic32s->rx_fifo = pic32_spi_rx_byte;
		pic32s->tx_fifo = pic32_spi_tx_byte;
		buswidth = PIC32_BPW_8;
		dmawidth = DMA_SLAVE_BUSWIDTH_1_BYTE;
		break;
	case 16:
		pic32s->rx_fifo = pic32_spi_rx_word;
		pic32s->tx_fifo = pic32_spi_tx_word;
		buswidth = PIC32_BPW_16;
		dmawidth = DMA_SLAVE_BUSWIDTH_2_BYTES;
		break;
	case 32:
		pic32s->rx_fifo = pic32_spi_rx_dword;
		pic32s->tx_fifo = pic32_spi_tx_dword;
		buswidth = PIC32_BPW_32;
		dmawidth = DMA_SLAVE_BUSWIDTH_4_BYTES;
		break;
	default:
		 
		return -EINVAL;
	}

	 
	pic32s->fifo_n_elm = DIV_ROUND_UP(pic32s->fifo_n_byte,
					  bits_per_word / 8);
	 
	v = readl(&pic32s->regs->ctrl);
	v &= ~(CTRL_BPW_MASK << CTRL_BPW_SHIFT);
	v |= buswidth << CTRL_BPW_SHIFT;
	writel(v, &pic32s->regs->ctrl);

	 
	if (test_bit(PIC32F_DMA_PREP, &pic32s->flags))
		pic32_spi_dma_config(pic32s, dmawidth);

	return 0;
}

static int pic32_spi_prepare_hardware(struct spi_controller *host)
{
	struct pic32_spi *pic32s = spi_controller_get_devdata(host);

	pic32_spi_enable(pic32s);

	return 0;
}

static int pic32_spi_prepare_message(struct spi_controller *host,
				     struct spi_message *msg)
{
	struct pic32_spi *pic32s = spi_controller_get_devdata(host);
	struct spi_device *spi = msg->spi;
	u32 val;

	 
	if (pic32s->bits_per_word != spi->bits_per_word) {
		pic32_spi_set_word_size(pic32s, spi->bits_per_word);
		pic32s->bits_per_word = spi->bits_per_word;
	}

	 
	if (pic32s->speed_hz != spi->max_speed_hz) {
		pic32_spi_set_clk_rate(pic32s, spi->max_speed_hz);
		pic32s->speed_hz = spi->max_speed_hz;
	}

	 
	if (pic32s->mode != spi->mode) {
		val = readl(&pic32s->regs->ctrl);
		 
		if (spi->mode & SPI_CPOL)
			val |= CTRL_CKP;
		else
			val &= ~CTRL_CKP;
		 
		if (spi->mode & SPI_CPHA)
			val &= ~CTRL_CKE;
		else
			val |= CTRL_CKE;

		 
		val |= CTRL_SMP;
		writel(val, &pic32s->regs->ctrl);
		pic32s->mode = spi->mode;
	}

	return 0;
}

static bool pic32_spi_can_dma(struct spi_controller *host,
			      struct spi_device *spi,
			      struct spi_transfer *xfer)
{
	struct pic32_spi *pic32s = spi_controller_get_devdata(host);

	 
	return (xfer->len >= PIC32_DMA_LEN_MIN) &&
	       test_bit(PIC32F_DMA_PREP, &pic32s->flags);
}

static int pic32_spi_one_transfer(struct spi_controller *host,
				  struct spi_device *spi,
				  struct spi_transfer *transfer)
{
	struct pic32_spi *pic32s;
	bool dma_issued = false;
	unsigned long timeout;
	int ret;

	pic32s = spi_controller_get_devdata(host);

	 
	if (transfer->bits_per_word &&
	    (transfer->bits_per_word != pic32s->bits_per_word)) {
		ret = pic32_spi_set_word_size(pic32s, transfer->bits_per_word);
		if (ret)
			return ret;
		pic32s->bits_per_word = transfer->bits_per_word;
	}

	 
	if (transfer->speed_hz && (transfer->speed_hz != pic32s->speed_hz)) {
		pic32_spi_set_clk_rate(pic32s, transfer->speed_hz);
		pic32s->speed_hz = transfer->speed_hz;
	}

	reinit_completion(&pic32s->xfer_done);

	 
	if (transfer->rx_sg.nents && transfer->tx_sg.nents) {
		ret = pic32_spi_dma_transfer(pic32s, transfer);
		if (ret) {
			dev_err(&spi->dev, "dma submit error\n");
			return ret;
		}

		 
		dma_issued = true;
	} else {
		 
		pic32s->tx = (const void *)transfer->tx_buf;
		pic32s->rx = (const void *)transfer->rx_buf;
		pic32s->tx_end = pic32s->tx + transfer->len;
		pic32s->rx_end = pic32s->rx + transfer->len;
		pic32s->len = transfer->len;

		 
		enable_irq(pic32s->fault_irq);
		enable_irq(pic32s->rx_irq);
		enable_irq(pic32s->tx_irq);
	}

	 
	timeout = wait_for_completion_timeout(&pic32s->xfer_done, 2 * HZ);
	if (timeout == 0) {
		dev_err(&spi->dev, "wait error/timedout\n");
		if (dma_issued) {
			dmaengine_terminate_all(host->dma_rx);
			dmaengine_terminate_all(host->dma_tx);
		}
		ret = -ETIMEDOUT;
	} else {
		ret = 0;
	}

	return ret;
}

static int pic32_spi_unprepare_message(struct spi_controller *host,
				       struct spi_message *msg)
{
	 
	return 0;
}

static int pic32_spi_unprepare_hardware(struct spi_controller *host)
{
	struct pic32_spi *pic32s = spi_controller_get_devdata(host);

	pic32_spi_disable(pic32s);

	return 0;
}

 
static int pic32_spi_setup(struct spi_device *spi)
{
	if (!spi->max_speed_hz) {
		dev_err(&spi->dev, "No max speed HZ parameter\n");
		return -EINVAL;
	}

	 
	if (!spi_get_csgpiod(spi, 0))
		return -EINVAL;

	return 0;
}

static void pic32_spi_cleanup(struct spi_device *spi)
{
	 
	gpiod_direction_output(spi_get_csgpiod(spi, 0), 0);
}

static int pic32_spi_dma_prep(struct pic32_spi *pic32s, struct device *dev)
{
	struct spi_controller *host = pic32s->host;
	int ret = 0;

	host->dma_rx = dma_request_chan(dev, "spi-rx");
	if (IS_ERR(host->dma_rx)) {
		if (PTR_ERR(host->dma_rx) == -EPROBE_DEFER)
			ret = -EPROBE_DEFER;
		else
			dev_warn(dev, "RX channel not found.\n");

		host->dma_rx = NULL;
		goto out_err;
	}

	host->dma_tx = dma_request_chan(dev, "spi-tx");
	if (IS_ERR(host->dma_tx)) {
		if (PTR_ERR(host->dma_tx) == -EPROBE_DEFER)
			ret = -EPROBE_DEFER;
		else
			dev_warn(dev, "TX channel not found.\n");

		host->dma_tx = NULL;
		goto out_err;
	}

	if (pic32_spi_dma_config(pic32s, DMA_SLAVE_BUSWIDTH_1_BYTE))
		goto out_err;

	 
	set_bit(PIC32F_DMA_PREP, &pic32s->flags);

	return 0;

out_err:
	if (host->dma_rx) {
		dma_release_channel(host->dma_rx);
		host->dma_rx = NULL;
	}

	if (host->dma_tx) {
		dma_release_channel(host->dma_tx);
		host->dma_tx = NULL;
	}

	return ret;
}

static void pic32_spi_dma_unprep(struct pic32_spi *pic32s)
{
	if (!test_bit(PIC32F_DMA_PREP, &pic32s->flags))
		return;

	clear_bit(PIC32F_DMA_PREP, &pic32s->flags);
	if (pic32s->host->dma_rx)
		dma_release_channel(pic32s->host->dma_rx);

	if (pic32s->host->dma_tx)
		dma_release_channel(pic32s->host->dma_tx);
}

static void pic32_spi_hw_init(struct pic32_spi *pic32s)
{
	u32 ctrl;

	 
	pic32_spi_disable(pic32s);

	ctrl = readl(&pic32s->regs->ctrl);
	 
	ctrl |= CTRL_ENHBUF;
	pic32s->fifo_n_byte = 16;

	 
	ctrl &= ~CTRL_FRMEN;

	 
	ctrl |= CTRL_MSTEN;

	 
	ctrl &= ~(0x3 << CTRL_TX_INT_SHIFT);
	ctrl |= (TX_FIFO_HALF_EMPTY << CTRL_TX_INT_SHIFT);

	 
	ctrl &= ~(0x3 << CTRL_RX_INT_SHIFT);
	ctrl |= (RX_FIFO_NOT_EMPTY << CTRL_RX_INT_SHIFT);

	 
	ctrl &= ~CTRL_MCLKSEL;

	 
	ctrl &= ~CTRL_MSSEN;

	writel(ctrl, &pic32s->regs->ctrl);

	 
	ctrl = CTRL2_TX_UR_EN | CTRL2_RX_OV_EN | CTRL2_FRM_ERR_EN;
	writel(ctrl, &pic32s->regs->ctrl2_set);
}

static int pic32_spi_hw_probe(struct platform_device *pdev,
			      struct pic32_spi *pic32s)
{
	struct resource *mem;
	int ret;

	pic32s->regs = devm_platform_get_and_ioremap_resource(pdev, 0, &mem);
	if (IS_ERR(pic32s->regs))
		return PTR_ERR(pic32s->regs);

	pic32s->dma_base = mem->start;

	 
	pic32s->fault_irq = platform_get_irq_byname(pdev, "fault");
	if (pic32s->fault_irq < 0)
		return pic32s->fault_irq;

	pic32s->rx_irq = platform_get_irq_byname(pdev, "rx");
	if (pic32s->rx_irq < 0)
		return pic32s->rx_irq;

	pic32s->tx_irq = platform_get_irq_byname(pdev, "tx");
	if (pic32s->tx_irq < 0)
		return pic32s->tx_irq;

	 
	pic32s->clk = devm_clk_get(&pdev->dev, "mck0");
	if (IS_ERR(pic32s->clk)) {
		dev_err(&pdev->dev, "clk not found\n");
		ret = PTR_ERR(pic32s->clk);
		goto err_unmap_mem;
	}

	ret = clk_prepare_enable(pic32s->clk);
	if (ret)
		goto err_unmap_mem;

	pic32_spi_hw_init(pic32s);

	return 0;

err_unmap_mem:
	dev_err(&pdev->dev, "%s failed, err %d\n", __func__, ret);
	return ret;
}

static int pic32_spi_probe(struct platform_device *pdev)
{
	struct spi_controller *host;
	struct pic32_spi *pic32s;
	int ret;

	host = spi_alloc_host(&pdev->dev, sizeof(*pic32s));
	if (!host)
		return -ENOMEM;

	pic32s = spi_controller_get_devdata(host);
	pic32s->host = host;

	ret = pic32_spi_hw_probe(pdev, pic32s);
	if (ret)
		goto err_host;

	host->dev.of_node	= pdev->dev.of_node;
	host->mode_bits	= SPI_MODE_3 | SPI_MODE_0 | SPI_CS_HIGH;
	host->num_chipselect	= 1;  
	host->max_speed_hz	= clk_get_rate(pic32s->clk);
	host->setup		= pic32_spi_setup;
	host->cleanup		= pic32_spi_cleanup;
	host->flags		= SPI_CONTROLLER_MUST_TX | SPI_CONTROLLER_MUST_RX;
	host->bits_per_word_mask	= SPI_BPW_MASK(8) | SPI_BPW_MASK(16) |
					  SPI_BPW_MASK(32);
	host->transfer_one		= pic32_spi_one_transfer;
	host->prepare_message		= pic32_spi_prepare_message;
	host->unprepare_message	= pic32_spi_unprepare_message;
	host->prepare_transfer_hardware	= pic32_spi_prepare_hardware;
	host->unprepare_transfer_hardware	= pic32_spi_unprepare_hardware;
	host->use_gpio_descriptors = true;

	 
	ret = pic32_spi_dma_prep(pic32s, &pdev->dev);
	if (ret)
		goto err_bailout;

	if (test_bit(PIC32F_DMA_PREP, &pic32s->flags))
		host->can_dma	= pic32_spi_can_dma;

	init_completion(&pic32s->xfer_done);
	pic32s->mode = -1;

	 
	irq_set_status_flags(pic32s->fault_irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(&pdev->dev, pic32s->fault_irq,
			       pic32_spi_fault_irq, IRQF_NO_THREAD,
			       dev_name(&pdev->dev), pic32s);
	if (ret < 0) {
		dev_err(&pdev->dev, "request fault-irq %d\n", pic32s->rx_irq);
		goto err_bailout;
	}

	 
	irq_set_status_flags(pic32s->rx_irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(&pdev->dev, pic32s->rx_irq,
			       pic32_spi_rx_irq, IRQF_NO_THREAD,
			       dev_name(&pdev->dev), pic32s);
	if (ret < 0) {
		dev_err(&pdev->dev, "request rx-irq %d\n", pic32s->rx_irq);
		goto err_bailout;
	}

	 
	irq_set_status_flags(pic32s->tx_irq, IRQ_NOAUTOEN);
	ret = devm_request_irq(&pdev->dev, pic32s->tx_irq,
			       pic32_spi_tx_irq, IRQF_NO_THREAD,
			       dev_name(&pdev->dev), pic32s);
	if (ret < 0) {
		dev_err(&pdev->dev, "request tx-irq %d\n", pic32s->tx_irq);
		goto err_bailout;
	}

	 
	ret = devm_spi_register_controller(&pdev->dev, host);
	if (ret) {
		dev_err(&host->dev, "failed registering spi host\n");
		goto err_bailout;
	}

	platform_set_drvdata(pdev, pic32s);

	return 0;

err_bailout:
	pic32_spi_dma_unprep(pic32s);
	clk_disable_unprepare(pic32s->clk);
err_host:
	spi_controller_put(host);
	return ret;
}

static void pic32_spi_remove(struct platform_device *pdev)
{
	struct pic32_spi *pic32s;

	pic32s = platform_get_drvdata(pdev);
	pic32_spi_disable(pic32s);
	clk_disable_unprepare(pic32s->clk);
	pic32_spi_dma_unprep(pic32s);
}

static const struct of_device_id pic32_spi_of_match[] = {
	{.compatible = "microchip,pic32mzda-spi",},
	{},
};
MODULE_DEVICE_TABLE(of, pic32_spi_of_match);

static struct platform_driver pic32_spi_driver = {
	.driver = {
		.name = "spi-pic32",
		.of_match_table = of_match_ptr(pic32_spi_of_match),
	},
	.probe = pic32_spi_probe,
	.remove_new = pic32_spi_remove,
};

module_platform_driver(pic32_spi_driver);

MODULE_AUTHOR("Purna Chandra Mandal <purna.mandal@microchip.com>");
MODULE_DESCRIPTION("Microchip SPI driver for PIC32 SPI controller.");
MODULE_LICENSE("GPL v2");
