 

#define FSL_RE_MAX_CHANS		4
#define FSL_RE_DPAA_MODE		BIT(30)
#define FSL_RE_NON_DPAA_MODE		BIT(31)
#define FSL_RE_GFM_POLY			0x1d000000
#define FSL_RE_ADD_JOB(x)		((x) << 16)
#define FSL_RE_RMVD_JOB(x)		((x) << 16)
#define FSL_RE_CFG1_CBSI		0x08000000
#define FSL_RE_CFG1_CBS0		0x00080000
#define FSL_RE_SLOT_FULL_SHIFT		8
#define FSL_RE_SLOT_FULL(x)		((x) >> FSL_RE_SLOT_FULL_SHIFT)
#define FSL_RE_SLOT_AVAIL_SHIFT		8
#define FSL_RE_SLOT_AVAIL(x)		((x) >> FSL_RE_SLOT_AVAIL_SHIFT)
#define FSL_RE_PQ_OPCODE		0x1B
#define FSL_RE_XOR_OPCODE		0x1A
#define FSL_RE_MOVE_OPCODE		0x8
#define FSL_RE_FRAME_ALIGN		16
#define FSL_RE_BLOCK_SIZE		0x3  
#define FSL_RE_CACHEABLE_IO		0x0
#define FSL_RE_BUFFER_OUTPUT		0x0
#define FSL_RE_INTR_ON_ERROR		0x1
#define FSL_RE_DATA_DEP			0x1
#define FSL_RE_ENABLE_DPI		0x0
#define FSL_RE_RING_SIZE		0x400
#define FSL_RE_RING_SIZE_MASK		(FSL_RE_RING_SIZE - 1)
#define FSL_RE_RING_SIZE_SHIFT		8
#define FSL_RE_ADDR_BIT_SHIFT		4
#define FSL_RE_ADDR_BIT_MASK		(BIT(FSL_RE_ADDR_BIT_SHIFT) - 1)
#define FSL_RE_ERROR			0x40000000
#define FSL_RE_INTR			0x80000000
#define FSL_RE_CLR_INTR			0x80000000
#define FSL_RE_PAUSE			0x80000000
#define FSL_RE_ENABLE			0x80000000
#define FSL_RE_REG_LIODN_MASK		0x00000FFF

#define FSL_RE_CDB_OPCODE_MASK		0xF8000000
#define FSL_RE_CDB_OPCODE_SHIFT		27
#define FSL_RE_CDB_EXCLEN_MASK		0x03000000
#define FSL_RE_CDB_EXCLEN_SHIFT		24
#define FSL_RE_CDB_EXCLQ1_MASK		0x00F00000
#define FSL_RE_CDB_EXCLQ1_SHIFT		20
#define FSL_RE_CDB_EXCLQ2_MASK		0x000F0000
#define FSL_RE_CDB_EXCLQ2_SHIFT		16
#define FSL_RE_CDB_BLKSIZE_MASK		0x0000C000
#define FSL_RE_CDB_BLKSIZE_SHIFT	14
#define FSL_RE_CDB_CACHE_MASK		0x00003000
#define FSL_RE_CDB_CACHE_SHIFT		12
#define FSL_RE_CDB_BUFFER_MASK		0x00000800
#define FSL_RE_CDB_BUFFER_SHIFT		11
#define FSL_RE_CDB_ERROR_MASK		0x00000400
#define FSL_RE_CDB_ERROR_SHIFT		10
#define FSL_RE_CDB_NRCS_MASK		0x0000003C
#define FSL_RE_CDB_NRCS_SHIFT		6
#define FSL_RE_CDB_DEPEND_MASK		0x00000008
#define FSL_RE_CDB_DEPEND_SHIFT		3
#define FSL_RE_CDB_DPI_MASK		0x00000004
#define FSL_RE_CDB_DPI_SHIFT		2

 
#define FSL_RE_CF_DESC_SIZE		320
#define FSL_RE_CF_CDB_SIZE		512
#define FSL_RE_CF_CDB_ALIGN		64

struct fsl_re_ctrl {
	 
	__be32 global_config;	 
	u8     rsvd1[4];
	__be32 galois_field_config;  
	u8     rsvd2[4];
	__be32 jq_wrr_config;    
	u8     rsvd3[4];
	__be32 crc_config;	 
	u8     rsvd4[228];
	__be32 system_reset;	 
	u8     rsvd5[252];
	__be32 global_status;	 
	u8     rsvd6[832];
	__be32 re_liodn_base;	 
	u8     rsvd7[1712];
	__be32 re_version_id;	 
	__be32 re_version_id_2;  
	u8     rsvd8[512];
	__be32 host_config;	 
};

struct fsl_re_chan_cfg {
	 
