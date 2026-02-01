 
 

#ifndef SKL_SST_CLDMA_H_
#define SKL_SST_CLDMA_H_

#define FW_CL_STREAM_NUMBER		0x1

#define DMA_ADDRESS_128_BITS_ALIGNMENT	7
#define BDL_ALIGN(x)			(x >> DMA_ADDRESS_128_BITS_ALIGNMENT)

#define SKL_ADSPIC_CL_DMA			0x2
#define SKL_ADSPIS_CL_DMA			0x2
#define SKL_CL_DMA_SD_INT_DESC_ERR		0x10  
#define SKL_CL_DMA_SD_INT_FIFO_ERR		0x08  
#define SKL_CL_DMA_SD_INT_COMPLETE		0x04  

 

#define HDA_ADSP_LOADER_BASE		0x80

 
#define SKL_ADSP_REG_CL_SD_CTL			(HDA_ADSP_LOADER_BASE + 0x00)
#define SKL_ADSP_REG_CL_SD_STS			(HDA_ADSP_LOADER_BASE + 0x03)
#define SKL_ADSP_REG_CL_SD_LPIB			(HDA_ADSP_LOADER_BASE + 0x04)
#define SKL_ADSP_REG_CL_SD_CBL			(HDA_ADSP_LOADER_BASE + 0x08)
#define SKL_ADSP_REG_CL_SD_LVI			(HDA_ADSP_LOADER_BASE + 0x0c)
#define SKL_ADSP_REG_CL_SD_FIFOW		(HDA_ADSP_LOADER_BASE + 0x0e)
#define SKL_ADSP_REG_CL_SD_FIFOSIZE		(HDA_ADSP_LOADER_BASE + 0x10)
#define SKL_ADSP_REG_CL_SD_FORMAT		(HDA_ADSP_LOADER_BASE + 0x12)
#define SKL_ADSP_REG_CL_SD_FIFOL		(HDA_ADSP_LOADER_BASE + 0x14)
#define SKL_ADSP_REG_CL_SD_BDLPL		(HDA_ADSP_LOADER_BASE + 0x18)
#define SKL_ADSP_REG_CL_SD_BDLPU		(HDA_ADSP_LOADER_BASE + 0x1c)

 
#define SKL_ADSP_REG_CL_SPBFIFO			(HDA_ADSP_LOADER_BASE + 0x20)
#define SKL_ADSP_REG_CL_SPBFIFO_SPBFCH		(SKL_ADSP_REG_CL_SPBFIFO + 0x0)
#define SKL_ADSP_REG_CL_SPBFIFO_SPBFCCTL	(SKL_ADSP_REG_CL_SPBFIFO + 0x4)
#define SKL_ADSP_REG_CL_SPBFIFO_SPIB		(SKL_ADSP_REG_CL_SPBFIFO + 0x8)
#define SKL_ADSP_REG_CL_SPBFIFO_MAXFIFOS	(SKL_ADSP_REG_CL_SPBFIFO + 0xc)

 

 
#define CL_SD_CTL_SRST_SHIFT		0
#define CL_SD_CTL_SRST_MASK		(1 << CL_SD_CTL_SRST_SHIFT)
#define CL_SD_CTL_SRST(x)		\
			((x << CL_SD_CTL_SRST_SHIFT) & CL_SD_CTL_SRST_MASK)

 
#define CL_SD_CTL_RUN_SHIFT		1
#define CL_SD_CTL_RUN_MASK		(1 << CL_SD_CTL_RUN_SHIFT)
#define CL_SD_CTL_RUN(x)		\
			((x << CL_SD_CTL_RUN_SHIFT) & CL_SD_CTL_RUN_MASK)

 
#define CL_SD_CTL_IOCE_SHIFT		2
#define CL_SD_CTL_IOCE_MASK		(1 << CL_SD_CTL_IOCE_SHIFT)
#define CL_SD_CTL_IOCE(x)		\
			((x << CL_SD_CTL_IOCE_SHIFT) & CL_SD_CTL_IOCE_MASK)

 
