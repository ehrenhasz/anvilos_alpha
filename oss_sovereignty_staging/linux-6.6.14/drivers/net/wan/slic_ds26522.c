
 

#include <linux/bitrev.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/spi/spi.h>
#include <linux/wait.h>
#include <linux/param.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include "slic_ds26522.h"

#define SLIC_TRANS_LEN 1
#define SLIC_TWO_LEN 2
#define SLIC_THREE_LEN 3

static struct spi_device *g_spi;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhao Qiang<B45475@freescale.com>");

 
static void slic_write(struct spi_device *spi, u16 addr,
		       u8 data)
{
	u8 temp[3];

	addr = bitrev16(addr) >> 1;
	data = bitrev8(data);
	temp[0] = (u8)((addr >> 8) & 0x7f);
	temp[1] = (u8)(addr & 0xfe);
	temp[2] = data;

	 
	spi_write(spi, &temp[0], SLIC_THREE_LEN);
}

static u8 slic_read(struct spi_device *spi, u16 addr)
{
	u8 temp[2];
	u8 data;

	addr = bitrev16(addr) >> 1;
	temp[0] = (u8)(((addr >> 8) & 0x7f) | 0x80);
	temp[1] = (u8)(addr & 0xfe);

	spi_write_then_read(spi, &temp[0], SLIC_TWO_LEN, &data,
			    SLIC_TRANS_LEN);

	data = bitrev8(data);
	return data;
}

static bool get_slic_product_code(struct spi_device *spi)
{
	u8 device_id;

	device_id = slic_read(spi, DS26522_IDR_ADDR);
	if ((device_id & 0xf8) == 0x68)
		return true;
	else
		return false;
}

static void ds26522_e1_spec_config(struct spi_device *spi)
{
	 
	slic_write(spi, DS26522_RMMR_ADDR, DS26522_RMMR_E1);

	 
	slic_write(spi, DS26522_TMMR_ADDR, DS26522_TMMR_E1);

	 
	slic_write(spi, DS26522_RMMR_ADDR,
		   slic_read(spi, DS26522_RMMR_ADDR) | DS26522_RMMR_FRM_EN);

	 
	slic_write(spi, DS26522_TMMR_ADDR,
		   slic_read(spi, DS26522_TMMR_ADDR) | DS26522_TMMR_FRM_EN);

	 
	slic_write(spi, DS26522_RCR1_ADDR,
		   DS26522_RCR1_E1_HDB3 | DS26522_RCR1_E1_CCS);

	 
	slic_write(spi, DS26522_RIOCR_ADDR,
		   DS26522_RIOCR_2048KHZ | DS26522_RIOCR_RSIO_OUT);

	 
	slic_write(spi, DS26522_TCR1_ADDR, DS26522_TCR1_TB8ZS);

	 
	slic_write(spi, DS26522_TIOCR_ADDR,
		   DS26522_TIOCR_2048KHZ | DS26522_TIOCR_TSIO_OUT);

	 
	slic_write(spi, DS26522_E1TAF_ADDR, DS26522_E1TAF_DEFAULT);

	 
	slic_write(spi, DS26522_E1TNAF_ADDR, DS26522_E1TNAF_DEFAULT);

	 
	slic_write(spi, DS26522_RMMR_ADDR, slic_read(spi, DS26522_RMMR_ADDR) |
		   DS26522_RMMR_INIT_DONE);

	 
	slic_write(spi, DS26522_TMMR_ADDR, slic_read(spi, DS26522_TMMR_ADDR) |
		   DS26522_TMMR_INIT_DONE);

	 
	slic_write(spi, DS26522_LTRCR_ADDR, DS26522_LTRCR_E1);

	 
	slic_write(spi, DS26522_LTITSR_ADDR,
		   DS26522_LTITSR_TLIS_75OHM | DS26522_LTITSR_LBOS_75OHM);

	 
	slic_write(spi, DS26522_LRISMR_ADDR,
		   DS26522_LRISMR_75OHM | DS26522_LRISMR_MAX);

	 
	slic_write(spi, DS26522_LMCR_ADDR, DS26522_LMCR_TE);
}

static int slic_ds26522_init_configure(struct spi_device *spi)
{
	u16 addr;

	 
	slic_write(spi, DS26522_GTCCR_ADDR, DS26522_GTCCR_BPREFSEL_REFCLKIN |
			DS26522_GTCCR_BFREQSEL_2048KHZ |
			DS26522_GTCCR_FREQSEL_2048KHZ);
	slic_write(spi, DS26522_GTCR2_ADDR, DS26522_GTCR2_TSSYNCOUT);
	slic_write(spi, DS26522_GFCR_ADDR, DS26522_GFCR_BPCLK_2048KHZ);

	 
	slic_write(spi, DS26522_GTCR1_ADDR, DS26522_GTCR1);

	 
	slic_write(spi, DS26522_GLSRR_ADDR, DS26522_GLSRR_RESET);

	 
	slic_write(spi, DS26522_GFSRR_ADDR, DS26522_GFSRR_RESET);

	usleep_range(100, 120);

	slic_write(spi, DS26522_GLSRR_ADDR, DS26522_GLSRR_NORMAL);
	slic_write(spi, DS26522_GFSRR_ADDR, DS26522_GFSRR_NORMAL);

	 
	slic_write(spi, DS26522_RMMR_ADDR, DS26522_RMMR_SFTRST);

	 
	slic_write(spi, DS26522_TMMR_ADDR, DS26522_TMMR_SFTRST);

	usleep_range(100, 120);

	 
	for (addr = DS26522_RF_ADDR_START; addr <= DS26522_RF_ADDR_END;
	     addr++)
		slic_write(spi, addr, 0);

	for (addr = DS26522_TF_ADDR_START; addr <= DS26522_TF_ADDR_END;
	     addr++)
		slic_write(spi, addr, 0);

	for (addr = DS26522_LIU_ADDR_START; addr <= DS26522_LIU_ADDR_END;
	     addr++)
		slic_write(spi, addr, 0);

	for (addr = DS26522_BERT_ADDR_START; addr <= DS26522_BERT_ADDR_END;
	     addr++)
		slic_write(spi, addr, 0);

	 
	ds26522_e1_spec_config(spi);

	slic_write(spi, DS26522_GTCR1_ADDR, 0x00);

	return 0;
}

static void slic_ds26522_remove(struct spi_device *spi)
{
	pr_info("DS26522 module uninstalled\n");
}

static int slic_ds26522_probe(struct spi_device *spi)
{
	int ret = 0;

	g_spi = spi;
	spi->bits_per_word = 8;

	if (!get_slic_product_code(spi))
		return ret;

	ret = slic_ds26522_init_configure(spi);
	if (ret == 0)
		pr_info("DS26522 cs%d configured\n", spi_get_chipselect(spi, 0));

	return ret;
}

static const struct spi_device_id slic_ds26522_id[] = {
	{ .name = "ds26522" },
	{   },
};
MODULE_DEVICE_TABLE(spi, slic_ds26522_id);

static const struct of_device_id slic_ds26522_match[] = {
	{
	 .compatible = "maxim,ds26522",
	 },
	{},
};
MODULE_DEVICE_TABLE(of, slic_ds26522_match);

static struct spi_driver slic_ds26522_driver = {
	.driver = {
		   .name = "ds26522",
		   .bus = &spi_bus_type,
		   .of_match_table = slic_ds26522_match,
		   },
	.probe = slic_ds26522_probe,
	.remove = slic_ds26522_remove,
	.id_table = slic_ds26522_id,
};

module_spi_driver(slic_ds26522_driver);
