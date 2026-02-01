 
 

#ifndef REGS_H
#define REGS_H

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/io-64-nonatomic-hi-lo.h>

 

extern bool caam_little_end;
extern bool caam_imx;
extern size_t caam_ptr_sz;

#define caam_to_cpu(len)						\
static inline u##len caam##len ## _to_cpu(u##len val)			\
{									\
	if (caam_little_end)						\
		return le##len ## _to_cpu((__force __le##len)val);	\
	else								\
		return be##len ## _to_cpu((__force __be##len)val);	\
}

#define cpu_to_caam(len)					\
static inline u##len cpu_to_caam##len(u##len val)		\
{								\
	if (caam_little_end)					\
		return (__force u##len)cpu_to_le##len(val);	\
	else							\
		return (__force u##len)cpu_to_be##len(val);	\
}

caam_to_cpu(16)
caam_to_cpu(32)
caam_to_cpu(64)
cpu_to_caam(16)
cpu_to_caam(32)
cpu_to_caam(64)

static inline void wr_reg32(void __iomem *reg, u32 data)
{
	if (caam_little_end)
		iowrite32(data, reg);
	else
		iowrite32be(data, reg);
}

static inline u32 rd_reg32(void __iomem *reg)
{
	if (caam_little_end)
		return ioread32(reg);

	return ioread32be(reg);
}

static inline void clrsetbits_32(void __iomem *reg, u32 clear, u32 set)
{
	if (caam_little_end)
		iowrite32((ioread32(reg) & ~clear) | set, reg);
	else
		iowrite32be((ioread32be(reg) & ~clear) | set, reg);
}

 
static inline void wr_reg64(void __iomem *reg, u64 data)
{
	if (caam_little_end) {
		if (caam_imx) {
			iowrite32(data >> 32, (u32 __iomem *)(reg));
			iowrite32(data, (u32 __iomem *)(reg) + 1);
		} else {
			iowrite64(data, reg);
		}
	} else {
		iowrite64be(data, reg);
	}
}

static inline u64 rd_reg64(void __iomem *reg)
{
	if (caam_little_end) {
		if (caam_imx) {
			u32 low, high;

			high = ioread32(reg);
			low  = ioread32(reg + sizeof(u32));

			return low + ((u64)high << 32);
		} else {
			return ioread64(reg);
		}
	} else {
		return ioread64be(reg);
	}
}

static inline u64 cpu_to_caam_dma64(dma_addr_t value)
{
	if (caam_imx) {
		u64 ret_val = (u64)cpu_to_caam32(lower_32_bits(value)) << 32;

		if (IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT))
			ret_val |= (u64)cpu_to_caam32(upper_32_bits(value));

		return ret_val;
	}

	return cpu_to_caam64(value);
}

static inline u64 caam_dma64_to_cpu(u64 value)
{
	if (caam_imx)
		return (((u64)caam32_to_cpu(lower_32_bits(value)) << 32) |
			 (u64)caam32_to_cpu(upper_32_bits(value)));

	return caam64_to_cpu(value);
}

static inline u64 cpu_to_caam_dma(u64 value)
{
	if (IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT) &&
	    caam_ptr_sz == sizeof(u64))
		return cpu_to_caam_dma64(value);
	else
		return cpu_to_caam32(value);
}

static inline u64 caam_dma_to_cpu(u64 value)
{
	if (IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT) &&
	    caam_ptr_sz == sizeof(u64))
		return caam_dma64_to_cpu(value);
	else
		return caam32_to_cpu(value);
}

 

static inline void jr_outentry_get(void *outring, int hw_idx, dma_addr_t *desc,
				   u32 *jrstatus)
{

	if (caam_ptr_sz == sizeof(u32)) {
		struct {
			u32 desc;
			u32 jrstatus;
		} __packed *outentry = outring;

		*desc = outentry[hw_idx].desc;
		*jrstatus = outentry[hw_idx].jrstatus;
	} else {
		struct {
			dma_addr_t desc; 
			u32 jrstatus;	 
		} __packed *outentry = outring;

		*desc = outentry[hw_idx].desc;
		*jrstatus = outentry[hw_idx].jrstatus;
	}
}

