








#include <linux/clk.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/io.h>
#include <linux/log2.h>

#define SIFIVE_SPI_DRIVER_NAME           "sifive_spi"

#define SIFIVE_SPI_MAX_CS                32
#define SIFIVE_SPI_DEFAULT_DEPTH         8
#define SIFIVE_SPI_DEFAULT_MAX_BITS      8

 
#define SIFIVE_SPI_REG_SCKDIV            0x00  
#define SIFIVE_SPI_REG_SCKMODE           0x04  
#define SIFIVE_SPI_REG_CSID              0x10  
#define SIFIVE_SPI_REG_CSDEF             0x14  
#define SIFIVE_SPI_REG_CSMODE            0x18  
#define SIFIVE_SPI_REG_DELAY0            0x28  
#define SIFIVE_SPI_REG_DELAY1            0x2c  
#define SIFIVE_SPI_REG_FMT               0x40  
#define SIFIVE_SPI_REG_TXDATA            0x48  
#define SIFIVE_SPI_REG_RXDATA            0x4c  
#define SIFIVE_SPI_REG_TXMARK            0x50  
#define SIFIVE_SPI_REG_RXMARK            0x54  
#define SIFIVE_SPI_REG_FCTRL             0x60  
#define SIFIVE_SPI_REG_FFMT              0x64  
#define SIFIVE_SPI_REG_IE                0x70  
#define SIFIVE_SPI_REG_IP                0x74  

 
#define SIFIVE_SPI_SCKDIV_DIV_MASK       0xfffU

 
#define SIFIVE_SPI_SCKMODE_PHA           BIT(0)
#define SIFIVE_SPI_SCKMODE_POL           BIT(1)
#define SIFIVE_SPI_SCKMODE_MODE_MASK     (SIFIVE_SPI_SCKMODE_PHA | \
					  SIFIVE_SPI_SCKMODE_POL)

 
#define SIFIVE_SPI_CSMODE_MODE_AUTO      0U
#define SIFIVE_SPI_CSMODE_MODE_HOLD      2U
#define SIFIVE_SPI_CSMODE_MODE_OFF       3U

 
#define SIFIVE_SPI_DELAY0_CSSCK(x)       ((u32)(x))
#define SIFIVE_SPI_DELAY0_CSSCK_MASK     0xffU
#define SIFIVE_SPI_DELAY0_SCKCS(x)       ((u32)(x) << 16)
#define SIFIVE_SPI_DELAY0_SCKCS_MASK     (0xffU << 16)

 
#define SIFIVE_SPI_DELAY1_INTERCS(x)     ((u32)(x))
#define SIFIVE_SPI_DELAY1_INTERCS_MASK   0xffU
#define SIFIVE_SPI_DELAY1_INTERXFR(x)    ((u32)(x) << 16)
#define SIFIVE_SPI_DELAY1_INTERXFR_MASK  (0xffU << 16)

 
#define SIFIVE_SPI_FMT_PROTO_SINGLE      0U
#define SIFIVE_SPI_FMT_PROTO_DUAL        1U
#define SIFIVE_SPI_FMT_PROTO_QUAD        2U
#define SIFIVE_SPI_FMT_PROTO_MASK        3U
#define SIFIVE_SPI_FMT_ENDIAN            BIT(2)
#define SIFIVE_SPI_FMT_DIR               BIT(3)
#define SIFIVE_SPI_FMT_LEN(x)            ((u32)(x) << 16)
#define SIFIVE_SPI_FMT_LEN_MASK          (0xfU << 16)

 
#define SIFIVE_SPI_TXDATA_DATA_MASK      0xffU
#define SIFIVE_SPI_TXDATA_FULL           BIT(31)

 
#define SIFIVE_SPI_RXDATA_DATA_MASK      0xffU
#define SIFIVE_SPI_RXDATA_EMPTY          BIT(31)

 
#define SIFIVE_SPI_IP_TXWM               BIT(0)
#define SIFIVE_SPI_IP_RXWM               BIT(1)

struct sifive_spi {
	void __iomem      *regs;         
	struct clk        *clk;          
	unsigned int      fifo_depth;    
	u32               cs_inactive;   
	struct completion done;          
};

static void sifive_spi_write(struct sifive_spi *spi, int offset, u32 value)
{
	iowrite32(value, spi->regs + offset);
}

