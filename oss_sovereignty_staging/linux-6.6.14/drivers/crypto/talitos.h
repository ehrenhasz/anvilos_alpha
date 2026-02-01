 
 

#define TALITOS_TIMEOUT 100000
#define TALITOS1_MAX_DATA_LEN 32768
#define TALITOS2_MAX_DATA_LEN 65535

#define DESC_TYPE(desc_hdr) ((be32_to_cpu(desc_hdr) >> 3) & 0x1f)
#define PRIMARY_EU(desc_hdr) ((be32_to_cpu(desc_hdr) >> 28) & 0xf)
#define SECONDARY_EU(desc_hdr) ((be32_to_cpu(desc_hdr) >> 16) & 0xf)

 
struct talitos_ptr {
	union {
		struct {		 
			__be16 len;      
			u8 j_extent;     
			u8 eptr;         
		};
		struct {			 
			__be16 res;
			__be16 len1;	 
		};
	};
	__be32 ptr;      
};

 
struct talitos_desc {
	__be32 hdr;                      
	union {
		__be32 hdr_lo;		 
		__be32 hdr1;		 
	};
	struct talitos_ptr ptr[7];       
	__be32 next_desc;		 
};

#define TALITOS_DESC_SIZE	(sizeof(struct talitos_desc) - sizeof(__be32))

 
struct talitos_edesc {
	int src_nents;
	int dst_nents;
	dma_addr_t iv_dma;
	int dma_len;
	dma_addr_t dma_link_tbl;
	struct talitos_desc desc;
	union {
		DECLARE_FLEX_ARRAY(struct talitos_ptr, link_tbl);
		DECLARE_FLEX_ARRAY(u8, buf);
	};
};

 
struct talitos_request {
	struct talitos_desc *desc;
	dma_addr_t dma_desc;
	void (*callback) (struct device *dev, struct talitos_desc *desc,
			  void *context, int error);
	void *context;
};

 
struct talitos_channel {
	void __iomem *reg;

	 
	struct talitos_request *fifo;

	 
	atomic_t submit_count ____cacheline_aligned;

	 
	spinlock_t head_lock ____cacheline_aligned;
	 
	int head;

	 
	spinlock_t tail_lock ____cacheline_aligned;
	 
	int tail;
};

struct talitos_private {
	struct device *dev;
	struct platform_device *ofdev;
	void __iomem *reg;
	void __iomem *reg_deu;
	void __iomem *reg_aesu;
	void __iomem *reg_mdeu;
	void __iomem *reg_afeu;
	void __iomem *reg_rngu;
	void __iomem *reg_pkeu;
	void __iomem *reg_keu;
	void __iomem *reg_crcu;
	int irq[2];

	 
	spinlock_t reg_lock ____cacheline_aligned;

	 
	unsigned int num_channels;
	unsigned int chfifo_len;
	unsigned int exec_units;
	unsigned int desc_types;

	 
	unsigned long features;

	 
	unsigned int fifo_len;

	struct talitos_channel *chan;

	 
	atomic_t last_chan ____cacheline_aligned;

	 
	struct tasklet_struct done_task[2];

	 
	struct list_head alg_list;

	 
	struct hwrng rng;
	bool rng_registered;
};

 
#define TALITOS_FTR_SRC_LINK_TBL_LEN_INCLUDES_EXTENT 0x00000001
#define TALITOS_FTR_HW_AUTH_CHECK 0x00000002
#define TALITOS_FTR_SHA224_HWINIT 0x00000004
#define TALITOS_FTR_HMAC_OK 0x00000008
#define TALITOS_FTR_SEC1 0x00000010

 
static inline bool has_ftr_sec1(struct talitos_private *priv)
{
	if (IS_ENABLED(CONFIG_CRYPTO_DEV_TALITOS1) &&
	    IS_ENABLED(CONFIG_CRYPTO_DEV_TALITOS2))
		return priv->features & TALITOS_FTR_SEC1;

	return IS_ENABLED(CONFIG_CRYPTO_DEV_TALITOS1);
}

 

#define ISR1_FORMAT(x)			(((x) << 28) | ((x) << 16))
#define ISR2_FORMAT(x)			(((x) << 4) | (x))

 
#define TALITOS_MCR			0x1030   
#define   TALITOS_MCR_RCA0		(1 << 15)  
#define   TALITOS_MCR_RCA1		(1 << 14)  
#define   TALITOS_MCR_RCA2		(1 << 13)  
#define   TALITOS_MCR_RCA3		(1 << 12)  
#define   TALITOS1_MCR_SWR		0x1000000      
#define   TALITOS2_MCR_SWR		0x1      
#define TALITOS_MCR_LO			0x1034
#define TALITOS_IMR			0x1008   
 