#define SIZEOF_JR_OUTENTRY	(caam_ptr_sz + sizeof(u32))

static inline dma_addr_t jr_outentry_desc(void *outring, int hw_idx)
{
	dma_addr_t desc;
	u32 unused;

	jr_outentry_get(outring, hw_idx, &desc, &unused);

	return desc;
}

static inline u32 jr_outentry_jrstatus(void *outring, int hw_idx)
{
	dma_addr_t unused;
	u32 jrstatus;

	jr_outentry_get(outring, hw_idx, &unused, &jrstatus);

	return jrstatus;
}

static inline void jr_inpentry_set(void *inpring, int hw_idx, dma_addr_t val)
{
	if (caam_ptr_sz == sizeof(u32)) {
		u32 *inpentry = inpring;

		inpentry[hw_idx] = val;
	} else {
		dma_addr_t *inpentry = inpring;

		inpentry[hw_idx] = val;
	}
}

#define SIZEOF_JR_INPENTRY	caam_ptr_sz


 
struct version_regs {
	u32 crca;	 
	u32 afha;	 
	u32 kfha;	 
	u32 pkha;	 
	u32 aesa;	 
	u32 mdha;	 
	u32 desa;	 
	u32 snw8a;	 
	u32 snw9a;	 
	u32 zuce;	 
	u32 zuca;	 
	u32 ccha;	 
	u32 ptha;	 
	u32 rng;	 
	u32 trng;	 
	u32 aaha;	 
	u32 rsvd[10];
	u32 sr;		 
	u32 dma;	 
	u32 ai;		 
	u32 qi;		 
	u32 jr;		 
	u32 deco;	 
};

 

 
#define CHA_VER_NUM_MASK	0xffull
 
#define CHA_VER_MISC_SHIFT	8
#define CHA_VER_MISC_MASK	(0xffull << CHA_VER_MISC_SHIFT)
 
#define CHA_VER_REV_SHIFT	16
#define CHA_VER_REV_MASK	(0xffull << CHA_VER_REV_SHIFT)
 
#define CHA_VER_VID_SHIFT	24
#define CHA_VER_VID_MASK	(0xffull << CHA_VER_VID_SHIFT)

 
#define CHA_VER_MISC_AES_NUM_MASK	GENMASK(7, 0)
#define CHA_VER_MISC_AES_GCM		BIT(1 + CHA_VER_MISC_SHIFT)

 
#define CHA_VER_MISC_PKHA_NO_CRYPT	BIT(7 + CHA_VER_MISC_SHIFT)

 

 
#define CHA_NUM_MS_DECONUM_SHIFT	24
#define CHA_NUM_MS_DECONUM_MASK	(0xfull << CHA_NUM_MS_DECONUM_SHIFT)

 
#define CHA_ID_LS_AES_SHIFT	0
#define CHA_ID_LS_AES_MASK	(0xfull << CHA_ID_LS_AES_SHIFT)

#define CHA_ID_LS_DES_SHIFT	4
#define CHA_ID_LS_DES_MASK	(0xfull << CHA_ID_LS_DES_SHIFT)

#define CHA_ID_LS_ARC4_SHIFT	8
#define CHA_ID_LS_ARC4_MASK	(0xfull << CHA_ID_LS_ARC4_SHIFT)

#define CHA_ID_LS_MD_SHIFT	12
#define CHA_ID_LS_MD_MASK	(0xfull << CHA_ID_LS_MD_SHIFT)

#define CHA_ID_LS_RNG_SHIFT	16
#define CHA_ID_LS_RNG_MASK	(0xfull << CHA_ID_LS_RNG_SHIFT)

#define CHA_ID_LS_SNW8_SHIFT	20
#define CHA_ID_LS_SNW8_MASK	(0xfull << CHA_ID_LS_SNW8_SHIFT)

#define CHA_ID_LS_KAS_SHIFT	24
#define CHA_ID_LS_KAS_MASK	(0xfull << CHA_ID_LS_KAS_SHIFT)