#define CL_SD_CTL_FEIE_SHIFT		3
#define CL_SD_CTL_FEIE_MASK		(1 << CL_SD_CTL_FEIE_SHIFT)
#define CL_SD_CTL_FEIE(x)		\
			((x << CL_SD_CTL_FEIE_SHIFT) & CL_SD_CTL_FEIE_MASK)

 
#define CL_SD_CTL_DEIE_SHIFT		4
#define CL_SD_CTL_DEIE_MASK		(1 << CL_SD_CTL_DEIE_SHIFT)
#define CL_SD_CTL_DEIE(x)		\
			((x << CL_SD_CTL_DEIE_SHIFT) & CL_SD_CTL_DEIE_MASK)

 
#define CL_SD_CTL_FIFOLC_SHIFT		5
#define CL_SD_CTL_FIFOLC_MASK		(1 << CL_SD_CTL_FIFOLC_SHIFT)
#define CL_SD_CTL_FIFOLC(x)		\
			((x << CL_SD_CTL_FIFOLC_SHIFT) & CL_SD_CTL_FIFOLC_MASK)

 
#define CL_SD_CTL_STRIPE_SHIFT		16
#define CL_SD_CTL_STRIPE_MASK		(0x3 << CL_SD_CTL_STRIPE_SHIFT)
#define CL_SD_CTL_STRIPE(x)		\
			((x << CL_SD_CTL_STRIPE_SHIFT) & CL_SD_CTL_STRIPE_MASK)

 
#define CL_SD_CTL_TP_SHIFT		18
#define CL_SD_CTL_TP_MASK		(1 << CL_SD_CTL_TP_SHIFT)
#define CL_SD_CTL_TP(x)			\
			((x << CL_SD_CTL_TP_SHIFT) & CL_SD_CTL_TP_MASK)

 
#define CL_SD_CTL_DIR_SHIFT		19
#define CL_SD_CTL_DIR_MASK		(1 << CL_SD_CTL_DIR_SHIFT)
#define CL_SD_CTL_DIR(x)		\
			((x << CL_SD_CTL_DIR_SHIFT) & CL_SD_CTL_DIR_MASK)

 
#define CL_SD_CTL_STRM_SHIFT		20
#define CL_SD_CTL_STRM_MASK		(0xf << CL_SD_CTL_STRM_SHIFT)
#define CL_SD_CTL_STRM(x)		\
			((x << CL_SD_CTL_STRM_SHIFT) & CL_SD_CTL_STRM_MASK)

 

 
#define CL_SD_STS_BCIS(x)		CL_SD_CTL_IOCE(x)

 
#define CL_SD_STS_FIFOE(x)		CL_SD_CTL_FEIE(x)

 
#define CL_SD_STS_DESE(x)		CL_SD_CTL_DEIE(x)

 
#define CL_SD_STS_FIFORDY(x)	CL_SD_CTL_FIFOLC(x)


 
#define CL_SD_LVI_SHIFT			0
#define CL_SD_LVI_MASK			(0xff << CL_SD_LVI_SHIFT)
#define CL_SD_LVI(x)			((x << CL_SD_LVI_SHIFT) & CL_SD_LVI_MASK)

 
#define CL_SD_FIFOW_SHIFT		0
#define CL_SD_FIFOW_MASK		(0x7 << CL_SD_FIFOW_SHIFT)
#define CL_SD_FIFOW(x)			\
			((x << CL_SD_FIFOW_SHIFT) & CL_SD_FIFOW_MASK)

 

 
#define CL_SD_BDLPLBA_PROT_SHIFT	0
#define CL_SD_BDLPLBA_PROT_MASK		(1 << CL_SD_BDLPLBA_PROT_SHIFT)
#define CL_SD_BDLPLBA_PROT(x)		\
		((x << CL_SD_BDLPLBA_PROT_SHIFT) & CL_SD_BDLPLBA_PROT_MASK)

 
#define CL_SD_BDLPLBA_SHIFT		7
#define CL_SD_BDLPLBA_MASK		(0x1ffffff << CL_SD_BDLPLBA_SHIFT)
#define CL_SD_BDLPLBA(x)		\
	((BDL_ALIGN(lower_32_bits(x)) << CL_SD_BDLPLBA_SHIFT) & CL_SD_BDLPLBA_MASK)

 