#define   TALITOS1_IMR_INIT		ISR1_FORMAT(0xf)
#define   TALITOS1_IMR_DONE		ISR1_FORMAT(0x5)  
 
#define   TALITOS2_IMR_INIT		(ISR2_FORMAT(0xf) | 0x10000)
#define   TALITOS2_IMR_DONE		ISR1_FORMAT(0x5)  
#define TALITOS_IMR_LO			0x100C
#define   TALITOS1_IMR_LO_INIT		0x2000000  
#define   TALITOS2_IMR_LO_INIT		0x20000  
#define TALITOS_ISR			0x1010   
#define   TALITOS1_ISR_4CHERR		ISR1_FORMAT(0xa)  
#define   TALITOS1_ISR_4CHDONE		ISR1_FORMAT(0x5)  
#define   TALITOS1_ISR_CH_0_ERR		(2 << 28)  
#define   TALITOS1_ISR_CH_0_DONE	(1 << 28)  
#define   TALITOS1_ISR_TEA_ERR		0x00000040
#define   TALITOS2_ISR_4CHERR		ISR2_FORMAT(0xa)  
#define   TALITOS2_ISR_4CHDONE		ISR2_FORMAT(0x5)  
#define   TALITOS2_ISR_CH_0_ERR		2  
#define   TALITOS2_ISR_CH_0_DONE	1  
#define   TALITOS2_ISR_CH_0_2_ERR	ISR2_FORMAT(0x2)  
#define   TALITOS2_ISR_CH_0_2_DONE	ISR2_FORMAT(0x1)  
#define   TALITOS2_ISR_CH_1_3_ERR	ISR2_FORMAT(0x8)  
#define   TALITOS2_ISR_CH_1_3_DONE	ISR2_FORMAT(0x4)  
#define TALITOS_ISR_LO			0x1014
#define TALITOS_ICR			0x1018   
#define TALITOS_ICR_LO			0x101C

 
#define TALITOS_CH_BASE_OFFSET		0x1000	 
#define TALITOS1_CH_STRIDE		0x1000
#define TALITOS2_CH_STRIDE		0x100

 
#define TALITOS_CCCR			0x8
#define   TALITOS2_CCCR_CONT		0x2     
#define   TALITOS2_CCCR_RESET		0x1     
#define TALITOS_CCCR_LO			0xc
#define   TALITOS_CCCR_LO_IWSE		0x80    
#define   TALITOS_CCCR_LO_EAE		0x20    
#define   TALITOS_CCCR_LO_CDWE		0x10    
#define   TALITOS_CCCR_LO_NE		0x8     
#define   TALITOS_CCCR_LO_NT		0x4     
#define   TALITOS_CCCR_LO_CDIE		0x2     
#define   TALITOS1_CCCR_LO_RESET	0x1     

 
#define TALITOS_CCPSR			0x10
#define TALITOS_CCPSR_LO		0x14
#define   TALITOS_CCPSR_LO_DOF		0x8000  
#define   TALITOS_CCPSR_LO_SOF		0x4000  
#define   TALITOS_CCPSR_LO_MDTE		0x2000  
#define   TALITOS_CCPSR_LO_SGDLZ	0x1000  
#define   TALITOS_CCPSR_LO_FPZ		0x0800  
#define   TALITOS_CCPSR_LO_IDH		0x0400  
#define   TALITOS_CCPSR_LO_IEU		0x0200  
#define   TALITOS_CCPSR_LO_EU		0x0100  
#define   TALITOS_CCPSR_LO_GB		0x0080  
#define   TALITOS_CCPSR_LO_GRL		0x0040  
#define   TALITOS_CCPSR_LO_SB		0x0020  
#define   TALITOS_CCPSR_LO_SRL		0x0010  

 
#define TALITOS_FF			0x48
#define TALITOS_FF_LO			0x4c

 
#define TALITOS_CDPR			0x40
#define TALITOS_CDPR_LO			0x44

 
#define TALITOS_DESCBUF			0x80
#define TALITOS_DESCBUF_LO		0x84

 
#define TALITOS_GATHER			0xc0
#define TALITOS_GATHER_LO		0xc4

 
#define TALITOS_SCATTER			0xe0
#define TALITOS_SCATTER_LO		0xe4

 
#define TALITOS2_DEU			0x2000
#define TALITOS2_AESU			0x4000
#define TALITOS2_MDEU			0x6000
#define TALITOS2_AFEU			0x8000
#define TALITOS2_RNGU			0xa000
#define TALITOS2_PKEU			0xc000
#define TALITOS2_KEU			0xe000
#define TALITOS2_CRCU			0xf000

