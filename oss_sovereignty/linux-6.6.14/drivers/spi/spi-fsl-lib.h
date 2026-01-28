#ifndef __SPI_FSL_LIB_H__
#define __SPI_FSL_LIB_H__
#include <asm/io.h>
struct mpc8xxx_spi {
	struct device *dev;
	void __iomem *reg_base;
	const void *tx;
	void *rx;
	int subblock;
	struct spi_pram __iomem *pram;
#ifdef CONFIG_FSL_SOC
	struct cpm_buf_desc __iomem *tx_bd;
	struct cpm_buf_desc __iomem *rx_bd;
#endif
	struct spi_transfer *xfer_in_progress;
	dma_addr_t tx_dma;
	dma_addr_t rx_dma;
	bool map_tx_dma;
	bool map_rx_dma;
	dma_addr_t dma_dummy_tx;
	dma_addr_t dma_dummy_rx;
	void (*get_rx) (u32 rx_data, struct mpc8xxx_spi *);
	u32(*get_tx) (struct mpc8xxx_spi *);
	unsigned int count;
	unsigned int irq;
	unsigned nsecs;		 
	u32 spibrg;		 
	u32 rx_shift;		 
	u32 tx_shift;		 
	unsigned int flags;
#if IS_ENABLED(CONFIG_SPI_FSL_SPI)
	int type;
	int native_chipselects;
	u8 max_bits_per_word;
	void (*set_shifts)(u32 *rx_shift, u32 *tx_shift,
			   int bits_per_word, int msb_first);
#endif
	struct completion done;
};
struct spi_mpc8xxx_cs {
	void (*get_rx) (u32 rx_data, struct mpc8xxx_spi *);
	u32 (*get_tx) (struct mpc8xxx_spi *);
	u32 rx_shift;		 
	u32 tx_shift;		 
	u32 hw_mode;		 
};
static inline void mpc8xxx_spi_write_reg(__be32 __iomem *reg, u32 val)
{
	iowrite32be(val, reg);
}
static inline u32 mpc8xxx_spi_read_reg(__be32 __iomem *reg)
{
	return ioread32be(reg);
}
struct mpc8xxx_spi_probe_info {
	struct fsl_spi_platform_data pdata;
	__be32 __iomem *immr_spi_cs;
};
extern u32 mpc8xxx_spi_tx_buf_u8(struct mpc8xxx_spi *mpc8xxx_spi);
extern u32 mpc8xxx_spi_tx_buf_u16(struct mpc8xxx_spi *mpc8xxx_spi);
extern u32 mpc8xxx_spi_tx_buf_u32(struct mpc8xxx_spi *mpc8xxx_spi);
extern void mpc8xxx_spi_rx_buf_u8(u32 data, struct mpc8xxx_spi *mpc8xxx_spi);
extern void mpc8xxx_spi_rx_buf_u16(u32 data, struct mpc8xxx_spi *mpc8xxx_spi);
extern void mpc8xxx_spi_rx_buf_u32(u32 data, struct mpc8xxx_spi *mpc8xxx_spi);
extern struct mpc8xxx_spi_probe_info *to_of_pinfo(
		struct fsl_spi_platform_data *pdata);
extern const char *mpc8xxx_spi_strmode(unsigned int flags);
extern void mpc8xxx_spi_probe(struct device *dev, struct resource *mem,
		unsigned int irq);
extern int of_mpc8xxx_spi_probe(struct platform_device *ofdev);
#endif  
