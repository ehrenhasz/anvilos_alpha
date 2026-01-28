#ifndef _POWERPC_SYSDEV_FSL_MSI_H
#define _POWERPC_SYSDEV_FSL_MSI_H
#include <linux/of.h>
#include <asm/msi_bitmap.h>
#define NR_MSI_REG_MSIIR	8   
#define NR_MSI_REG_MSIIR1	16  
#define NR_MSI_REG_MAX		NR_MSI_REG_MSIIR1
#define IRQS_PER_MSI_REG	32
#define NR_MSI_IRQS_MAX	(NR_MSI_REG_MAX * IRQS_PER_MSI_REG)
#define FSL_PIC_IP_MASK   0x0000000F
#define FSL_PIC_IP_MPIC   0x00000001
#define FSL_PIC_IP_IPIC   0x00000002
#define FSL_PIC_IP_VMPIC  0x00000003
#define MSI_HW_ERRATA_ENDIAN 0x00000010
struct fsl_msi_cascade_data;
struct fsl_msi {
	struct irq_domain *irqhost;
	unsigned long cascade_irq;
	u32 msiir_offset;  
	u32 ibs_shift;  
	u32 srs_shift;  
	void __iomem *msi_regs;
	u32 feature;
	struct fsl_msi_cascade_data *cascade_array[NR_MSI_REG_MAX];
	struct msi_bitmap bitmap;
	struct list_head list;           
	phandle phandle;
};
#endif  