#define TALITOS12_AESU			0x4000
#define TALITOS12_DEU			0x5000
#define TALITOS12_MDEU			0x6000

#define TALITOS10_AFEU			0x8000
#define TALITOS10_DEU			0xa000
#define TALITOS10_MDEU			0xc000
#define TALITOS10_RNGU			0xe000
#define TALITOS10_PKEU			0x10000
#define TALITOS10_AESU			0x12000

 
#define TALITOS_EUDSR			0x10	 
#define TALITOS_EUDSR_LO		0x14
#define TALITOS_EURCR			0x18  
#define TALITOS_EURCR_LO		0x1c
#define TALITOS_EUSR			0x28  
#define TALITOS_EUSR_LO			0x2c
#define TALITOS_EUISR			0x30
#define TALITOS_EUISR_LO		0x34
#define TALITOS_EUICR			0x38  
#define TALITOS_EUICR_LO		0x3c
#define TALITOS_EU_FIFO			0x800  
#define TALITOS_EU_FIFO_LO		0x804  
 
#define   TALITOS1_DEUICR_KPE		0x00200000  
 
#define   TALITOS_MDEUICR_LO_ICE	0x4000  
 
#define   TALITOS_RNGUSR_LO_RD		0x1	 
#define   TALITOS_RNGUSR_LO_OFL		0xff0000 
#define   TALITOS_RNGURCR_LO_SR		0x1	 

#define TALITOS_MDEU_CONTEXT_SIZE_MD5_SHA1_SHA256	0x28
#define TALITOS_MDEU_CONTEXT_SIZE_SHA384_SHA512		0x48

 

 
#define DESC_HDR_DONE			cpu_to_be32(0xff000000)
#define DESC_HDR_LO_ICCR1_MASK		cpu_to_be32(0x00180000)
#define DESC_HDR_LO_ICCR1_PASS		cpu_to_be32(0x00080000)
#define DESC_HDR_LO_ICCR1_FAIL		cpu_to_be32(0x00100000)

 
#define	DESC_HDR_SEL0_MASK		cpu_to_be32(0xf0000000)
#define	DESC_HDR_SEL0_AFEU		cpu_to_be32(0x10000000)
#define	DESC_HDR_SEL0_DEU		cpu_to_be32(0x20000000)
#define	DESC_HDR_SEL0_MDEUA		cpu_to_be32(0x30000000)
#define	DESC_HDR_SEL0_MDEUB		cpu_to_be32(0xb0000000)
#define	DESC_HDR_SEL0_RNG		cpu_to_be32(0x40000000)
#define	DESC_HDR_SEL0_PKEU		cpu_to_be32(0x50000000)
#define	DESC_HDR_SEL0_AESU		cpu_to_be32(0x60000000)
#define	DESC_HDR_SEL0_KEU		cpu_to_be32(0x70000000)
#define	DESC_HDR_SEL0_CRCU		cpu_to_be32(0x80000000)

 
#define	DESC_HDR_MODE0_ENCRYPT		cpu_to_be32(0x00100000)
#define	DESC_HDR_MODE0_AESU_MASK	cpu_to_be32(0x00600000)
#define	DESC_HDR_MODE0_AESU_CBC		cpu_to_be32(0x00200000)
#define	DESC_HDR_MODE0_AESU_CTR		cpu_to_be32(0x00600000)
#define	DESC_HDR_MODE0_DEU_CBC		cpu_to_be32(0x00400000)
#define	DESC_HDR_MODE0_DEU_3DES		cpu_to_be32(0x00200000)
#define	DESC_HDR_MODE0_MDEU_CONT	cpu_to_be32(0x08000000)
#define	DESC_HDR_MODE0_MDEU_INIT	cpu_to_be32(0x01000000)
#define	DESC_HDR_MODE0_MDEU_HMAC	cpu_to_be32(0x00800000)
#define	DESC_HDR_MODE0_MDEU_PAD		cpu_to_be32(0x00400000)
#define	DESC_HDR_MODE0_MDEU_SHA224	cpu_to_be32(0x00300000)
#define	DESC_HDR_MODE0_MDEU_MD5		cpu_to_be32(0x00200000)
#define	DESC_HDR_MODE0_MDEU_SHA256	cpu_to_be32(0x00100000)
#define	DESC_HDR_MODE0_MDEU_SHA1	cpu_to_be32(0x00000000)
#define	DESC_HDR_MODE0_MDEUB_SHA384	cpu_to_be32(0x00000000)
#define	DESC_HDR_MODE0_MDEUB_SHA512	cpu_to_be32(0x00200000)
#define	DESC_HDR_MODE0_MDEU_MD5_HMAC	(DESC_HDR_MODE0_MDEU_MD5 | \
					 DESC_HDR_MODE0_MDEU_HMAC)