#define CHA_ID_LS_PK_SHIFT	28
#define CHA_ID_LS_PK_MASK	(0xfull << CHA_ID_LS_PK_SHIFT)

#define CHA_ID_MS_CRC_SHIFT	0
#define CHA_ID_MS_CRC_MASK	(0xfull << CHA_ID_MS_CRC_SHIFT)

#define CHA_ID_MS_SNW9_SHIFT	4
#define CHA_ID_MS_SNW9_MASK	(0xfull << CHA_ID_MS_SNW9_SHIFT)

#define CHA_ID_MS_DECO_SHIFT	24
#define CHA_ID_MS_DECO_MASK	(0xfull << CHA_ID_MS_DECO_SHIFT)

#define CHA_ID_MS_JR_SHIFT	28
#define CHA_ID_MS_JR_MASK	(0xfull << CHA_ID_MS_JR_SHIFT)

 
#define CHA_VER_VID_AES_LP	0x3ull
#define CHA_VER_VID_AES_HP	0x4ull
#define CHA_VER_VID_MD_LP256	0x0ull
#define CHA_VER_VID_MD_LP512	0x1ull
#define CHA_VER_VID_MD_HP	0x2ull

struct sec_vid {
	u16 ip_id;
	u8 maj_rev;
	u8 min_rev;
};

struct caam_perfmon {
	 
	u64 req_dequeued;	 
	u64 ob_enc_req;	 
	u64 ib_dec_req;	 
	u64 ob_enc_bytes;	 
	u64 ob_prot_bytes;	 
	u64 ib_dec_bytes;	 
	u64 ib_valid_bytes;	 
	u64 rsvd[13];

	 
	u32 cha_rev_ms;		 
	u32 cha_rev_ls;		 
#define CTPR_MS_QI_SHIFT	25
#define CTPR_MS_QI_MASK		(0x1ull << CTPR_MS_QI_SHIFT)
#define CTPR_MS_PS		BIT(17)
#define CTPR_MS_DPAA2		BIT(13)
#define CTPR_MS_VIRT_EN_INCL	0x00000001
#define CTPR_MS_VIRT_EN_POR	0x00000002
#define CTPR_MS_PG_SZ_MASK	0x10
#define CTPR_MS_PG_SZ_SHIFT	4
	u32 comp_parms_ms;	 
#define CTPR_LS_BLOB           BIT(1)
	u32 comp_parms_ls;	 
	u64 rsvd1[2];

	 
	u64 faultaddr;	 
	u32 faultliodn;	 
	u32 faultdetail;	 
	u32 rsvd2;
#define CSTA_PLEND		BIT(10)
#define CSTA_ALT_PLEND		BIT(18)
#define CSTA_MOO		GENMASK(9, 8)
#define CSTA_MOO_SECURE	1
#define CSTA_MOO_TRUSTED	2
	u32 status;		 
	u64 rsvd3;

	 
	u32 rtic_id;		 
#define CCBVID_ERA_MASK		0xff000000
#define CCBVID_ERA_SHIFT	24
	u32 ccb_id;		 
	u32 cha_id_ms;		 
	u32 cha_id_ls;		 
	u32 cha_num_ms;		 
	u32 cha_num_ls;		 
#define SECVID_MS_IPID_MASK	0xffff0000
#define SECVID_MS_IPID_SHIFT	16
#define SECVID_MS_MAJ_REV_MASK	0x0000ff00
#define SECVID_MS_MAJ_REV_SHIFT	8
	u32 caam_id_ms;		 
	u32 caam_id_ls;		 
};

 
#define MSTRID_LOCK_LIODN	0x80000000
#define MSTRID_LOCK_MAKETRUSTED	0x00010000	 

#define MSTRID_LIODN_MASK	0x0fff
struct masterid {
	u32 liodn_ms;	 
	u32 liodn_ls;	 
};

 
 
