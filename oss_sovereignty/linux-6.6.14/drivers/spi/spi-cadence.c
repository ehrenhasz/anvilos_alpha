
 

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spi.h>

 
#define CDNS_SPI_NAME		"cdns-spi"

 
#define CDNS_SPI_CR	0x00  
#define CDNS_SPI_ISR	0x04  
#define CDNS_SPI_IER	0x08  
#define CDNS_SPI_IDR	0x0c  
#define CDNS_SPI_IMR	0x10  
#define CDNS_SPI_ER	0x14  
#define CDNS_SPI_DR	0x18  
#define CDNS_SPI_TXD	0x1C  
#define CDNS_SPI_RXD	0x20  
#define CDNS_SPI_SICR	0x24  
#define CDNS_SPI_THLD	0x28  

#define SPI_AUTOSUSPEND_TIMEOUT		3000
 
#define CDNS_SPI_CR_MANSTRT	0x00010000  
#define CDNS_SPI_CR_CPHA		0x00000004  
#define CDNS_SPI_CR_CPOL		0x00000002  
#define CDNS_SPI_CR_SSCTRL		0x00003C00  
#define CDNS_SPI_CR_PERI_SEL	0x00000200  
#define CDNS_SPI_CR_BAUD_DIV	0x00000038  
#define CDNS_SPI_CR_MSTREN		0x00000001  
#define CDNS_SPI_CR_MANSTRTEN	0x00008000  
#define CDNS_SPI_CR_SSFORCE	0x00004000  
#define CDNS_SPI_CR_BAUD_DIV_4	0x00000008  
#define CDNS_SPI_CR_DEFAULT	(CDNS_SPI_CR_MSTREN | \
					CDNS_SPI_CR_SSCTRL | \
					CDNS_SPI_CR_SSFORCE | \
					CDNS_SPI_CR_BAUD_DIV_4)

 

#define CDNS_SPI_BAUD_DIV_MAX		7  
#define CDNS_SPI_BAUD_DIV_MIN		1  
#define CDNS_SPI_BAUD_DIV_SHIFT		3  
#define CDNS_SPI_SS_SHIFT		10  
#define CDNS_SPI_SS0			0x1  
#define CDNS_SPI_NOSS			0xF  

 
#define CDNS_SPI_IXR_TXOW	0x00000004  
#define CDNS_SPI_IXR_MODF	0x00000002  
#define CDNS_SPI_IXR_RXNEMTY 0x00000010  
#define CDNS_SPI_IXR_DEFAULT	(CDNS_SPI_IXR_TXOW | \
					CDNS_SPI_IXR_MODF)
#define CDNS_SPI_IXR_TXFULL	0x00000008  
#define CDNS_SPI_IXR_ALL	0x0000007F  

 
#define CDNS_SPI_ER_ENABLE	0x00000001  
#define CDNS_SPI_ER_DISABLE	0x0  

 
#define CDNS_SPI_DEFAULT_NUM_CS		4

 
struct cdns_spi {
	void __iomem *regs;
	struct clk *ref_clk;
	struct clk *pclk;
	unsigned int clk_rate;
	u32 speed_hz;
	const u8 *txbuf;
	u8 *rxbuf;
	int tx_bytes;
	int rx_bytes;
	u8 dev_busy;
	u32 is_decoded_cs;
	unsigned int tx_fifo_depth;
};

 
static inline u32 cdns_spi_read(struct cdns_spi *xspi, u32 offset)
{
	return readl_relaxed(xspi->regs + offset);
}

