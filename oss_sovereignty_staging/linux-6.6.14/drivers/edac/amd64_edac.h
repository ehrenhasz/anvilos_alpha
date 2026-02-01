 

#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/slab.h>
#include <linux/mmzone.h>
#include <linux/edac.h>
#include <linux/bitfield.h>
#include <asm/cpu_device_id.h>
#include <asm/msr.h>
#include "edac_module.h"
#include "mce_amd.h"

#define amd64_info(fmt, arg...) \
	edac_printk(KERN_INFO, "amd64", fmt, ##arg)

#define amd64_warn(fmt, arg...) \
	edac_printk(KERN_WARNING, "amd64", "Warning: " fmt, ##arg)

#define amd64_err(fmt, arg...) \
	edac_printk(KERN_ERR, "amd64", "Error: " fmt, ##arg)

#define amd64_mc_warn(mci, fmt, arg...) \
	edac_mc_chipset_printk(mci, KERN_WARNING, "amd64", fmt, ##arg)

#define amd64_mc_err(mci, fmt, arg...) \
	edac_mc_chipset_printk(mci, KERN_ERR, "amd64", fmt, ##arg)

 

#define EDAC_MOD_STR			"amd64_edac"

 
#define K8_REV_D			1
#define K8_REV_E			2
#define K8_REV_F			4

 
#define NUM_CHIPSELECTS			8
#define DRAM_RANGES			8
#define NUM_CONTROLLERS			12

#define ON true
#define OFF false

 
#define PCI_DEVICE_ID_AMD_15H_NB_F1	0x1601
#define PCI_DEVICE_ID_AMD_15H_NB_F2	0x1602
#define PCI_DEVICE_ID_AMD_15H_M30H_NB_F1 0x141b
#define PCI_DEVICE_ID_AMD_15H_M30H_NB_F2 0x141c
#define PCI_DEVICE_ID_AMD_15H_M60H_NB_F1 0x1571
#define PCI_DEVICE_ID_AMD_15H_M60H_NB_F2 0x1572
#define PCI_DEVICE_ID_AMD_16H_NB_F1	0x1531
#define PCI_DEVICE_ID_AMD_16H_NB_F2	0x1532
#define PCI_DEVICE_ID_AMD_16H_M30H_NB_F1 0x1581
#define PCI_DEVICE_ID_AMD_16H_M30H_NB_F2 0x1582

 
#define DRAM_BASE_LO			0x40
#define DRAM_LIMIT_LO			0x44

 
#define DRAM_CONT_BASE			0x200
#define DRAM_CONT_LIMIT			0x204

 
#define DRAM_CONT_HIGH_OFF		0x240

#define dram_rw(pvt, i)			((u8)(pvt->ranges[i].base.lo & 0x3))
#define dram_intlv_sel(pvt, i)		((u8)((pvt->ranges[i].lim.lo >> 8) & 0x7))
#define dram_dst_node(pvt, i)		((u8)(pvt->ranges[i].lim.lo & 0x7))

#define DHAR				0xf0
#define dhar_mem_hoist_valid(pvt)	((pvt)->dhar & BIT(1))
#define dhar_base(pvt)			((pvt)->dhar & 0xff000000)
#define k8_dhar_offset(pvt)		(((pvt)->dhar & 0x0000ff00) << 16)

					 
#define f10_dhar_offset(pvt)		(((pvt)->dhar & 0x0000ff80) << 16)

#define DCT_CFG_SEL			0x10C

#define DRAM_LOCAL_NODE_BASE		0x120
#define DRAM_LOCAL_NODE_LIM		0x124

#define DRAM_BASE_HI			0x140
#define DRAM_LIMIT_HI			0x144


 
#define DCSB0				0x40
#define DCSB1				0x140
#define DCSB_CS_ENABLE			BIT(0)

#define DCSM0				0x60
#define DCSM1				0x160

#define csrow_enabled(i, dct, pvt)	((pvt)->csels[(dct)].csbases[(i)]     & DCSB_CS_ENABLE)
#define csrow_sec_enabled(i, dct, pvt)	((pvt)->csels[(dct)].csbases_sec[(i)] & DCSB_CS_ENABLE)

#define DRAM_CONTROL			0x78

#define DBAM0				0x80
#define DBAM1				0x180

 
#define DBAM_DIMM(i, reg)		((((reg) >> (4*(i)))) & 0xF)

#define DBAM_MAX_VALUE			11

#define DCLR0				0x90
#define DCLR1				0x190
#define REVE_WIDTH_128			BIT(16)
#define WIDTH_128			BIT(11)

#define DCHR0				0x94
#define DCHR1				0x194
#define DDR3_MODE			BIT(8)

#define DCT_SEL_LO			0x110
#define dct_high_range_enabled(pvt)	((pvt)->dct_sel_lo & BIT(0))
#define dct_interleave_enabled(pvt)	((pvt)->dct_sel_lo & BIT(2))

#define dct_ganging_enabled(pvt)	((boot_cpu_data.x86 == 0x10) && ((pvt)->dct_sel_lo & BIT(4)))

#define dct_data_intlv_enabled(pvt)	((pvt)->dct_sel_lo & BIT(5))
#define dct_memory_cleared(pvt)		((pvt)->dct_sel_lo & BIT(10))

#define SWAP_INTLV_REG			0x10c

#define DCT_SEL_HI			0x114

#define F15H_M60H_SCRCTRL		0x1C8

 
#define NBCTL				0x40

#define NBCFG				0x44
#define NBCFG_CHIPKILL			BIT(23)
#define NBCFG_ECC_ENABLE		BIT(22)

 
#define F10_NBSL_EXT_ERR_ECC		0x8
#define NBSL_PP_OBS			0x2

#define SCRCTRL				0x58

#define F10_ONLINE_SPARE		0xB0
#define online_spare_swap_done(pvt, c)	(((pvt)->online_spare >> (1 + 2 * (c))) & 0x1)
#define online_spare_bad_dramcs(pvt, c)	(((pvt)->online_spare >> (4 + 4 * (c))) & 0x7)

#define F10_NB_ARRAY_ADDR		0xB8
#define F10_NB_ARRAY_DRAM		BIT(31)

 
#define SET_NB_ARRAY_ADDR(section)	(((section) & 0x3) << 1)

#define F10_NB_ARRAY_DATA		0xBC
#define F10_NB_ARR_ECC_WR_REQ		BIT(17)
#define SET_NB_DRAM_INJECTION_WRITE(inj)  \
					(BIT(((inj.word) & 0xF) + 20) | \
					F10_NB_ARR_ECC_WR_REQ | inj.bit_map)
#define SET_NB_DRAM_INJECTION_READ(inj)  \
					(BIT(((inj.word) & 0xF) + 20) | \
					BIT(16) |  inj.bit_map)


#define NBCAP				0xE8
#define NBCAP_CHIPKILL			BIT(4)
#define NBCAP_SECDED			BIT(3)
#define NBCAP_DCT_DUAL			BIT(0)

#define EXT_NB_MCA_CFG			0x180

 
#define MSR_MCGCTL_NBE			BIT(4)

 

 
#define DF_DHAR				0x104

 
#define UMCCH_BASE_ADDR			0x0
#define UMCCH_BASE_ADDR_SEC		0x10
#define UMCCH_ADDR_MASK			0x20
#define UMCCH_ADDR_MASK_SEC		0x28
#define UMCCH_ADDR_MASK_SEC_DDR5	0x30
#define UMCCH_ADDR_CFG			0x30
#define UMCCH_ADDR_CFG_DDR5		0x40
#define UMCCH_DIMM_CFG			0x80
#define UMCCH_DIMM_CFG_DDR5		0x90
#define UMCCH_UMC_CFG			0x100
#define UMCCH_SDP_CTRL			0x104
#define UMCCH_ECC_CTRL			0x14C
#define UMCCH_ECC_BAD_SYMBOL		0xD90
#define UMCCH_UMC_CAP			0xDF0
#define UMCCH_UMC_CAP_HI		0xDF4

 
#define UMC_ECC_CHIPKILL_CAP		BIT(31)
#define UMC_ECC_ENABLED			BIT(30)

#define UMC_SDP_INIT			BIT(31)

 
struct error_injection {
	u32	 section;
	u32	 word;
	u32	 bit_map;
};

 
struct reg_pair {
	u32 lo, hi;
};

 
struct dram_range {
	struct reg_pair base;
	struct reg_pair lim;
};

 
struct chip_select {
	u32 csbases[NUM_CHIPSELECTS];
	u32 csbases_sec[NUM_CHIPSELECTS];
	u8 b_cnt;

	u32 csmasks[NUM_CHIPSELECTS];
	u32 csmasks_sec[NUM_CHIPSELECTS];
	u8 m_cnt;
};

struct amd64_umc {
	u32 dimm_cfg;		 
	u32 umc_cfg;		 
	u32 sdp_ctrl;		 
	u32 ecc_ctrl;		 
	u32 umc_cap_hi;		 

	 
	enum mem_type dram_type;
};

struct amd64_family_flags {
	 
	__u64 zn_regs_v2	: 1,

	      __reserved	: 63;
};

struct amd64_pvt {
	struct low_ops *ops;

	 
	struct pci_dev *F1, *F2, *F3;

	u16 mc_node_id;		 
	u8 fam;			 
	u8 model;		 
	u8 stepping;		 

	int ext_model;		 

	 
	u32 dclr0;		 
	u32 dclr1;		 
	u32 dchr0;		 
	u32 dchr1;		 
	u32 nbcap;		 
	u32 nbcfg;		 
	u32 ext_nbcfg;		 
	u32 dhar;		 
	u32 dbam0;		 
	u32 dbam1;		 

	 
	struct chip_select csels[NUM_CONTROLLERS];

	 
	struct dram_range ranges[DRAM_RANGES];

	u64 top_mem;		 
	u64 top_mem2;		 

	u32 dct_sel_lo;		 
	u32 dct_sel_hi;		 
	u32 online_spare;	 

	 
	u8 ecc_sym_sz;

	const char *ctl_name;
	u16 f1_id, f2_id;
	 
	u8 max_mcs;

	struct amd64_family_flags flags;
	 
	struct error_injection injection;

	 
	enum mem_type dram_type;

	struct amd64_umc *umc;	 
};

enum err_codes {
	DECODE_OK	=  0,
	ERR_NODE	= -1,
	ERR_CSROW	= -2,
	ERR_CHANNEL	= -3,
	ERR_SYND	= -4,
	ERR_NORM_ADDR	= -5,
};

struct err_info {
	int err_code;
	struct mem_ctl_info *src_mci;
	int csrow;
	int channel;
	u16 syndrome;
	u32 page;
	u32 offset;
};

static inline u32 get_umc_base(u8 channel)
{
	 
	return 0x50000 + (channel << 20);
}

static inline u64 get_dram_base(struct amd64_pvt *pvt, u8 i)
{
	u64 addr = ((u64)pvt->ranges[i].base.lo & 0xffff0000) << 8;

	if (boot_cpu_data.x86 == 0xf)
		return addr;

	return (((u64)pvt->ranges[i].base.hi & 0x000000ff) << 40) | addr;
}

static inline u64 get_dram_limit(struct amd64_pvt *pvt, u8 i)
{
	u64 lim = (((u64)pvt->ranges[i].lim.lo & 0xffff0000) << 8) | 0x00ffffff;

	if (boot_cpu_data.x86 == 0xf)
		return lim;

	return (((u64)pvt->ranges[i].lim.hi & 0x000000ff) << 40) | lim;
}

static inline u16 extract_syndrome(u64 status)
{
	return ((status >> 47) & 0xff) | ((status >> 16) & 0xff00);
}

static inline u8 dct_sel_interleave_addr(struct amd64_pvt *pvt)
{
	if (pvt->fam == 0x15 && pvt->model >= 0x30)
		return (((pvt->dct_sel_hi >> 9) & 0x1) << 2) |
			((pvt->dct_sel_lo >> 6) & 0x3);

	return	((pvt)->dct_sel_lo >> 6) & 0x3;
}
 
struct ecc_settings {
	u32 old_nbctl;
	bool nbctl_valid;

	struct flags {
		unsigned long nb_mce_enable:1;
		unsigned long nb_ecc_prev:1;
	} flags;
};

 
struct low_ops {
	void (*map_sysaddr_to_csrow)(struct mem_ctl_info *mci, u64 sys_addr,
				     struct err_info *err);
	int  (*dbam_to_cs)(struct amd64_pvt *pvt, u8 dct,
			   unsigned int cs_mode, int cs_mask_nr);
	int (*hw_info_get)(struct amd64_pvt *pvt);
	bool (*ecc_enabled)(struct amd64_pvt *pvt);
	void (*setup_mci_misc_attrs)(struct mem_ctl_info *mci);
	void (*dump_misc_regs)(struct amd64_pvt *pvt);
	void (*get_err_info)(struct mce *m, struct err_info *err);
};

int __amd64_read_pci_cfg_dword(struct pci_dev *pdev, int offset,
			       u32 *val, const char *func);
int __amd64_write_pci_cfg_dword(struct pci_dev *pdev, int offset,
				u32 val, const char *func);

#define amd64_read_pci_cfg(pdev, offset, val)	\
	__amd64_read_pci_cfg_dword(pdev, offset, val, __func__)

#define amd64_write_pci_cfg(pdev, offset, val)	\
	__amd64_write_pci_cfg_dword(pdev, offset, val, __func__)

#define to_mci(k) container_of(k, struct mem_ctl_info, dev)

 
static inline void disable_caches(void *dummy)
{
	write_cr0(read_cr0() | X86_CR0_CD);
	wbinvd();
}

static inline void enable_caches(void *dummy)
{
	write_cr0(read_cr0() & ~X86_CR0_CD);
}

static inline u8 dram_intlv_en(struct amd64_pvt *pvt, unsigned int i)
{
	if (pvt->fam == 0x15 && pvt->model >= 0x30) {
		u32 tmp;
		amd64_read_pci_cfg(pvt->F1, DRAM_CONT_LIMIT, &tmp);
		return (u8) tmp & 0xF;
	}
	return (u8) (pvt->ranges[i].base.lo >> 8) & 0x7;
}

static inline u8 dhar_valid(struct amd64_pvt *pvt)
{
	if (pvt->fam == 0x15 && pvt->model >= 0x30) {
		u32 tmp;
		amd64_read_pci_cfg(pvt->F1, DRAM_CONT_BASE, &tmp);
		return (tmp >> 1) & BIT(0);
	}
	return (pvt)->dhar & BIT(0);
}

static inline u32 dct_sel_baseaddr(struct amd64_pvt *pvt)
{
	if (pvt->fam == 0x15 && pvt->model >= 0x30) {
		u32 tmp;
		amd64_read_pci_cfg(pvt->F1, DRAM_CONT_BASE, &tmp);
		return (tmp >> 11) & 0x1FFF;
	}
	return (pvt)->dct_sel_lo & 0xFFFFF800;
}