struct rngtst {
	u32 mode;		 
	u32 rsvd1[3];
	u32 reset;		 
	u32 rsvd2[3];
	u32 status;		 
	u32 rsvd3;
	u32 errstat;		 
	u32 rsvd4;
	u32 errctl;		 
	u32 rsvd5;
	u32 entropy;		 
	u32 rsvd6[15];
	u32 verifctl;	 
	u32 rsvd7;
	u32 verifstat;	 
	u32 rsvd8;
	u32 verifdata;	 
	u32 rsvd9;
	u32 xkey;		 
	u32 rsvd10;
	u32 oscctctl;	 
	u32 rsvd11;
	u32 oscct;		 
	u32 rsvd12;
	u32 oscctstat;	 
	u32 rsvd13[2];
	u32 ofifo[4];	 
	u32 rsvd14[15];
};

 
struct rng4tst {
#define RTMCTL_ACC  BIT(5)   
#define RTMCTL_PRGM BIT(16)  
#define RTMCTL_SAMP_MODE_VON_NEUMANN_ES_SC	0  
#define RTMCTL_SAMP_MODE_RAW_ES_SC		1  
#define RTMCTL_SAMP_MODE_VON_NEUMANN_ES_RAW_SC	2  
#define RTMCTL_SAMP_MODE_INVALID		3  
	u32 rtmctl;		 
	u32 rtscmisc;		 
	u32 rtpkrrng;		 
	union {
		u32 rtpkrmax;	 
		u32 rtpkrsq;	 
	};
#define RTSDCTL_ENT_DLY_SHIFT 16
#define RTSDCTL_ENT_DLY_MASK (0xffff << RTSDCTL_ENT_DLY_SHIFT)
#define RTSDCTL_ENT_DLY_MIN 3200
#define RTSDCTL_ENT_DLY_MAX 12800
#define RTSDCTL_SAMP_SIZE_MASK 0xffff
#define RTSDCTL_SAMP_SIZE_VAL 512
	u32 rtsdctl;		 
	union {
		u32 rtsblim;	 
		u32 rttotsam;	 
	};
	u32 rtfrqmin;		 
#define RTFRQMAX_DISABLE	(1 << 20)
	union {
		u32 rtfrqmax;	 
		u32 rtfrqcnt;	 
	};
	union {
		u32 rtscmc;	 
		u32 rtscml;	 
	};
	union {
		u32 rtscrc[6];	 
		u32 rtscrl[6];	 
	};
	u32 rsvd1[33];
#define RDSTA_SKVT 0x80000000
#define RDSTA_SKVN 0x40000000
#define RDSTA_PR0 BIT(4)
#define RDSTA_PR1 BIT(5)
#define RDSTA_IF0 0x00000001
#define RDSTA_IF1 0x00000002
#define RDSTA_MASK (RDSTA_PR1 | RDSTA_PR0 | RDSTA_IF1 | RDSTA_IF0)
	u32 rdsta;
	u32 rsvd2[15];
};

 

#define KEK_KEY_SIZE		8
#define TKEK_KEY_SIZE		8
#define TDSK_KEY_SIZE		8

#define DECO_RESET	1	 
#define DECO_RESET_0	(DECO_RESET << 0)
#define DECO_RESET_1	(DECO_RESET << 1)
#define DECO_RESET_2	(DECO_RESET << 2)
#define DECO_RESET_3	(DECO_RESET << 3)
#define DECO_RESET_4	(DECO_RESET << 4)

struct caam_ctrl {
	 
	 
	u32 rsvd1;
	u32 mcr;		 
	u32 rsvd2;
	u32 scfgr;		 

	 
	 
	struct masterid jr_mid[4];	 
	u32 rsvd3[11];
	u32 jrstart;			 
	struct masterid rtic_mid[4];	 
	u32 rsvd4[5];
	u32 deco_rsr;			 
	u32 rsvd11;
	u32 deco_rq;			 
	struct masterid deco_mid[16];	 

	 
	u32 deco_avail;		 
	u32 deco_reset;		 
	u32 rsvd6[182];

	 
	 