static inline void cdns_spi_write(struct cdns_spi *xspi, u32 offset, u32 val)
{
	writel_relaxed(val, xspi->regs + offset);
}

 
static void cdns_spi_init_hw(struct cdns_spi *xspi, bool is_target)
{
	u32 ctrl_reg = 0;

	if (!is_target)
		ctrl_reg |= CDNS_SPI_CR_DEFAULT;

	if (xspi->is_decoded_cs)
		ctrl_reg |= CDNS_SPI_CR_PERI_SEL;

	cdns_spi_write(xspi, CDNS_SPI_ER, CDNS_SPI_ER_DISABLE);
	cdns_spi_write(xspi, CDNS_SPI_IDR, CDNS_SPI_IXR_ALL);

	 
	while (cdns_spi_read(xspi, CDNS_SPI_ISR) & CDNS_SPI_IXR_RXNEMTY)
		cdns_spi_read(xspi, CDNS_SPI_RXD);

	cdns_spi_write(xspi, CDNS_SPI_ISR, CDNS_SPI_IXR_ALL);
	cdns_spi_write(xspi, CDNS_SPI_CR, ctrl_reg);
	cdns_spi_write(xspi, CDNS_SPI_ER, CDNS_SPI_ER_ENABLE);
}

 
static void cdns_spi_chipselect(struct spi_device *spi, bool is_high)
{
	struct cdns_spi *xspi = spi_controller_get_devdata(spi->controller);
	u32 ctrl_reg;

	ctrl_reg = cdns_spi_read(xspi, CDNS_SPI_CR);

	if (is_high) {
		 
		ctrl_reg |= CDNS_SPI_CR_SSCTRL;
	} else {
		 
		ctrl_reg &= ~CDNS_SPI_CR_SSCTRL;
		if (!(xspi->is_decoded_cs))
			ctrl_reg |= ((~(CDNS_SPI_SS0 << spi_get_chipselect(spi, 0))) <<
				     CDNS_SPI_SS_SHIFT) &
				     CDNS_SPI_CR_SSCTRL;
		else
			ctrl_reg |= (spi_get_chipselect(spi, 0) << CDNS_SPI_SS_SHIFT) &
				     CDNS_SPI_CR_SSCTRL;
	}

	cdns_spi_write(xspi, CDNS_SPI_CR, ctrl_reg);
}

 
static void cdns_spi_config_clock_mode(struct spi_device *spi)
{
	struct cdns_spi *xspi = spi_controller_get_devdata(spi->controller);
	u32 ctrl_reg, new_ctrl_reg;

	new_ctrl_reg = cdns_spi_read(xspi, CDNS_SPI_CR);
	ctrl_reg = new_ctrl_reg;

	 
	new_ctrl_reg &= ~(CDNS_SPI_CR_CPHA | CDNS_SPI_CR_CPOL);
	if (spi->mode & SPI_CPHA)
		new_ctrl_reg |= CDNS_SPI_CR_CPHA;
	if (spi->mode & SPI_CPOL)
		new_ctrl_reg |= CDNS_SPI_CR_CPOL;

	if (new_ctrl_reg != ctrl_reg) {
		 
		cdns_spi_write(xspi, CDNS_SPI_ER, CDNS_SPI_ER_DISABLE);
		cdns_spi_write(xspi, CDNS_SPI_CR, new_ctrl_reg);
		cdns_spi_write(xspi, CDNS_SPI_ER, CDNS_SPI_ER_ENABLE);
	}
}

 
static void cdns_spi_config_clock_freq(struct spi_device *spi,
				       struct spi_transfer *transfer)
{
	struct cdns_spi *xspi = spi_controller_get_devdata(spi->controller);
	u32 ctrl_reg, baud_rate_val;
	unsigned long frequency;

	frequency = xspi->clk_rate;

	ctrl_reg = cdns_spi_read(xspi, CDNS_SPI_CR);

	 
	if (xspi->speed_hz != transfer->speed_hz) {
		 
		baud_rate_val = CDNS_SPI_BAUD_DIV_MIN;
		while ((baud_rate_val < CDNS_SPI_BAUD_DIV_MAX) &&
		       (frequency / (2 << baud_rate_val)) > transfer->speed_hz)
			baud_rate_val++;

		ctrl_reg &= ~CDNS_SPI_CR_BAUD_DIV;
		ctrl_reg |= baud_rate_val << CDNS_SPI_BAUD_DIV_SHIFT;

		xspi->speed_hz = frequency / (2 << baud_rate_val);
	}
	cdns_spi_write(xspi, CDNS_SPI_CR, ctrl_reg);
}

 
static int cdns_spi_setup_transfer(struct spi_device *spi,
				   struct spi_transfer *transfer)
{
	struct cdns_spi *xspi = spi_controller_get_devdata(spi->controller);

	cdns_spi_config_clock_freq(spi, transfer);

	dev_dbg(&spi->dev, "%s, mode %d, %u bits/w, %u clock speed\n",
		__func__, spi->mode, spi->bits_per_word,
		xspi->speed_hz);