static u32 sifive_spi_read(struct sifive_spi *spi, int offset)
{
	return ioread32(spi->regs + offset);
}

static void sifive_spi_init(struct sifive_spi *spi)
{
	 
	sifive_spi_write(spi, SIFIVE_SPI_REG_IE, 0);

	 
	sifive_spi_write(spi, SIFIVE_SPI_REG_TXMARK, 1);
	sifive_spi_write(spi, SIFIVE_SPI_REG_RXMARK, 0);

	 
	sifive_spi_write(spi, SIFIVE_SPI_REG_DELAY0,
			 SIFIVE_SPI_DELAY0_CSSCK(1) |
			 SIFIVE_SPI_DELAY0_SCKCS(1));
	sifive_spi_write(spi, SIFIVE_SPI_REG_DELAY1,
			 SIFIVE_SPI_DELAY1_INTERCS(1) |
			 SIFIVE_SPI_DELAY1_INTERXFR(0));

	 
	sifive_spi_write(spi, SIFIVE_SPI_REG_FCTRL, 0);
}

static int
sifive_spi_prepare_message(struct spi_controller *host, struct spi_message *msg)
{
	struct sifive_spi *spi = spi_controller_get_devdata(host);
	struct spi_device *device = msg->spi;

	 
	if (device->mode & SPI_CS_HIGH)
		spi->cs_inactive &= ~BIT(spi_get_chipselect(device, 0));
	else
		spi->cs_inactive |= BIT(spi_get_chipselect(device, 0));
	sifive_spi_write(spi, SIFIVE_SPI_REG_CSDEF, spi->cs_inactive);

	 
	sifive_spi_write(spi, SIFIVE_SPI_REG_CSID, spi_get_chipselect(device, 0));

	 
	sifive_spi_write(spi, SIFIVE_SPI_REG_SCKMODE,
			 device->mode & SIFIVE_SPI_SCKMODE_MODE_MASK);

	return 0;
}

static void sifive_spi_set_cs(struct spi_device *device, bool is_high)
{
	struct sifive_spi *spi = spi_controller_get_devdata(device->controller);

	 
	if (device->mode & SPI_CS_HIGH)
		is_high = !is_high;

	sifive_spi_write(spi, SIFIVE_SPI_REG_CSMODE, is_high ?
			 SIFIVE_SPI_CSMODE_MODE_AUTO :
			 SIFIVE_SPI_CSMODE_MODE_HOLD);
}

static int
sifive_spi_prep_transfer(struct sifive_spi *spi, struct spi_device *device,
			 struct spi_transfer *t)
{
	u32 cr;
	unsigned int mode;

	 
	cr = DIV_ROUND_UP(clk_get_rate(spi->clk) >> 1, t->speed_hz) - 1;
	cr &= SIFIVE_SPI_SCKDIV_DIV_MASK;
	sifive_spi_write(spi, SIFIVE_SPI_REG_SCKDIV, cr);

	mode = max_t(unsigned int, t->rx_nbits, t->tx_nbits);

	 
	cr = SIFIVE_SPI_FMT_LEN(t->bits_per_word);
	switch (mode) {
	case SPI_NBITS_QUAD:
		cr |= SIFIVE_SPI_FMT_PROTO_QUAD;
		break;
	case SPI_NBITS_DUAL:
		cr |= SIFIVE_SPI_FMT_PROTO_DUAL;
		break;
	default:
		cr |= SIFIVE_SPI_FMT_PROTO_SINGLE;
		break;
	}
	if (device->mode & SPI_LSB_FIRST)
		cr |= SIFIVE_SPI_FMT_ENDIAN;
	if (!t->rx_buf)
		cr |= SIFIVE_SPI_FMT_DIR;
	sifive_spi_write(spi, SIFIVE_SPI_REG_FMT, cr);

	 
	return 1600000 * spi->fifo_depth <= t->speed_hz * mode;
}