	__be32 jr_config_0;	 
	__be32 jr_config_1;	 
	__be32 jr_interrupt_status;  
	u8     rsvd1[4];
	__be32 jr_command;	 
	u8     rsvd2[4];
	__be32 jr_status;	 
	u8     rsvd3[228];

	 
	__be32 inbring_base_h;	 
	__be32 inbring_base_l;	 
	__be32 inbring_size;	 
	u8     rsvd4[4];
	__be32 inbring_slot_avail;  
	u8     rsvd5[4];
	__be32 inbring_add_job;	 
	u8     rsvd6[4];
	__be32 inbring_cnsmr_indx;  
	u8     rsvd7[220];

	 
	__be32 oubring_base_h;	 
	__be32 oubring_base_l;	 
	__be32 oubring_size;	 
	u8     rsvd8[4];
	__be32 oubring_job_rmvd;  
	u8     rsvd9[4];
	__be32 oubring_slot_full;  
	u8     rsvd10[4];
	__be32 oubring_prdcr_indx;  
};

 
struct fsl_re_move_cdb {
	__be32 cdb32;
};

 
#define FSL_RE_DPI_APPS_MASK		0xC0000000
#define FSL_RE_DPI_APPS_SHIFT		30
#define FSL_RE_DPI_REF_MASK		0x30000000
#define FSL_RE_DPI_REF_SHIFT		28
#define FSL_RE_DPI_GUARD_MASK		0x0C000000
#define FSL_RE_DPI_GUARD_SHIFT		26
#define FSL_RE_DPI_ATTR_MASK		0x03000000
#define FSL_RE_DPI_ATTR_SHIFT		24
#define FSL_RE_DPI_META_MASK		0x0000FFFF

struct fsl_re_dpi {
	__be32 dpi32;
	__be32 ref;
};

 
struct fsl_re_xor_cdb {
	__be32 cdb32;
	u8 gfm[16];
	struct fsl_re_dpi dpi_dest_spec;
	struct fsl_re_dpi dpi_src_spec[16];
};

 
struct fsl_re_noop_cdb {
	__be32 cdb32;
};

 
struct fsl_re_pq_cdb {
	__be32 cdb32;
	u8 gfm_q1[16];
	u8 gfm_q2[16];
	struct fsl_re_dpi dpi_dest_spec[2];
	struct fsl_re_dpi dpi_src_spec[16];
};

 
#define FSL_RE_CF_ADDR_HIGH_MASK	0x000000FF
#define FSL_RE_CF_EXT_MASK		0x80000000
#define FSL_RE_CF_EXT_SHIFT		31
#define FSL_RE_CF_FINAL_MASK		0x40000000
#define FSL_RE_CF_FINAL_SHIFT		30
#define FSL_RE_CF_LENGTH_MASK		0x000FFFFF
#define FSL_RE_CF_BPID_MASK		0x00FF0000
#define FSL_RE_CF_BPID_SHIFT		16
#define FSL_RE_CF_OFFSET_MASK		0x00001FFF

struct fsl_re_cmpnd_frame {
	__be32 addr_high;
	__be32 addr_low;
	__be32 efrl32;
	__be32 rbro32;
};

 
#define FSL_RE_HWDESC_LIODN_MASK	0x3F000000
#define FSL_RE_HWDESC_LIODN_SHIFT	24
#define FSL_RE_HWDESC_BPID_MASK		0x00FF0000
#define FSL_RE_HWDESC_BPID_SHIFT	16
#define FSL_RE_HWDESC_ELIODN_MASK	0x0000F000
#define FSL_RE_HWDESC_ELIODN_SHIFT	12
#define FSL_RE_HWDESC_FMT_SHIFT		29
#define FSL_RE_HWDESC_FMT_MASK		(0x3 << FSL_RE_HWDESC_FMT_SHIFT)

struct fsl_re_hw_desc {
	__be32 lbea32;
	__be32 addr_low;
	__be32 fmt32;
	__be32 status;
};

 
struct fsl_re_drv_private {
	u8 total_chans;
	struct dma_device dma_dev;
	struct fsl_re_ctrl *re_regs;
	struct fsl_re_chan *re_jrs[FSL_RE_MAX_CHANS];
	struct dma_pool *cf_desc_pool;
	struct dma_pool *hw_desc_pool;
};

 
struct fsl_re_chan {
	char name[16];
	spinlock_t desc_lock;  
	struct list_head ack_q;   
	struct list_head active_q;  
	struct list_head submit_q;
	struct list_head free_q;  
	struct device *dev;
	struct fsl_re_drv_private *re_dev;
	struct dma_chan chan;
	struct fsl_re_chan_cfg *jrregs;
	int irq;
	struct tasklet_struct irqtask;
	u32 alloc_count;

	 
	dma_addr_t inb_phys_addr;
	struct fsl_re_hw_desc *inb_ring_virt_addr;
	u32 inb_count;

	 
	dma_addr_t oub_phys_addr;
	struct fsl_re_hw_desc *oub_ring_virt_addr;
	u32 oub_count;
};

 
struct fsl_re_desc {
	struct dma_async_tx_descriptor async_tx;
	struct list_head node;
	struct fsl_re_hw_desc hwdesc;
	struct fsl_re_chan *re_chan;

	 
	void *cf_addr;
	dma_addr_t cf_paddr;

	void *cdb_addr;
	dma_addr_t cdb_paddr;
	int status;
};