	return 0;
}

 
static void cdns_spi_process_fifo(struct cdns_spi *xspi, int ntx, int nrx)
{
	ntx = clamp(ntx, 0, xspi->tx_bytes);
	nrx = clamp(nrx, 0, xspi->rx_bytes);

	xspi->tx_bytes -= ntx;
	xspi->rx_bytes -= nrx;

	while (ntx || nrx) {
		if (ntx) {
			if (xspi->txbuf)
				cdns_spi_write(xspi, CDNS_SPI_TXD, *xspi->txbuf++);
			else
				cdns_spi_write(xspi, CDNS_SPI_TXD, 0);

			ntx--;
		}

		if (nrx) {
			u8 data = cdns_spi_read(xspi, CDNS_SPI_RXD);

			if (xspi->rxbuf)
				*xspi->rxbuf++ = data;

			nrx--;
		}
	}
}

 
static irqreturn_t cdns_spi_irq(int irq, void *dev_id)
{
	struct spi_controller *ctlr = dev_id;
	struct cdns_spi *xspi = spi_controller_get_devdata(ctlr);
	irqreturn_t status;
	u32 intr_status;

	status = IRQ_NONE;
	intr_status = cdns_spi_read(xspi, CDNS_SPI_ISR);
	cdns_spi_write(xspi, CDNS_SPI_ISR, intr_status);

	if (intr_status & CDNS_SPI_IXR_MODF) {
		 
		cdns_spi_write(xspi, CDNS_SPI_IDR, CDNS_SPI_IXR_DEFAULT);
		spi_finalize_current_transfer(ctlr);
		status = IRQ_HANDLED;
	} else if (intr_status & CDNS_SPI_IXR_TXOW) {
		int threshold = cdns_spi_read(xspi, CDNS_SPI_THLD);
		int trans_cnt = xspi->rx_bytes - xspi->tx_bytes;

		if (threshold > 1)
			trans_cnt -= threshold;

		 
		if (xspi->tx_bytes < xspi->tx_fifo_depth >> 1)
			cdns_spi_write(xspi, CDNS_SPI_THLD, 1);

		if (xspi->tx_bytes) {
			cdns_spi_process_fifo(xspi, trans_cnt, trans_cnt);
		} else {
			 
			udelay(10);
			cdns_spi_process_fifo(xspi, 0, trans_cnt);
			cdns_spi_write(xspi, CDNS_SPI_IDR,
				       CDNS_SPI_IXR_DEFAULT);
			spi_finalize_current_transfer(ctlr);
		}
		status = IRQ_HANDLED;
	}

	return status;
}

static int cdns_prepare_message(struct spi_controller *ctlr,
				struct spi_message *msg)
{
	if (!spi_controller_is_target(ctlr))
		cdns_spi_config_clock_mode(msg->spi);
	return 0;
}

 
static int cdns_transfer_one(struct spi_controller *ctlr,
			     struct spi_device *spi,
			     struct spi_transfer *transfer)
{
	struct cdns_spi *xspi = spi_controller_get_devdata(ctlr);

	xspi->txbuf = transfer->tx_buf;
	xspi->rxbuf = transfer->rx_buf;
	xspi->tx_bytes = transfer->len;
	xspi->rx_bytes = transfer->len;

	if (!spi_controller_is_target(ctlr)) {
		cdns_spi_setup_transfer(spi, transfer);
	} else {
		 
		if (xspi->tx_bytes > xspi->tx_fifo_depth)
			cdns_spi_write(xspi, CDNS_SPI_THLD, xspi->tx_fifo_depth >> 1);
	}

	 
	if (cdns_spi_read(xspi, CDNS_SPI_ISR) & CDNS_SPI_IXR_TXFULL)
		udelay(10);

	cdns_spi_process_fifo(xspi, xspi->tx_fifo_depth, 0);