static irqreturn_t sifive_spi_irq(int irq, void *dev_id)
{
	struct sifive_spi *spi = dev_id;
	u32 ip = sifive_spi_read(spi, SIFIVE_SPI_REG_IP);

	if (ip & (SIFIVE_SPI_IP_TXWM | SIFIVE_SPI_IP_RXWM)) {
		 
		sifive_spi_write(spi, SIFIVE_SPI_REG_IE, 0);
		complete(&spi->done);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static void sifive_spi_wait(struct sifive_spi *spi, u32 bit, int poll)
{
	if (poll) {
		u32 cr;

		do {
			cr = sifive_spi_read(spi, SIFIVE_SPI_REG_IP);
		} while (!(cr & bit));
	} else {
		reinit_completion(&spi->done);
		sifive_spi_write(spi, SIFIVE_SPI_REG_IE, bit);
		wait_for_completion(&spi->done);
	}
}

static void sifive_spi_tx(struct sifive_spi *spi, const u8 *tx_ptr)
{
	WARN_ON_ONCE((sifive_spi_read(spi, SIFIVE_SPI_REG_TXDATA)
				& SIFIVE_SPI_TXDATA_FULL) != 0);
	sifive_spi_write(spi, SIFIVE_SPI_REG_TXDATA,
			 *tx_ptr & SIFIVE_SPI_TXDATA_DATA_MASK);
}

static void sifive_spi_rx(struct sifive_spi *spi, u8 *rx_ptr)
{
	u32 data = sifive_spi_read(spi, SIFIVE_SPI_REG_RXDATA);

	WARN_ON_ONCE((data & SIFIVE_SPI_RXDATA_EMPTY) != 0);
	*rx_ptr = data & SIFIVE_SPI_RXDATA_DATA_MASK;
}

static int
sifive_spi_transfer_one(struct spi_controller *host, struct spi_device *device,
			struct spi_transfer *t)
{
	struct sifive_spi *spi = spi_controller_get_devdata(host);
	int poll = sifive_spi_prep_transfer(spi, device, t);
	const u8 *tx_ptr = t->tx_buf;
	u8 *rx_ptr = t->rx_buf;
	unsigned int remaining_words = t->len;

	while (remaining_words) {
		unsigned int n_words = min(remaining_words, spi->fifo_depth);
		unsigned int i;

		 
		for (i = 0; i < n_words; i++)
			sifive_spi_tx(spi, tx_ptr++);

		if (rx_ptr) {
			 
			sifive_spi_write(spi, SIFIVE_SPI_REG_RXMARK,
					 n_words - 1);
			sifive_spi_wait(spi, SIFIVE_SPI_IP_RXWM, poll);

			 
			for (i = 0; i < n_words; i++)
				sifive_spi_rx(spi, rx_ptr++);
		} else {
			 
			sifive_spi_wait(spi, SIFIVE_SPI_IP_TXWM, poll);
		}

		remaining_words -= n_words;
	}

	return 0;
}

static int sifive_spi_probe(struct platform_device *pdev)
{
	struct sifive_spi *spi;
	int ret, irq, num_cs;
	u32 cs_bits, max_bits_per_word;
	struct spi_controller *host;

	host = spi_alloc_host(&pdev->dev, sizeof(struct sifive_spi));
	if (!host) {
		dev_err(&pdev->dev, "out of memory\n");
		return -ENOMEM;
	}

	spi = spi_controller_get_devdata(host);
	init_completion(&spi->done);
	platform_set_drvdata(pdev, host);

	spi->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(spi->regs)) {
		ret = PTR_ERR(spi->regs);
		goto put_host;
	}

	spi->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(spi->clk)) {
		dev_err(&pdev->dev, "Unable to find bus clock\n");
		ret = PTR_ERR(spi->clk);
		goto put_host;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto put_host;
	}

	 
	ret =
	  of_property_read_u32(pdev->dev.of_node, "sifive,fifo-depth",
			       &spi->fifo_depth);
	if (ret < 0)
		spi->fifo_depth = SIFIVE_SPI_DEFAULT_DEPTH;

	ret =
	  of_property_read_u32(pdev->dev.of_node, "sifive,max-bits-per-word",
			       &max_bits_per_word);

	if (!ret && max_bits_per_word < 8) {
		dev_err(&pdev->dev, "Only 8bit SPI words supported by the driver\n");
		ret = -EINVAL;
		goto put_host;
	}

	 
	ret = clk_prepare_enable(spi->clk);
	if (ret) {
		dev_err(&pdev->dev, "Unable to enable bus clock\n");
		goto put_host;
	}

	 
	spi->cs_inactive = sifive_spi_read(spi, SIFIVE_SPI_REG_CSDEF);
	sifive_spi_write(spi, SIFIVE_SPI_REG_CSDEF, 0xffffffffU);
	cs_bits = sifive_spi_read(spi, SIFIVE_SPI_REG_CSDEF);
	sifive_spi_write(spi, SIFIVE_SPI_REG_CSDEF, spi->cs_inactive);
	if (!cs_bits) {
		dev_err(&pdev->dev, "Could not auto probe CS lines\n");
		ret = -EINVAL;
		goto disable_clk;
	}

	num_cs = ilog2(cs_bits) + 1;
	if (num_cs > SIFIVE_SPI_MAX_CS) {
		dev_err(&pdev->dev, "Invalid number of spi targets\n");
		ret = -EINVAL;
		goto disable_clk;
	}

	 
	host->dev.of_node = pdev->dev.of_node;
	host->bus_num = pdev->id;
	host->num_chipselect = num_cs;
	host->mode_bits = SPI_CPHA | SPI_CPOL
			  | SPI_CS_HIGH | SPI_LSB_FIRST
			  | SPI_TX_DUAL | SPI_TX_QUAD
			  | SPI_RX_DUAL | SPI_RX_QUAD;
	 
	host->bits_per_word_mask = SPI_BPW_MASK(8);
	host->flags = SPI_CONTROLLER_MUST_TX | SPI_CONTROLLER_GPIO_SS;
	host->prepare_message = sifive_spi_prepare_message;
	host->set_cs = sifive_spi_set_cs;
	host->transfer_one = sifive_spi_transfer_one;

	pdev->dev.dma_mask = NULL;
	 
	sifive_spi_init(spi);

	 
	ret = devm_request_irq(&pdev->dev, irq, sifive_spi_irq, 0,
			       dev_name(&pdev->dev), spi);
	if (ret) {
		dev_err(&pdev->dev, "Unable to bind to interrupt\n");
		goto disable_clk;
	}

	dev_info(&pdev->dev, "mapped; irq=%d, cs=%d\n",
		 irq, host->num_chipselect);

	ret = devm_spi_register_controller(&pdev->dev, host);
	if (ret < 0) {
		dev_err(&pdev->dev, "spi_register_host failed\n");
		goto disable_clk;
	}

	return 0;

disable_clk:
	clk_disable_unprepare(spi->clk);
put_host:
	spi_controller_put(host);

	return ret;
}

