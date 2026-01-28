#ifndef _NTB_INTEL_GEN3_H_
#define _NTB_INTEL_GEN3_H_
#include "ntb_hw_intel.h"
#define GEN3_IMBAR1SZ_OFFSET		0x00d0
#define GEN3_IMBAR2SZ_OFFSET		0x00d1
#define GEN3_EMBAR1SZ_OFFSET		0x00d2
#define GEN3_EMBAR2SZ_OFFSET		0x00d3
#define GEN3_DEVCTRL_OFFSET		0x0098
#define GEN3_DEVSTS_OFFSET		0x009a
#define GEN3_UNCERRSTS_OFFSET		0x014c
#define GEN3_CORERRSTS_OFFSET		0x0158
#define GEN3_LINK_STATUS_OFFSET		0x01a2
#define GEN3_NTBCNTL_OFFSET		0x0000
#define GEN3_IMBAR1XBASE_OFFSET		0x0010		 
#define GEN3_IMBAR1XLMT_OFFSET		0x0018		 
#define GEN3_IMBAR2XBASE_OFFSET		0x0020		 
#define GEN3_IMBAR2XLMT_OFFSET		0x0028		 
#define GEN3_IM_INT_STATUS_OFFSET	0x0040
#define GEN3_IM_INT_DISABLE_OFFSET	0x0048
#define GEN3_IM_SPAD_OFFSET		0x0080		 
#define GEN3_USMEMMISS_OFFSET		0x0070
#define GEN3_INTVEC_OFFSET		0x00d0
#define GEN3_IM_DOORBELL_OFFSET		0x0100		 
#define GEN3_B2B_SPAD_OFFSET		0x0180		 
#define GEN3_EMBAR0XBASE_OFFSET		0x4008		 
#define GEN3_EMBAR1XBASE_OFFSET		0x4010		 
#define GEN3_EMBAR1XLMT_OFFSET		0x4018		 
#define GEN3_EMBAR2XBASE_OFFSET		0x4020		 
#define GEN3_EMBAR2XLMT_OFFSET		0x4028		 
#define GEN3_EM_INT_STATUS_OFFSET	0x4040
#define GEN3_EM_INT_DISABLE_OFFSET	0x4048
#define GEN3_EM_SPAD_OFFSET		0x4080		 
#define GEN3_EM_DOORBELL_OFFSET		0x4100		 
#define GEN3_SPCICMD_OFFSET		0x4504		 
#define GEN3_EMBAR0_OFFSET		0x4510		 
#define GEN3_EMBAR1_OFFSET		0x4518		 
#define GEN3_EMBAR2_OFFSET		0x4520		 
#define GEN3_DB_COUNT			32
#define GEN3_DB_LINK			32
#define GEN3_DB_LINK_BIT		BIT_ULL(GEN3_DB_LINK)
#define GEN3_DB_MSIX_VECTOR_COUNT	33
#define GEN3_DB_MSIX_VECTOR_SHIFT	1
#define GEN3_DB_TOTAL_SHIFT		33
#define GEN3_SPAD_COUNT			16
static inline u64 gen3_db_ioread(const void __iomem *mmio)
{
	return ioread64(mmio);
}
static inline void gen3_db_iowrite(u64 bits, void __iomem *mmio)
{
	iowrite64(bits, mmio);
}
ssize_t ndev_ntb3_debugfs_read(struct file *filp, char __user *ubuf,
				      size_t count, loff_t *offp);
int gen3_init_dev(struct intel_ntb_dev *ndev);
int intel_ntb3_link_enable(struct ntb_dev *ntb, enum ntb_speed max_speed,
		enum ntb_width max_width);
u64 intel_ntb3_db_read(struct ntb_dev *ntb);
int intel_ntb3_db_clear(struct ntb_dev *ntb, u64 db_bits);
int intel_ntb3_peer_db_set(struct ntb_dev *ntb, u64 db_bits);
int intel_ntb3_peer_db_addr(struct ntb_dev *ntb, phys_addr_t *db_addr,
				resource_size_t *db_size,
				u64 *db_data, int db_bit);
extern const struct ntb_dev_ops intel_ntb3_ops;
#endif