#define CL_SD_BDLPUBA_SHIFT		0
#define CL_SD_BDLPUBA_MASK		(0xffffffff << CL_SD_BDLPUBA_SHIFT)
#define CL_SD_BDLPUBA(x)		\
		((upper_32_bits(x) << CL_SD_BDLPUBA_SHIFT) & CL_SD_BDLPUBA_MASK)

 

 
#define CL_SPBFIFO_SPBFCH_PTR_SHIFT	0
#define CL_SPBFIFO_SPBFCH_PTR_MASK	(0xff << CL_SPBFIFO_SPBFCH_PTR_SHIFT)
#define CL_SPBFIFO_SPBFCH_PTR(x)	\
		((x << CL_SPBFIFO_SPBFCH_PTR_SHIFT) & CL_SPBFIFO_SPBFCH_PTR_MASK)

 
#define CL_SPBFIFO_SPBFCH_ID_SHIFT	16
#define CL_SPBFIFO_SPBFCH_ID_MASK	(0xfff << CL_SPBFIFO_SPBFCH_ID_SHIFT)
#define CL_SPBFIFO_SPBFCH_ID(x)		\
		((x << CL_SPBFIFO_SPBFCH_ID_SHIFT) & CL_SPBFIFO_SPBFCH_ID_MASK)

 
#define CL_SPBFIFO_SPBFCH_VER_SHIFT	28
#define CL_SPBFIFO_SPBFCH_VER_MASK	(0xf << CL_SPBFIFO_SPBFCH_VER_SHIFT)
#define CL_SPBFIFO_SPBFCH_VER(x)	\
	((x << CL_SPBFIFO_SPBFCH_VER_SHIFT) & CL_SPBFIFO_SPBFCH_VER_MASK)

 
#define CL_SPBFIFO_SPBFCCTL_SPIBE_SHIFT	0
#define CL_SPBFIFO_SPBFCCTL_SPIBE_MASK	(1 << CL_SPBFIFO_SPBFCCTL_SPIBE_SHIFT)
#define CL_SPBFIFO_SPBFCCTL_SPIBE(x)	\
	((x << CL_SPBFIFO_SPBFCCTL_SPIBE_SHIFT) & CL_SPBFIFO_SPBFCCTL_SPIBE_MASK)

 
#define SKL_WAIT_TIMEOUT		500	 
#define SKL_MAX_BUFFER_SIZE		(32 * PAGE_SIZE)

enum skl_cl_dma_wake_states {
	SKL_CL_DMA_STATUS_NONE = 0,
	SKL_CL_DMA_BUF_COMPLETE,
	SKL_CL_DMA_ERR,	 
};

struct sst_dsp;

struct skl_cl_dev_ops {
	void (*cl_setup_bdle)(struct sst_dsp *ctx,
			struct snd_dma_buffer *dmab_data,
			__le32 **bdlp, int size, int with_ioc);
	void (*cl_setup_controller)(struct sst_dsp *ctx,
			struct snd_dma_buffer *dmab_bdl,
			unsigned int max_size, u32 page_count);
	void (*cl_setup_spb)(struct sst_dsp  *ctx,
			unsigned int size, bool enable);
	void (*cl_cleanup_spb)(struct sst_dsp  *ctx);
	void (*cl_trigger)(struct sst_dsp  *ctx, bool enable);
	void (*cl_cleanup_controller)(struct sst_dsp  *ctx);
	int (*cl_copy_to_dmabuf)(struct sst_dsp *ctx,
			const void *bin, u32 size, bool wait);
	void (*cl_stop_dma)(struct sst_dsp *ctx);
};

 
struct skl_cl_dev {
	struct snd_dma_buffer dmab_data;
	struct snd_dma_buffer dmab_bdl;

	unsigned int bufsize;
	unsigned int frags;

	unsigned int curr_spib_pos;
	unsigned int dma_buffer_offset;
	struct skl_cl_dev_ops ops;

	wait_queue_head_t wait_queue;
	int wake_status;
	bool wait_condition;
};

#endif  