#define	DESC_HDR_MODE0_MDEU_SHA256_HMAC	(DESC_HDR_MODE0_MDEU_SHA256 | \
					 DESC_HDR_MODE0_MDEU_HMAC)
#define	DESC_HDR_MODE0_MDEU_SHA1_HMAC	(DESC_HDR_MODE0_MDEU_SHA1 | \
					 DESC_HDR_MODE0_MDEU_HMAC)

 
#define	DESC_HDR_SEL1_MASK		cpu_to_be32(0x000f0000)
#define	DESC_HDR_SEL1_MDEUA		cpu_to_be32(0x00030000)
#define	DESC_HDR_SEL1_MDEUB		cpu_to_be32(0x000b0000)
#define	DESC_HDR_SEL1_CRCU		cpu_to_be32(0x00080000)

 
#define	DESC_HDR_MODE1_MDEU_CICV	cpu_to_be32(0x00004000)
#define	DESC_HDR_MODE1_MDEU_INIT	cpu_to_be32(0x00001000)
#define	DESC_HDR_MODE1_MDEU_HMAC	cpu_to_be32(0x00000800)
#define	DESC_HDR_MODE1_MDEU_PAD		cpu_to_be32(0x00000400)
#define	DESC_HDR_MODE1_MDEU_SHA224	cpu_to_be32(0x00000300)
#define	DESC_HDR_MODE1_MDEU_MD5		cpu_to_be32(0x00000200)
#define	DESC_HDR_MODE1_MDEU_SHA256	cpu_to_be32(0x00000100)
#define	DESC_HDR_MODE1_MDEU_SHA1	cpu_to_be32(0x00000000)
#define	DESC_HDR_MODE1_MDEUB_SHA384	cpu_to_be32(0x00000000)
#define	DESC_HDR_MODE1_MDEUB_SHA512	cpu_to_be32(0x00000200)
#define	DESC_HDR_MODE1_MDEU_MD5_HMAC	(DESC_HDR_MODE1_MDEU_MD5 | \
					 DESC_HDR_MODE1_MDEU_HMAC)
#define	DESC_HDR_MODE1_MDEU_SHA256_HMAC	(DESC_HDR_MODE1_MDEU_SHA256 | \
					 DESC_HDR_MODE1_MDEU_HMAC)
#define	DESC_HDR_MODE1_MDEU_SHA1_HMAC	(DESC_HDR_MODE1_MDEU_SHA1 | \
					 DESC_HDR_MODE1_MDEU_HMAC)
#define DESC_HDR_MODE1_MDEU_SHA224_HMAC	(DESC_HDR_MODE1_MDEU_SHA224 | \
					 DESC_HDR_MODE1_MDEU_HMAC)
#define DESC_HDR_MODE1_MDEUB_SHA384_HMAC	(DESC_HDR_MODE1_MDEUB_SHA384 | \
						 DESC_HDR_MODE1_MDEU_HMAC)
#define DESC_HDR_MODE1_MDEUB_SHA512_HMAC	(DESC_HDR_MODE1_MDEUB_SHA512 | \
						 DESC_HDR_MODE1_MDEU_HMAC)

 
#define	DESC_HDR_DIR_INBOUND		cpu_to_be32(0x00000002)

 
#define	DESC_HDR_DONE_NOTIFY		cpu_to_be32(0x00000001)

 
#define DESC_HDR_TYPE_AESU_CTR_NONSNOOP		cpu_to_be32(0 << 3)
#define DESC_HDR_TYPE_IPSEC_ESP			cpu_to_be32(1 << 3)
#define DESC_HDR_TYPE_COMMON_NONSNOOP_NO_AFEU	cpu_to_be32(2 << 3)
#define DESC_HDR_TYPE_HMAC_SNOOP_NO_AFEU	cpu_to_be32(4 << 3)

 
#define DESC_PTR_LNKTBL_JUMP			0x80
#define DESC_PTR_LNKTBL_RET			0x02
#define DESC_PTR_LNKTBL_NEXT			0x01
