

#ifndef __DRIVERS_MTD_NAND_GPMI_NAND_H
#define __DRIVERS_MTD_NAND_GPMI_NAND_H

#include <linux/mtd/rawnand.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>

#define GPMI_CLK_MAX 5 
struct resources {
	void __iomem  *gpmi_regs;
	void __iomem  *bch_regs;
	unsigned int  dma_low_channel;
	unsigned int  dma_high_channel;
	struct clk    *clock[GPMI_CLK_MAX];
};


struct bch_geometry {
	unsigned int  gf_len;
	unsigned int  ecc_strength;
	unsigned int  page_size;
	unsigned int  metadata_size;
	unsigned int  ecc0_chunk_size;
	unsigned int  eccn_chunk_size;
	unsigned int  ecc_chunk_count;
	unsigned int  payload_size;
	unsigned int  auxiliary_size;
	unsigned int  auxiliary_status_offset;
	unsigned int  block_mark_byte_offset;
	unsigned int  block_mark_bit_offset;
	unsigned int  ecc_for_meta; 
};


struct boot_rom_geometry {
	unsigned int  stride_size_in_pages;
	unsigned int  search_area_stride_exponent;
};

enum gpmi_type {
	IS_MX23,
	IS_MX28,
	IS_MX6Q,
	IS_MX6SX,
	IS_MX7D,
};

struct gpmi_devdata {
	enum gpmi_type type;
	int bch_max_ecc_strength;
	int max_chain_delay; 
	const char * const *clks;
	const int clks_count;
};


struct gpmi_nfc_hardware_timing {
	bool must_apply_timings;
	unsigned long int clk_rate;
	u32 timing0;
	u32 timing1;
	u32 ctrl1n;
};

#define GPMI_MAX_TRANSFERS	8

struct gpmi_transfer {
	u8 cmdbuf[8];
	struct scatterlist sgl;
	enum dma_data_direction direction;
};

struct gpmi_nand_data {
	
	const struct gpmi_devdata *devdata;

	
	struct device		*dev;
	struct platform_device	*pdev;

	
	struct resources	resources;

	
	struct gpmi_nfc_hardware_timing hw;

	
	struct bch_geometry	bch_geometry;
	struct completion	bch_done;

	
	bool			swap_block_mark;
	struct boot_rom_geometry rom_geometry;

	
	struct nand_controller	base;
	struct nand_chip	nand;

	struct gpmi_transfer	transfers[GPMI_MAX_TRANSFERS];
	int			ntransfers;

	bool			bch;
	uint32_t		bch_flashlayout0;
	uint32_t		bch_flashlayout1;

	char			*data_buffer_dma;

	void			*auxiliary_virt;
	dma_addr_t		auxiliary_phys;

	void			*raw_buffer;

	
#define DMA_CHANS		8
	struct dma_chan		*dma_chans[DMA_CHANS];
	struct completion	dma_done;
};


#define STATUS_GOOD		0x00
#define STATUS_ERASED		0xff
#define STATUS_UNCORRECTABLE	0xfe


#define GPMI_IS_MX23(x)		((x)->devdata->type == IS_MX23)
#define GPMI_IS_MX28(x)		((x)->devdata->type == IS_MX28)
#define GPMI_IS_MX6Q(x)		((x)->devdata->type == IS_MX6Q)
#define GPMI_IS_MX6SX(x)	((x)->devdata->type == IS_MX6SX)
#define GPMI_IS_MX7D(x)		((x)->devdata->type == IS_MX7D)

#define GPMI_IS_MX6(x)		(GPMI_IS_MX6Q(x) || GPMI_IS_MX6SX(x) || \
				 GPMI_IS_MX7D(x))
#define GPMI_IS_MXS(x)		(GPMI_IS_MX23(x) || GPMI_IS_MX28(x))
#endif