	u32 kek[KEK_KEY_SIZE];	 
	u32 tkek[TKEK_KEY_SIZE];	 
	u32 tdsk[TDSK_KEY_SIZE];	 
	u32 rsvd7[32];
	u64 sknonce;			 
	u32 rsvd8[70];

	 
	 
	union {
		struct rngtst rtst[2];
		struct rng4tst r4tst[2];
	};

	u32 rsvd9[416];

	 
	struct version_regs vreg;
	 
	struct caam_perfmon perfmon;
};

 
#define MCFGR_SWRESET		0x80000000  
#define MCFGR_WDENABLE		0x40000000  
#define MCFGR_WDFAIL		0x20000000  
#define MCFGR_DMA_RESET		0x10000000
#define MCFGR_LONG_PTR		0x00010000  
#define SCFGR_RDBENABLE		0x00000400
#define SCFGR_VIRT_EN		0x00008000
#define DECORR_RQD0ENABLE	0x00000001  
#define DECORSR_JR0		0x00000001  
#define DECORSR_VALID		0x80000000
#define DECORR_DEN0		0x00010000  

 
#define MCFGR_ARCACHE_SHIFT	12
#define MCFGR_ARCACHE_MASK	(0xf << MCFGR_ARCACHE_SHIFT)
#define MCFGR_ARCACHE_BUFF	(0x1 << MCFGR_ARCACHE_SHIFT)
#define MCFGR_ARCACHE_CACH	(0x2 << MCFGR_ARCACHE_SHIFT)
#define MCFGR_ARCACHE_RALL	(0x4 << MCFGR_ARCACHE_SHIFT)

 
#define MCFGR_AWCACHE_SHIFT	8
#define MCFGR_AWCACHE_MASK	(0xf << MCFGR_AWCACHE_SHIFT)
#define MCFGR_AWCACHE_BUFF	(0x1 << MCFGR_AWCACHE_SHIFT)
#define MCFGR_AWCACHE_CACH	(0x2 << MCFGR_AWCACHE_SHIFT)
#define MCFGR_AWCACHE_WALL	(0x8 << MCFGR_AWCACHE_SHIFT)

 
#define MCFGR_AXIPIPE_SHIFT	4
#define MCFGR_AXIPIPE_MASK	(0xf << MCFGR_AXIPIPE_SHIFT)

#define MCFGR_AXIPRI		0x00000008  
#define MCFGR_LARGE_BURST	0x00000004  
#define MCFGR_BURST_64		0x00000001  

 
#define JRSTART_JR0_START       0x00000001  
#define JRSTART_JR1_START       0x00000002  
#define JRSTART_JR2_START       0x00000004  
#define JRSTART_JR3_START       0x00000008  

 
struct caam_job_ring {
	 
	u64 inpring_base;	 
	u32 rsvd1;
	u32 inpring_size;	 
	u32 rsvd2;
	u32 inpring_avail;	 
	u32 rsvd3;
	u32 inpring_jobadd;	 

	 
	u64 outring_base;	 
	u32 rsvd4;
	u32 outring_size;	 
	u32 rsvd5;
	u32 outring_rmvd;	 
	u32 rsvd6;
	u32 outring_used;	 

	 
	u32 rsvd7;
	u32 jroutstatus;	 
	u32 rsvd8;
	u32 jrintstatus;	 
	u32 rconfig_hi;	 
	u32 rconfig_lo;

	 
	u32 rsvd9;
	u32 inp_rdidx;	 
	u32 rsvd10;
	u32 out_wtidx;	 

	 
	u32 rsvd11;
	u32 jrcommand;	 

	u32 rsvd12[900];

	 
	struct version_regs vreg;
	 
	struct caam_perfmon perfmon;
};

#define JR_RINGSIZE_MASK	0x03ff
 
#define JRSTA_SSRC_SHIFT            28
#define JRSTA_SSRC_MASK             0xf0000000

#define JRSTA_SSRC_NONE             0x00000000
#define JRSTA_SSRC_CCB_ERROR        0x20000000
#define JRSTA_SSRC_JUMP_HALT_USER   0x30000000
#define JRSTA_SSRC_DECO             0x40000000
#define JRSTA_SSRC_QI               0x50000000
#define JRSTA_SSRC_JRERROR          0x60000000
#define JRSTA_SSRC_JUMP_HALT_CC     0x70000000