static void sifive_spi_remove(struct platform_device *pdev)
{
	struct spi_controller *host = platform_get_drvdata(pdev);
	struct sifive_spi *spi = spi_controller_get_devdata(host);

	 
	sifive_spi_write(spi, SIFIVE_SPI_REG_IE, 0);
	clk_disable_unprepare(spi->clk);
}

static int sifive_spi_suspend(struct device *dev)
{
	struct spi_controller *host = dev_get_drvdata(dev);
	struct sifive_spi *spi = spi_controller_get_devdata(host);
	int ret;

	ret = spi_controller_suspend(host);
	if (ret)
		return ret;

	 
	sifive_spi_write(spi, SIFIVE_SPI_REG_IE, 0);

	clk_disable_unprepare(spi->clk);

	return ret;
}

static int sifive_spi_resume(struct device *dev)
{
	struct spi_controller *host = dev_get_drvdata(dev);
	struct sifive_spi *spi = spi_controller_get_devdata(host);
	int ret;

	ret = clk_prepare_enable(spi->clk);
	if (ret)
		return ret;
	ret = spi_controller_resume(host);
	if (ret)
		clk_disable_unprepare(spi->clk);

	return ret;
}

static DEFINE_SIMPLE_DEV_PM_OPS(sifive_spi_pm_ops,
				sifive_spi_suspend, sifive_spi_resume);


static const struct of_device_id sifive_spi_of_match[] = {
	{ .compatible = "sifive,spi0", },
	{}
};
MODULE_DEVICE_TABLE(of, sifive_spi_of_match);

static struct platform_driver sifive_spi_driver = {
	.probe = sifive_spi_probe,
	.remove_new = sifive_spi_remove,
	.driver = {
		.name = SIFIVE_SPI_DRIVER_NAME,
		.pm = &sifive_spi_pm_ops,
		.of_match_table = sifive_spi_of_match,
	},
};
module_platform_driver(sifive_spi_driver);

MODULE_AUTHOR("SiFive, Inc. <sifive@sifive.com>");
MODULE_DESCRIPTION("SiFive SPI driver");
MODULE_LICENSE("GPL");
