 

#ifndef NTB_HW_AMD_H
#define NTB_HW_AMD_H

#include <linux/ntb.h>
#include <linux/pci.h>

#define AMD_LINK_HB_TIMEOUT	msecs_to_jiffies(1000)
#define NTB_LNK_STA_SPEED_MASK	0x000F0000
#define NTB_LNK_STA_WIDTH_MASK	0x03F00000
#define NTB_LNK_STA_SPEED(x)	(((x) & NTB_LNK_STA_SPEED_MASK) >> 16)
#define NTB_LNK_STA_WIDTH(x)	(((x) & NTB_LNK_STA_WIDTH_MASK) >> 20)

#ifndef read64
#ifdef readq
#define read64 readq
#else
#define read64 _read64
static inline u64 _read64(void __iomem *mmio)
{
	u64 low, high;

	low = readl(mmio);
	high = readl(mmio + sizeof(u32));
	return low | (high << 32);
}
#endif
#endif

#ifndef write64
#ifdef writeq
#define write64 writeq
#else
#define write64 _write64
static inline void _write64(u64 val, void __iomem *mmio)
{
	writel(val, mmio);
	writel(val >> 32, mmio + sizeof(u32));
}
#endif
#endif

enum {
	 
	AMD_DB_CNT		= 16,
	AMD_MSIX_VECTOR_CNT	= 24,
	AMD_SPADS_CNT		= 16,

	 
	AMD_CNTL_OFFSET		= 0x200,

	 
	PMM_REG_CTL		= BIT(21),
	SMM_REG_CTL		= BIT(20),
	SMM_REG_ACC_PATH	= BIT(18),
	PMM_REG_ACC_PATH	= BIT(17),
	NTB_CLK_EN		= BIT(16),

	AMD_STA_OFFSET		= 0x204,
	AMD_PGSLV_OFFSET	= 0x208,
	AMD_SPAD_MUX_OFFSET	= 0x20C,
	AMD_SPAD_OFFSET		= 0x210,
	AMD_RSMU_HCID		= 0x250,
	AMD_RSMU_SIID		= 0x254,
	AMD_PSION_OFFSET	= 0x300,
	AMD_SSION_OFFSET	= 0x330,
	AMD_MMINDEX_OFFSET	= 0x400,
	AMD_MMDATA_OFFSET	= 0x404,
	AMD_SIDEINFO_OFFSET	= 0x408,

	AMD_SIDE_MASK		= BIT(0),
	AMD_SIDE_READY		= BIT(1),

	 
	AMD_ROMBARLMT_OFFSET	= 0x410,
	AMD_BAR1LMT_OFFSET	= 0x414,
	AMD_BAR23LMT_OFFSET	= 0x418,
	AMD_BAR45LMT_OFFSET	= 0x420,
	 
	AMD_POMBARXLAT_OFFSET	= 0x428,
	AMD_BAR1XLAT_OFFSET	= 0x430,
	AMD_BAR23XLAT_OFFSET	= 0x438,
	AMD_BAR45XLAT_OFFSET	= 0x440,
	 
	AMD_DBFM_OFFSET		= 0x450,
	AMD_DBREQ_OFFSET	= 0x454,
	AMD_MIRRDBSTAT_OFFSET	= 0x458,
	AMD_DBMASK_OFFSET	= 0x45C,
	AMD_DBSTAT_OFFSET	= 0x460,
	AMD_INTMASK_OFFSET	= 0x470,
	AMD_INTSTAT_OFFSET	= 0x474,

	 
	AMD_PEER_FLUSH_EVENT	= BIT(0),
	AMD_PEER_RESET_EVENT	= BIT(1),
	AMD_PEER_D3_EVENT	= BIT(2),
	AMD_PEER_PMETO_EVENT	= BIT(3),
	AMD_PEER_D0_EVENT	= BIT(4),
	AMD_LINK_UP_EVENT	= BIT(5),
	AMD_LINK_DOWN_EVENT	= BIT(6),
	AMD_EVENT_INTMASK	= (AMD_PEER_FLUSH_EVENT |
				AMD_PEER_RESET_EVENT | AMD_PEER_D3_EVENT |
				AMD_PEER_PMETO_EVENT | AMD_PEER_D0_EVENT |
				AMD_LINK_UP_EVENT | AMD_LINK_DOWN_EVENT),

	AMD_PMESTAT_OFFSET	= 0x480,
	AMD_PMSGTRIG_OFFSET	= 0x490,
	AMD_LTRLATENCY_OFFSET	= 0x494,
	AMD_FLUSHTRIG_OFFSET	= 0x498,

	 
	AMD_SMUACK_OFFSET	= 0x4A0,
	AMD_SINRST_OFFSET	= 0x4A4,
	AMD_RSPNUM_OFFSET	= 0x4A8,
	AMD_SMU_SPADMUTEX	= 0x4B0,
	AMD_SMU_SPADOFFSET	= 0x4B4,

	AMD_PEER_OFFSET		= 0x400,
};

struct ntb_dev_data {
	const unsigned char mw_count;
	const unsigned int mw_idx;
};

struct amd_ntb_dev;

struct amd_ntb_vec {
	struct amd_ntb_dev	*ndev;
	int			num;
};

struct amd_ntb_dev {
	struct ntb_dev ntb;

	u32 ntb_side;
	u32 lnk_sta;
	u32 cntl_sta;
	u32 peer_sta;

	struct ntb_dev_data *dev_data;
	unsigned char mw_count;
	unsigned char spad_count;
	unsigned char db_count;
	unsigned char msix_vec_count;

	u64 db_valid_mask;
	u64 db_mask;
	u64 db_last_bit;
	u32 int_mask;

	struct msix_entry *msix;
	struct amd_ntb_vec *vec;

	 
	spinlock_t db_mask_lock;

	void __iomem *self_mmio;
	void __iomem *peer_mmio;
	unsigned int self_spad;
	unsigned int peer_spad;

	struct delayed_work hb_timer;

	struct dentry *debugfs_dir;
	struct dentry *debugfs_info;
};

#define ntb_ndev(__ntb) container_of(__ntb, struct amd_ntb_dev, ntb)
#define hb_ndev(__work) container_of(__work, struct amd_ntb_dev, hb_timer.work)

static void amd_set_side_info_reg(struct amd_ntb_dev *ndev, bool peer);
static void amd_clear_side_info_reg(struct amd_ntb_dev *ndev, bool peer);
static int amd_poll_link(struct amd_ntb_dev *ndev);

#endif