#define JRSTA_DECOERR_JUMP          0x08000000
#define JRSTA_DECOERR_INDEX_SHIFT   8
#define JRSTA_DECOERR_INDEX_MASK    0xff00
#define JRSTA_DECOERR_ERROR_MASK    0x00ff

#define JRSTA_DECOERR_NONE          0x00
#define JRSTA_DECOERR_LINKLEN       0x01
#define JRSTA_DECOERR_LINKPTR       0x02
#define JRSTA_DECOERR_JRCTRL        0x03
#define JRSTA_DECOERR_DESCCMD       0x04
#define JRSTA_DECOERR_ORDER         0x05
#define JRSTA_DECOERR_KEYCMD        0x06
#define JRSTA_DECOERR_LOADCMD       0x07
#define JRSTA_DECOERR_STORECMD      0x08
#define JRSTA_DECOERR_OPCMD         0x09
#define JRSTA_DECOERR_FIFOLDCMD     0x0a
#define JRSTA_DECOERR_FIFOSTCMD     0x0b
#define JRSTA_DECOERR_MOVECMD       0x0c
#define JRSTA_DECOERR_JUMPCMD       0x0d
#define JRSTA_DECOERR_MATHCMD       0x0e
#define JRSTA_DECOERR_SHASHCMD      0x0f
#define JRSTA_DECOERR_SEQCMD        0x10
#define JRSTA_DECOERR_DECOINTERNAL  0x11
#define JRSTA_DECOERR_SHDESCHDR     0x12
#define JRSTA_DECOERR_HDRLEN        0x13
#define JRSTA_DECOERR_BURSTER       0x14
#define JRSTA_DECOERR_DESCSIGNATURE 0x15
#define JRSTA_DECOERR_DMA           0x16
#define JRSTA_DECOERR_BURSTFIFO     0x17
#define JRSTA_DECOERR_JRRESET       0x1a
#define JRSTA_DECOERR_JOBFAIL       0x1b
#define JRSTA_DECOERR_DNRERR        0x80
#define JRSTA_DECOERR_UNDEFPCL      0x81
#define JRSTA_DECOERR_PDBERR        0x82
#define JRSTA_DECOERR_ANRPLY_LATE   0x83
#define JRSTA_DECOERR_ANRPLY_REPLAY 0x84
#define JRSTA_DECOERR_SEQOVF        0x85
#define JRSTA_DECOERR_INVSIGN       0x86
#define JRSTA_DECOERR_DSASIGN       0x87

#define JRSTA_QIERR_ERROR_MASK      0x00ff

#define JRSTA_CCBERR_JUMP           0x08000000
#define JRSTA_CCBERR_INDEX_MASK     0xff00
#define JRSTA_CCBERR_INDEX_SHIFT    8
#define JRSTA_CCBERR_CHAID_MASK     0x00f0
#define JRSTA_CCBERR_CHAID_SHIFT    4
#define JRSTA_CCBERR_ERRID_MASK     0x000f

#define JRSTA_CCBERR_CHAID_AES      (0x01 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_DES      (0x02 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_ARC4     (0x03 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_MD       (0x04 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_RNG      (0x05 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_SNOW     (0x06 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_KASUMI   (0x07 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_PK       (0x08 << JRSTA_CCBERR_CHAID_SHIFT)
#define JRSTA_CCBERR_CHAID_CRC      (0x09 << JRSTA_CCBERR_CHAID_SHIFT)