	cdns_spi_write(xspi, CDNS_SPI_IER, CDNS_SPI_IXR_DEFAULT);
	return transfer->len;
}

 
static int cdns_prepare_transfer_hardware(struct spi_controller *ctlr)
{
	struct cdns_spi *xspi = spi_controller_get_devdata(ctlr);

	cdns_spi_write(xspi, CDNS_SPI_ER, CDNS_SPI_ER_ENABLE);

	return 0;
}

 
static int cdns_unprepare_transfer_hardware(struct spi_controller *ctlr)
{
	struct cdns_spi *xspi = spi_controller_get_devdata(ctlr);
	u32 ctrl_reg;
	unsigned int cnt = xspi->tx_fifo_depth;

	if (spi_controller_is_target(ctlr)) {
		while (cnt--)
			cdns_spi_read(xspi, CDNS_SPI_RXD);
	}

	 
	ctrl_reg = cdns_spi_read(xspi, CDNS_SPI_CR);
	ctrl_reg = (ctrl_reg & CDNS_SPI_CR_SSCTRL) >>  CDNS_SPI_SS_SHIFT;
	if (ctrl_reg == CDNS_SPI_NOSS || spi_controller_is_target(ctlr))
		cdns_spi_write(xspi, CDNS_SPI_ER, CDNS_SPI_ER_DISABLE);

	 
	cdns_spi_write(xspi, CDNS_SPI_THLD, 0x1);
	return 0;
}

 
static void cdns_spi_detect_fifo_depth(struct cdns_spi *xspi)
{
	 
	cdns_spi_write(xspi, CDNS_SPI_THLD, 0xffff);
	xspi->tx_fifo_depth = cdns_spi_read(xspi, CDNS_SPI_THLD) + 1;

	 
	cdns_spi_write(xspi, CDNS_SPI_THLD, 0x1);
}

 
static int cdns_target_abort(struct spi_controller *ctlr)
{
	struct cdns_spi *xspi = spi_controller_get_devdata(ctlr);
	u32 intr_status;

	intr_status = cdns_spi_read(xspi, CDNS_SPI_ISR);
	cdns_spi_write(xspi, CDNS_SPI_ISR, intr_status);
	cdns_spi_write(xspi, CDNS_SPI_IDR, (CDNS_SPI_IXR_MODF | CDNS_SPI_IXR_RXNEMTY));
	spi_finalize_current_transfer(ctlr);

	return 0;
}

 
static int cdns_spi_probe(struct platform_device *pdev)
{
	int ret = 0, irq;
	struct spi_controller *ctlr;
	struct cdns_spi *xspi;
	u32 num_cs;
	bool target;

	target = of_property_read_bool(pdev->dev.of_node, "spi-slave");
	if (target)
		ctlr = spi_alloc_target(&pdev->dev, sizeof(*xspi));
	else
		ctlr = spi_alloc_host(&pdev->dev, sizeof(*xspi));

	if (!ctlr)
		return -ENOMEM;

	xspi = spi_controller_get_devdata(ctlr);
	ctlr->dev.of_node = pdev->dev.of_node;
	platform_set_drvdata(pdev, ctlr);

	xspi->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(xspi->regs)) {
		ret = PTR_ERR(xspi->regs);
		goto remove_ctlr;
	}

	xspi->pclk = devm_clk_get(&pdev->dev, "pclk");
	if (IS_ERR(xspi->pclk)) {
		dev_err(&pdev->dev, "pclk clock not found.\n");
		ret = PTR_ERR(xspi->pclk);
		goto remove_ctlr;
	}

	ret = clk_prepare_enable(xspi->pclk);
	if (ret) {
		dev_err(&pdev->dev, "Unable to enable APB clock.\n");
		goto remove_ctlr;
	}

	if (!spi_controller_is_target(ctlr)) {
		xspi->ref_clk = devm_clk_get(&pdev->dev, "ref_clk");
		if (IS_ERR(xspi->ref_clk)) {
			dev_err(&pdev->dev, "ref_clk clock not found.\n");
			ret = PTR_ERR(xspi->ref_clk);
			goto clk_dis_apb;
		}

		ret = clk_prepare_enable(xspi->ref_clk);
		if (ret) {
			dev_err(&pdev->dev, "Unable to enable device clock.\n");
			goto clk_dis_apb;
		}

		pm_runtime_use_autosuspend(&pdev->dev);
		pm_runtime_set_autosuspend_delay(&pdev->dev, SPI_AUTOSUSPEND_TIMEOUT);
		pm_runtime_get_noresume(&pdev->dev);
		pm_runtime_set_active(&pdev->dev);
		pm_runtime_enable(&pdev->dev);

		ret = of_property_read_u32(pdev->dev.of_node, "num-cs", &num_cs);
		if (ret < 0)
			ctlr->num_chipselect = CDNS_SPI_DEFAULT_NUM_CS;
		else
			ctlr->num_chipselect = num_cs;

		ret = of_property_read_u32(pdev->dev.of_node, "is-decoded-cs",
					   &xspi->is_decoded_cs);
		if (ret < 0)
			xspi->is_decoded_cs = 0;
	}

	cdns_spi_detect_fifo_depth(xspi);

	 
	cdns_spi_init_hw(xspi, spi_controller_is_target(ctlr));

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto clk_dis_all;
	}

	ret = devm_request_irq(&pdev->dev, irq, cdns_spi_irq,
			       0, pdev->name, ctlr);
	if (ret != 0) {
		ret = -ENXIO;
		dev_err(&pdev->dev, "request_irq failed\n");
		goto clk_dis_all;
	}

	ctlr->use_gpio_descriptors = true;
	ctlr->prepare_transfer_hardware = cdns_prepare_transfer_hardware;
	ctlr->prepare_message = cdns_prepare_message;
	ctlr->transfer_one = cdns_transfer_one;
	ctlr->unprepare_transfer_hardware = cdns_unprepare_transfer_hardware;
	ctlr->mode_bits = SPI_CPOL | SPI_CPHA;
	ctlr->bits_per_word_mask = SPI_BPW_MASK(8);

	if (!spi_controller_is_target(ctlr)) {
		ctlr->mode_bits |=  SPI_CS_HIGH;
		ctlr->set_cs = cdns_spi_chipselect;
		ctlr->auto_runtime_pm = true;
		xspi->clk_rate = clk_get_rate(xspi->ref_clk);
		 
		ctlr->max_speed_hz = xspi->clk_rate / 4;
		xspi->speed_hz = ctlr->max_speed_hz;
		pm_runtime_mark_last_busy(&pdev->dev);
		pm_runtime_put_autosuspend(&pdev->dev);
	} else {
		ctlr->mode_bits |= SPI_NO_CS;
		ctlr->target_abort = cdns_target_abort;
	}
	ret = spi_register_controller(ctlr);
	if (ret) {
		dev_err(&pdev->dev, "spi_register_controller failed\n");
		goto clk_dis_all;
	}

	return ret;