#define JRSTA_CCBERR_ERRID_NONE     0x00
#define JRSTA_CCBERR_ERRID_MODE     0x01
#define JRSTA_CCBERR_ERRID_DATASIZ  0x02
#define JRSTA_CCBERR_ERRID_KEYSIZ   0x03
#define JRSTA_CCBERR_ERRID_PKAMEMSZ 0x04
#define JRSTA_CCBERR_ERRID_PKBMEMSZ 0x05
#define JRSTA_CCBERR_ERRID_SEQUENCE 0x06
#define JRSTA_CCBERR_ERRID_PKDIVZRO 0x07
#define JRSTA_CCBERR_ERRID_PKMODEVN 0x08
#define JRSTA_CCBERR_ERRID_KEYPARIT 0x09
#define JRSTA_CCBERR_ERRID_ICVCHK   0x0a
#define JRSTA_CCBERR_ERRID_HARDWARE 0x0b
#define JRSTA_CCBERR_ERRID_CCMAAD   0x0c
#define JRSTA_CCBERR_ERRID_INVCHA   0x0f

#define JRINT_ERR_INDEX_MASK        0x3fff0000
#define JRINT_ERR_INDEX_SHIFT       16
#define JRINT_ERR_TYPE_MASK         0xf00
#define JRINT_ERR_TYPE_SHIFT        8
#define JRINT_ERR_HALT_MASK         0xc
#define JRINT_ERR_HALT_SHIFT        2
#define JRINT_ERR_HALT_INPROGRESS   0x4
#define JRINT_ERR_HALT_COMPLETE     0x8
#define JRINT_JR_ERROR              0x02
#define JRINT_JR_INT                0x01

#define JRINT_ERR_TYPE_WRITE        1
#define JRINT_ERR_TYPE_BAD_INPADDR  3
#define JRINT_ERR_TYPE_BAD_OUTADDR  4
#define JRINT_ERR_TYPE_INV_INPWRT   5
#define JRINT_ERR_TYPE_INV_OUTWRT   6
#define JRINT_ERR_TYPE_RESET        7
#define JRINT_ERR_TYPE_REMOVE_OFL   8
#define JRINT_ERR_TYPE_ADD_OFL      9

#define JRCFG_SOE		0x04
#define JRCFG_ICEN		0x02
#define JRCFG_IMSK		0x01
#define JRCFG_ICDCT_SHIFT	8
#define JRCFG_ICTT_SHIFT	16

#define JRCR_RESET                  0x01

 

struct rtic_element {
	u64 address;
	u32 rsvd;
	u32 length;
};

struct rtic_block {
	struct rtic_element element[2];
};

struct rtic_memhash {
	u32 memhash_be[32];
	u32 memhash_le[32];
};

struct caam_assurance {
     
	u32 rsvd1;
	u32 status;		 
	u32 rsvd2;
	u32 cmd;		 
	u32 rsvd3;
	u32 ctrl;		 
	u32 rsvd4;
	u32 throttle;	 
	u32 rsvd5[2];
	u64 watchdog;	 
	u32 rsvd6;
	u32 rend;		 
	u32 rsvd7[50];

	 
	struct rtic_block memblk[4];	 
	u32 rsvd8[32];

	 
	struct rtic_memhash hash[4];	 
	u32 rsvd_3[640];
};

 

struct caam_queue_if {
	u32 qi_control_hi;	 
	u32 qi_control_lo;
	u32 rsvd1;
	u32 qi_status;	 
	u32 qi_deq_cfg_hi;	 
	u32 qi_deq_cfg_lo;
	u32 qi_enq_cfg_hi;	 
	u32 qi_enq_cfg_lo;
	u32 rsvd2[1016];
};

 
#define QICTL_DQEN      0x01               
#define QICTL_STOP      0x02               
#define QICTL_SOE       0x04               

 
#define QICTL_MBSI	0x01
#define QICTL_MHWSI	0x02
#define QICTL_MWSI	0x04
#define QICTL_MDWSI	0x08
#define QICTL_CBSI	0x10		 
#define QICTL_CHWSI	0x20		 
#define QICTL_CWSI	0x40		 
#define QICTL_CDWSI	0x80		 
#define QICTL_MBSO	0x0100
#define QICTL_MHWSO	0x0200
#define QICTL_MWSO	0x0400
#define QICTL_MDWSO	0x0800
#define QICTL_CBSO	0x1000		 
#define QICTL_CHWSO	0x2000		 
#define QICTL_CWSO	0x4000		 
#define QICTL_CDWSO     0x8000		 
#define QICTL_DMBS	0x010000
#define QICTL_EPO	0x020000

 
#define QISTA_PHRDERR   0x01               
#define QISTA_CFRDERR   0x02               
#define QISTA_OFWRERR   0x04               
#define QISTA_BPDERR    0x08               
#define QISTA_BTSERR    0x10               
#define QISTA_CFWRERR   0x20               
#define QISTA_STOPD     0x80000000         

 
struct deco_sg_table {
	u64 addr;		 
	u32 elen;		 
	u32 bpid_offset;	 
};

 
struct caam_deco {
	u32 rsvd1;
	u32 cls1_mode;	 
	u32 rsvd2;
	u32 cls1_keysize;	 
	u32 cls1_datasize_hi;	 
	u32 cls1_datasize_lo;
	u32 rsvd3;
	u32 cls1_icvsize;	 
	u32 rsvd4[5];
	u32 cha_ctrl;	 
	u32 rsvd5;
	u32 irq_crtl;	 
	u32 rsvd6;
	u32 clr_written;	 
	u32 ccb_status_hi;	 
	u32 ccb_status_lo;
	u32 rsvd7[3];
	u32 aad_size;	 
	u32 rsvd8;
	u32 cls1_iv_size;	 
	u32 rsvd9[7];
	u32 pkha_a_size;	 
	u32 rsvd10;
	u32 pkha_b_size;	 
	u32 rsvd11;
	u32 pkha_n_size;	 
	u32 rsvd12;
	u32 pkha_e_size;	 
	u32 rsvd13[24];
	u32 cls1_ctx[16];	 
	u32 rsvd14[48];
	u32 cls1_key[8];	 
	u32 rsvd15[121];
	u32 cls2_mode;	 
	u32 rsvd16;
	u32 cls2_keysize;	 
	u32 cls2_datasize_hi;	 
	u32 cls2_datasize_lo;
	u32 rsvd17;
	u32 cls2_icvsize;	 
	u32 rsvd18[56];
	u32 cls2_ctx[18];	 
	u32 rsvd19[46];
	u32 cls2_key[32];	 
	u32 rsvd20[84];
	u32 inp_infofifo_hi;	 
	u32 inp_infofifo_lo;
	u32 rsvd21[2];
	u64 inp_datafifo;	 
	u32 rsvd22[2];
	u64 out_datafifo;	 
	u32 rsvd23[2];
	u32 jr_ctl_hi;	 
	u32 jr_ctl_lo;
	u64 jr_descaddr;	 
#define DECO_OP_STATUS_HI_ERR_MASK 0xF00000FF
	u32 op_status_hi;	 
	u32 op_status_lo;
	u32 rsvd24[2];
	u32 liodn;		 
	u32 td_liodn;	 
	u32 rsvd26[6];
	u64 math[4];		 
	u32 rsvd27[8];
	struct deco_sg_table gthr_tbl[4];	 
	u32 rsvd28[16];
	struct deco_sg_table sctr_tbl[4];	 
	u32 rsvd29[48];
	u32 descbuf[64];	 
	u32 rscvd30[193];
#define DESC_DBG_DECO_STAT_VALID	0x80000000
#define DESC_DBG_DECO_STAT_MASK		0x00F00000
#define DESC_DBG_DECO_STAT_SHIFT	20
	u32 desc_dbg;		 
	u32 rsvd31[13];
#define DESC_DER_DECO_STAT_MASK		0x000F0000
#define DESC_DER_DECO_STAT_SHIFT	16
	u32 dbg_exec;		 
	u32 rsvd32[112];
};

#define DECO_STAT_HOST_ERR	0xD

#define DECO_JQCR_WHL		0x20000000
#define DECO_JQCR_FOUR		0x10000000

#define JR_BLOCK_NUMBER		1
#define ASSURE_BLOCK_NUMBER	6
#define QI_BLOCK_NUMBER		7
#define DECO_BLOCK_NUMBER	8
#define PG_SIZE_4K		0x1000
#define PG_SIZE_64K		0x10000
#endif  