clk_dis_all:
	if (!spi_controller_is_target(ctlr)) {
		pm_runtime_set_suspended(&pdev->dev);
		pm_runtime_disable(&pdev->dev);
		clk_disable_unprepare(xspi->ref_clk);
	}
clk_dis_apb:
	clk_disable_unprepare(xspi->pclk);
remove_ctlr:
	spi_controller_put(ctlr);
	return ret;
}

 
static void cdns_spi_remove(struct platform_device *pdev)
{
	struct spi_controller *ctlr = platform_get_drvdata(pdev);
	struct cdns_spi *xspi = spi_controller_get_devdata(ctlr);

	cdns_spi_write(xspi, CDNS_SPI_ER, CDNS_SPI_ER_DISABLE);

	clk_disable_unprepare(xspi->ref_clk);
	clk_disable_unprepare(xspi->pclk);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	spi_unregister_controller(ctlr);
}

 
static int __maybe_unused cdns_spi_suspend(struct device *dev)
{
	struct spi_controller *ctlr = dev_get_drvdata(dev);

	return spi_controller_suspend(ctlr);
}

 
static int __maybe_unused cdns_spi_resume(struct device *dev)
{
	struct spi_controller *ctlr = dev_get_drvdata(dev);
	struct cdns_spi *xspi = spi_controller_get_devdata(ctlr);

	cdns_spi_init_hw(xspi, spi_controller_is_target(ctlr));
	return spi_controller_resume(ctlr);
}

 
static int __maybe_unused cdns_spi_runtime_resume(struct device *dev)
{
	struct spi_controller *ctlr = dev_get_drvdata(dev);
	struct cdns_spi *xspi = spi_controller_get_devdata(ctlr);
	int ret;

	ret = clk_prepare_enable(xspi->pclk);
	if (ret) {
		dev_err(dev, "Cannot enable APB clock.\n");
		return ret;
	}

	ret = clk_prepare_enable(xspi->ref_clk);
	if (ret) {
		dev_err(dev, "Cannot enable device clock.\n");
		clk_disable_unprepare(xspi->pclk);
		return ret;
	}
	return 0;
}

 
static int __maybe_unused cdns_spi_runtime_suspend(struct device *dev)
{
	struct spi_controller *ctlr = dev_get_drvdata(dev);
	struct cdns_spi *xspi = spi_controller_get_devdata(ctlr);

	clk_disable_unprepare(xspi->ref_clk);
	clk_disable_unprepare(xspi->pclk);

	return 0;
}

static const struct dev_pm_ops cdns_spi_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(cdns_spi_runtime_suspend,
			   cdns_spi_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(cdns_spi_suspend, cdns_spi_resume)
};

static const struct of_device_id cdns_spi_of_match[] = {
	{ .compatible = "xlnx,zynq-spi-r1p6" },
	{ .compatible = "cdns,spi-r1p6" },
	{   }
};
MODULE_DEVICE_TABLE(of, cdns_spi_of_match);

 
static struct platform_driver cdns_spi_driver = {
	.probe	= cdns_spi_probe,
	.remove_new = cdns_spi_remove,
	.driver = {
		.name = CDNS_SPI_NAME,
		.of_match_table = cdns_spi_of_match,
		.pm = &cdns_spi_dev_pm_ops,
	},
};

module_platform_driver(cdns_spi_driver);

MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION("Cadence SPI driver");
MODULE_LICENSE("GPL");
