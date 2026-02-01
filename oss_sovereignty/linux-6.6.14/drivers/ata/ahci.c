
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/dmi.h>
#include <linux/gfp.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <linux/libata.h>
#include <linux/ahci-remap.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include "ahci.h"

#define DRV_NAME	"ahci"
#define DRV_VERSION	"3.0"

enum {
	AHCI_PCI_BAR_STA2X11	= 0,
	AHCI_PCI_BAR_CAVIUM	= 0,
	AHCI_PCI_BAR_LOONGSON	= 0,
	AHCI_PCI_BAR_ENMOTUS	= 2,
	AHCI_PCI_BAR_CAVIUM_GEN5	= 4,
	AHCI_PCI_BAR_STANDARD	= 5,
};

enum board_ids {
	 
	board_ahci,
	board_ahci_ign_iferr,
	board_ahci_low_power,
	board_ahci_no_debounce_delay,
	board_ahci_nomsi,
	board_ahci_noncq,
	board_ahci_nosntf,
	board_ahci_yes_fbs,

	 
	board_ahci_al,
	board_ahci_avn,
	board_ahci_mcp65,
	board_ahci_mcp77,
	board_ahci_mcp89,
	board_ahci_mv,
	board_ahci_sb600,
	board_ahci_sb700,	 
	board_ahci_vt8251,

	 
	board_ahci_pcs7,

	 
	board_ahci_mcp_linux	= board_ahci_mcp65,
	board_ahci_mcp67	= board_ahci_mcp65,
	board_ahci_mcp73	= board_ahci_mcp65,
	board_ahci_mcp79	= board_ahci_mcp77,
};

static int ahci_init_one(struct pci_dev *pdev, const struct pci_device_id *ent);
static void ahci_remove_one(struct pci_dev *dev);
static void ahci_shutdown_one(struct pci_dev *dev);
static void ahci_intel_pcs_quirk(struct pci_dev *pdev, struct ahci_host_priv *hpriv);
static int ahci_vt8251_hardreset(struct ata_link *link, unsigned int *class,
				 unsigned long deadline);
static int ahci_avn_hardreset(struct ata_link *link, unsigned int *class,
			      unsigned long deadline);
static void ahci_mcp89_apple_enable(struct pci_dev *pdev);
static bool is_mcp89_apple(struct pci_dev *pdev);
static int ahci_p5wdh_hardreset(struct ata_link *link, unsigned int *class,
				unsigned long deadline);
#ifdef CONFIG_PM
static int ahci_pci_device_runtime_suspend(struct device *dev);
static int ahci_pci_device_runtime_resume(struct device *dev);
#ifdef CONFIG_PM_SLEEP
static int ahci_pci_device_suspend(struct device *dev);
static int ahci_pci_device_resume(struct device *dev);
#endif
#endif  

static const struct scsi_host_template ahci_sht = {
	AHCI_SHT("ahci"),
};

static struct ata_port_operations ahci_vt8251_ops = {
	.inherits		= &ahci_ops,
	.hardreset		= ahci_vt8251_hardreset,
};

static struct ata_port_operations ahci_p5wdh_ops = {
	.inherits		= &ahci_ops,
	.hardreset		= ahci_p5wdh_hardreset,
};

static struct ata_port_operations ahci_avn_ops = {
	.inherits		= &ahci_ops,
	.hardreset		= ahci_avn_hardreset,
};

static const struct ata_port_info ahci_port_info[] = {
	 
	[board_ahci] = {
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_ign_iferr] = {
		AHCI_HFLAGS	(AHCI_HFLAG_IGN_IRQ_IF_ERR),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_low_power] = {
		AHCI_HFLAGS	(AHCI_HFLAG_USE_LPM_POLICY),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_no_debounce_delay] = {
		.flags		= AHCI_FLAG_COMMON,
		.link_flags	= ATA_LFLAG_NO_DEBOUNCE_DELAY,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_nomsi] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_MSI),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_noncq] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_NCQ),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_nosntf] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_SNTF),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_yes_fbs] = {
		AHCI_HFLAGS	(AHCI_HFLAG_YES_FBS),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	 
	[board_ahci_al] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_PMP | AHCI_HFLAG_NO_MSI),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_avn] = {
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_avn_ops,
	},
	[board_ahci_mcp65] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_FPDMA_AA | AHCI_HFLAG_NO_PMP |
				 AHCI_HFLAG_YES_NCQ),
		.flags		= AHCI_FLAG_COMMON | ATA_FLAG_NO_DIPM,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_mcp77] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_FPDMA_AA | AHCI_HFLAG_NO_PMP),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_mcp89] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_FPDMA_AA),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_mv] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_NCQ | AHCI_HFLAG_NO_MSI |
				 AHCI_HFLAG_MV_PATA | AHCI_HFLAG_NO_PMP),
		.flags		= ATA_FLAG_SATA | ATA_FLAG_PIO_DMA,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
	[board_ahci_sb600] = {
		AHCI_HFLAGS	(AHCI_HFLAG_IGN_SERR_INTERNAL |
				 AHCI_HFLAG_NO_MSI | AHCI_HFLAG_SECT255 |
				 AHCI_HFLAG_32BIT_ONLY),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_pmp_retry_srst_ops,
	},
	[board_ahci_sb700] = {	 
		AHCI_HFLAGS	(AHCI_HFLAG_IGN_SERR_INTERNAL),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_pmp_retry_srst_ops,
	},
	[board_ahci_vt8251] = {
		AHCI_HFLAGS	(AHCI_HFLAG_NO_NCQ | AHCI_HFLAG_NO_PMP),
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_vt8251_ops,
	},
	[board_ahci_pcs7] = {
		.flags		= AHCI_FLAG_COMMON,
		.pio_mask	= ATA_PIO4,
		.udma_mask	= ATA_UDMA6,
		.port_ops	= &ahci_ops,
	},
};

static const struct pci_device_id ahci_pci_tbl[] = {
	 
	{ PCI_VDEVICE(INTEL, 0x06d6), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2652), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2653), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x27c1), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x27c5), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x27c3), board_ahci },  
	{ PCI_VDEVICE(AL, 0x5288), board_ahci_ign_iferr },  
	{ PCI_VDEVICE(INTEL, 0x2681), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2682), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2683), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x27c6), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2821), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2822), board_ahci_nosntf },  
	{ PCI_VDEVICE(INTEL, 0x2824), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2829), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x282a), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2922), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2923), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2924), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2925), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2927), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2929), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x292a), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x292b), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x292c), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x292f), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x294d), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x294e), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x502a), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x502b), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x3a05), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x3a22), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x3a25), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x3b22), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x3b23), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x3b24), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x3b25), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x3b29), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x3b2b), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x3b2c), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x3b2f), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x19b0), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19b1), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19b2), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19b3), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19b4), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19b5), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19b6), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19b7), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19bE), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19bF), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19c0), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19c1), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19c2), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19c3), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19c4), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19c5), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19c6), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19c7), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19cE), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x19cF), board_ahci_pcs7 },  
	{ PCI_VDEVICE(INTEL, 0x1c02), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1c03), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x1c04), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1c05), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x1c06), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1c07), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1d02), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1d04), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1d06), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2323), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1e02), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1e03), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x1e04), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1e05), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1e06), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1e07), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x1e0e), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8c02), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8c03), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x8c04), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8c05), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x8c06), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8c07), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x8c0e), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8c0f), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9c02), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9c03), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9c04), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9c05), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9c06), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9c07), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9c0e), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9c0f), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9dd3), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x1f22), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1f23), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1f24), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1f25), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1f26), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1f27), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1f2e), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1f2f), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x1f32), board_ahci_avn },  
	{ PCI_VDEVICE(INTEL, 0x1f33), board_ahci_avn },  
	{ PCI_VDEVICE(INTEL, 0x1f34), board_ahci_avn },  
	{ PCI_VDEVICE(INTEL, 0x1f35), board_ahci_avn },  
	{ PCI_VDEVICE(INTEL, 0x1f36), board_ahci_avn },  
	{ PCI_VDEVICE(INTEL, 0x1f37), board_ahci_avn },  
	{ PCI_VDEVICE(INTEL, 0x1f3e), board_ahci_avn },  
	{ PCI_VDEVICE(INTEL, 0x1f3f), board_ahci_avn },  
	{ PCI_VDEVICE(INTEL, 0x2823), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2826), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x2827), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x282f), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x43d4), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x43d5), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x43d6), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x43d7), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8d02), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8d04), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8d06), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8d0e), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8d62), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8d64), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8d66), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8d6e), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x23a3), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x9c83), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9c85), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9c87), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9c8f), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x8c82), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8c83), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x8c84), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8c85), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x8c86), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8c87), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x8c8e), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x8c8f), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9d03), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9d05), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x9d07), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0xa102), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0xa103), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0xa105), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0xa106), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0xa107), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0xa10f), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0xa182), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0xa186), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0xa1d2), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0xa1d6), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0xa202), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0xa206), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0xa252), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0xa256), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0xa356), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x06d7), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0xa386), board_ahci },  
	{ PCI_VDEVICE(INTEL, 0x0f22), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x0f23), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x22a3), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x5ae3), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x34d3), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x02d3), board_ahci_low_power },  
	{ PCI_VDEVICE(INTEL, 0x02d7), board_ahci_low_power },  
	 

	 
	{ PCI_VENDOR_ID_JMICRON, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	  PCI_CLASS_STORAGE_SATA_AHCI, 0xffffff, board_ahci_ign_iferr },
	 
	{ PCI_VDEVICE(JMICRON, 0x2362), board_ahci_ign_iferr },
	{ PCI_VDEVICE(JMICRON, 0x236f), board_ahci_ign_iferr },
	 

	 
	{ PCI_VDEVICE(ATI, 0x4380), board_ahci_sb600 },  
	{ PCI_VDEVICE(ATI, 0x4390), board_ahci_sb700 },  
	{ PCI_VDEVICE(ATI, 0x4391), board_ahci_sb700 },  
	{ PCI_VDEVICE(ATI, 0x4392), board_ahci_sb700 },  
	{ PCI_VDEVICE(ATI, 0x4393), board_ahci_sb700 },  
	{ PCI_VDEVICE(ATI, 0x4394), board_ahci_sb700 },  
	{ PCI_VDEVICE(ATI, 0x4395), board_ahci_sb700 },  

	 
	{ PCI_DEVICE(PCI_VENDOR_ID_AMAZON_ANNAPURNA_LABS, 0x0031),
		.class = PCI_CLASS_STORAGE_SATA_AHCI,
		.class_mask = 0xffffff,
		board_ahci_al },
	 
	{ PCI_VDEVICE(AMD, 0x7800), board_ahci },  
	{ PCI_VDEVICE(AMD, 0x7801), board_ahci_no_debounce_delay },  
	{ PCI_VDEVICE(AMD, 0x7900), board_ahci },  
	{ PCI_VDEVICE(AMD, 0x7901), board_ahci_low_power },  
	 
	{ PCI_VENDOR_ID_AMD, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	  PCI_CLASS_STORAGE_RAID << 8, 0xffffff, board_ahci },

	 
	{ PCI_VENDOR_ID_INTEL, PCI_ANY_ID, PCI_SUBVENDOR_ID_DELL, PCI_ANY_ID,
	  PCI_CLASS_STORAGE_RAID << 8, 0xffffff, board_ahci },

	 
	{ PCI_VDEVICE(VIA, 0x3349), board_ahci_vt8251 },  
	{ PCI_VDEVICE(VIA, 0x6287), board_ahci_vt8251 },  

	 
	{ PCI_VDEVICE(NVIDIA, 0x044c), board_ahci_mcp65 },	 
	{ PCI_VDEVICE(NVIDIA, 0x044d), board_ahci_mcp65 },	 
	{ PCI_VDEVICE(NVIDIA, 0x044e), board_ahci_mcp65 },	 
	{ PCI_VDEVICE(NVIDIA, 0x044f), board_ahci_mcp65 },	 
	{ PCI_VDEVICE(NVIDIA, 0x045c), board_ahci_mcp65 },	 
	{ PCI_VDEVICE(NVIDIA, 0x045d), board_ahci_mcp65 },	 
	{ PCI_VDEVICE(NVIDIA, 0x045e), board_ahci_mcp65 },	 
	{ PCI_VDEVICE(NVIDIA, 0x045f), board_ahci_mcp65 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0550), board_ahci_mcp67 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0551), board_ahci_mcp67 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0552), board_ahci_mcp67 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0553), board_ahci_mcp67 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0554), board_ahci_mcp67 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0555), board_ahci_mcp67 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0556), board_ahci_mcp67 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0557), board_ahci_mcp67 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0558), board_ahci_mcp67 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0559), board_ahci_mcp67 },	 
	{ PCI_VDEVICE(NVIDIA, 0x055a), board_ahci_mcp67 },	 
	{ PCI_VDEVICE(NVIDIA, 0x055b), board_ahci_mcp67 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0580), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x0581), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x0582), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x0583), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x0584), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x0585), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x0586), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x0587), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x0588), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x0589), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x058a), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x058b), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x058c), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x058d), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x058e), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x058f), board_ahci_mcp_linux },	 
	{ PCI_VDEVICE(NVIDIA, 0x07f0), board_ahci_mcp73 },	 
	{ PCI_VDEVICE(NVIDIA, 0x07f1), board_ahci_mcp73 },	 
	{ PCI_VDEVICE(NVIDIA, 0x07f2), board_ahci_mcp73 },	 
	{ PCI_VDEVICE(NVIDIA, 0x07f3), board_ahci_mcp73 },	 
	{ PCI_VDEVICE(NVIDIA, 0x07f4), board_ahci_mcp73 },	 
	{ PCI_VDEVICE(NVIDIA, 0x07f5), board_ahci_mcp73 },	 
	{ PCI_VDEVICE(NVIDIA, 0x07f6), board_ahci_mcp73 },	 
	{ PCI_VDEVICE(NVIDIA, 0x07f7), board_ahci_mcp73 },	 
	{ PCI_VDEVICE(NVIDIA, 0x07f8), board_ahci_mcp73 },	 
	{ PCI_VDEVICE(NVIDIA, 0x07f9), board_ahci_mcp73 },	 
	{ PCI_VDEVICE(NVIDIA, 0x07fa), board_ahci_mcp73 },	 
	{ PCI_VDEVICE(NVIDIA, 0x07fb), board_ahci_mcp73 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ad0), board_ahci_mcp77 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ad1), board_ahci_mcp77 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ad2), board_ahci_mcp77 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ad3), board_ahci_mcp77 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ad4), board_ahci_mcp77 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ad5), board_ahci_mcp77 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ad6), board_ahci_mcp77 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ad7), board_ahci_mcp77 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ad8), board_ahci_mcp77 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ad9), board_ahci_mcp77 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ada), board_ahci_mcp77 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0adb), board_ahci_mcp77 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ab4), board_ahci_mcp79 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ab5), board_ahci_mcp79 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ab6), board_ahci_mcp79 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ab7), board_ahci_mcp79 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ab8), board_ahci_mcp79 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0ab9), board_ahci_mcp79 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0aba), board_ahci_mcp79 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0abb), board_ahci_mcp79 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0abc), board_ahci_mcp79 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0abd), board_ahci_mcp79 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0abe), board_ahci_mcp79 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0abf), board_ahci_mcp79 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0d84), board_ahci_mcp89 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0d85), board_ahci_mcp89 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0d86), board_ahci_mcp89 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0d87), board_ahci_mcp89 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0d88), board_ahci_mcp89 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0d89), board_ahci_mcp89 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0d8a), board_ahci_mcp89 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0d8b), board_ahci_mcp89 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0d8c), board_ahci_mcp89 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0d8d), board_ahci_mcp89 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0d8e), board_ahci_mcp89 },	 
	{ PCI_VDEVICE(NVIDIA, 0x0d8f), board_ahci_mcp89 },	 

	 
	{ PCI_VDEVICE(SI, 0x1184), board_ahci },		 
	{ PCI_VDEVICE(SI, 0x1185), board_ahci },		 
	{ PCI_VDEVICE(SI, 0x0186), board_ahci },		 

	 
	{ PCI_VDEVICE(STMICRO, 0xCC06), board_ahci },		 

	 
	{ PCI_VDEVICE(MARVELL, 0x6145), board_ahci_mv },	 
	{ PCI_VDEVICE(MARVELL, 0x6121), board_ahci_mv },	 
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL_EXT, 0x9123),
	  .class = PCI_CLASS_STORAGE_SATA_AHCI,
	  .class_mask = 0xffffff,
	  .driver_data = board_ahci_yes_fbs },			 
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL_EXT, 0x9125),
	  .driver_data = board_ahci_yes_fbs },			 
	{ PCI_DEVICE_SUB(PCI_VENDOR_ID_MARVELL_EXT, 0x9178,
			 PCI_VENDOR_ID_MARVELL_EXT, 0x9170),
	  .driver_data = board_ahci_yes_fbs },			 
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL_EXT, 0x917a),
	  .driver_data = board_ahci_yes_fbs },			 
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL_EXT, 0x9172),
	  .driver_data = board_ahci_yes_fbs },			 
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL_EXT, 0x9182),
	  .driver_data = board_ahci_yes_fbs },			 
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL_EXT, 0x9192),
	  .driver_data = board_ahci_yes_fbs },			 
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL_EXT, 0x91a0),
	  .driver_data = board_ahci_yes_fbs },
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL_EXT, 0x91a2), 	 
	  .driver_data = board_ahci_yes_fbs },
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL_EXT, 0x91a3),
	  .driver_data = board_ahci_yes_fbs },
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL_EXT, 0x9230),
	  .driver_data = board_ahci_yes_fbs },
	{ PCI_DEVICE(PCI_VENDOR_ID_MARVELL_EXT, 0x9235),
	  .driver_data = board_ahci_no_debounce_delay },
	{ PCI_DEVICE(PCI_VENDOR_ID_TTI, 0x0642),  
	  .driver_data = board_ahci_yes_fbs },
	{ PCI_DEVICE(PCI_VENDOR_ID_TTI, 0x0645),  
	  .driver_data = board_ahci_yes_fbs },

	 
	{ PCI_VDEVICE(PROMISE, 0x3f20), board_ahci },	 
	{ PCI_VDEVICE(PROMISE, 0x3781), board_ahci },    

	 
	{ PCI_VDEVICE(ASMEDIA, 0x0601), board_ahci },	 
	{ PCI_VDEVICE(ASMEDIA, 0x0602), board_ahci },	 
	{ PCI_VDEVICE(ASMEDIA, 0x0611), board_ahci },	 
	{ PCI_VDEVICE(ASMEDIA, 0x0612), board_ahci },	 
	{ PCI_VDEVICE(ASMEDIA, 0x0621), board_ahci },    
	{ PCI_VDEVICE(ASMEDIA, 0x0622), board_ahci },    
	{ PCI_VDEVICE(ASMEDIA, 0x0624), board_ahci },    

	 
	{ PCI_VDEVICE(SAMSUNG, 0x1600), board_ahci_nomsi },
	{ PCI_VDEVICE(SAMSUNG, 0xa800), board_ahci_nomsi },

	 
	{ PCI_DEVICE(0x1c44, 0x8000), board_ahci },

	 
	{ PCI_VDEVICE(LOONGSON, 0x7a08), board_ahci },

	 
	{ PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	  PCI_CLASS_STORAGE_SATA_AHCI, 0xffffff, board_ahci },

	{ }	 
};

static const struct dev_pm_ops ahci_pci_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ahci_pci_device_suspend, ahci_pci_device_resume)
	SET_RUNTIME_PM_OPS(ahci_pci_device_runtime_suspend,
			   ahci_pci_device_runtime_resume, NULL)
};

static struct pci_driver ahci_pci_driver = {
	.name			= DRV_NAME,
	.id_table		= ahci_pci_tbl,
	.probe			= ahci_init_one,
	.remove			= ahci_remove_one,
	.shutdown		= ahci_shutdown_one,
	.driver = {
		.pm		= &ahci_pci_pm_ops,
	},
};

#if IS_ENABLED(CONFIG_PATA_MARVELL)
static int marvell_enable;
#else
static int marvell_enable = 1;
#endif
module_param(marvell_enable, int, 0644);
MODULE_PARM_DESC(marvell_enable, "Marvell SATA via AHCI (1 = enabled)");

static int mobile_lpm_policy = -1;
module_param(mobile_lpm_policy, int, 0644);
MODULE_PARM_DESC(mobile_lpm_policy, "Default LPM policy for mobile chipsets");

static void ahci_pci_save_initial_config(struct pci_dev *pdev,
					 struct ahci_host_priv *hpriv)
{
	if (pdev->vendor == PCI_VENDOR_ID_JMICRON && pdev->device == 0x2361) {
		dev_info(&pdev->dev, "JMB361 has only one port\n");
		hpriv->saved_port_map = 1;
	}

	 
	if (hpriv->flags & AHCI_HFLAG_MV_PATA) {
		if (pdev->device == 0x6121)
			hpriv->mask_port_map = 0x3;
		else
			hpriv->mask_port_map = 0xf;
		dev_info(&pdev->dev,
			  "Disabling your PATA port. Use the boot option 'ahci.marvell_enable=0' to avoid this.\n");
	}

	ahci_save_initial_config(&pdev->dev, hpriv);
}

static int ahci_pci_reset_controller(struct ata_host *host)
{
	struct pci_dev *pdev = to_pci_dev(host->dev);
	struct ahci_host_priv *hpriv = host->private_data;
	int rc;

	rc = ahci_reset_controller(host);
	if (rc)
		return rc;

	 
	ahci_intel_pcs_quirk(pdev, hpriv);

	return 0;
}

static void ahci_pci_init_controller(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
	struct pci_dev *pdev = to_pci_dev(host->dev);
	void __iomem *port_mmio;
	u32 tmp;
	int mv;

	if (hpriv->flags & AHCI_HFLAG_MV_PATA) {
		if (pdev->device == 0x6121)
			mv = 2;
		else
			mv = 4;
		port_mmio = __ahci_port_base(hpriv, mv);

		writel(0, port_mmio + PORT_IRQ_MASK);

		 
		tmp = readl(port_mmio + PORT_IRQ_STAT);
		dev_dbg(&pdev->dev, "PORT_IRQ_STAT 0x%x\n", tmp);
		if (tmp)
			writel(tmp, port_mmio + PORT_IRQ_STAT);
	}

	ahci_init_controller(host);
}

static int ahci_vt8251_hardreset(struct ata_link *link, unsigned int *class,
				 unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct ahci_host_priv *hpriv = ap->host->private_data;
	bool online;
	int rc;

	hpriv->stop_engine(ap);

	rc = sata_link_hardreset(link, sata_ehc_deb_timing(&link->eh_context),
				 deadline, &online, NULL);

	hpriv->start_engine(ap);

	 
	return online ? -EAGAIN : rc;
}

static int ahci_p5wdh_hardreset(struct ata_link *link, unsigned int *class,
				unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_host_priv *hpriv = ap->host->private_data;
	u8 *d2h_fis = pp->rx_fis + RX_FIS_D2H_REG;
	struct ata_taskfile tf;
	bool online;
	int rc;

	hpriv->stop_engine(ap);

	 
	ata_tf_init(link->device, &tf);
	tf.status = ATA_BUSY;
	ata_tf_to_fis(&tf, 0, 0, d2h_fis);

	rc = sata_link_hardreset(link, sata_ehc_deb_timing(&link->eh_context),
				 deadline, &online, NULL);

	hpriv->start_engine(ap);

	 
	if (online) {
		rc = ata_wait_after_reset(link, jiffies + 2 * HZ,
					  ahci_check_ready);
		if (rc)
			ahci_kick_engine(ap);
	}
	return rc;
}

 
static int ahci_avn_hardreset(struct ata_link *link, unsigned int *class,
			      unsigned long deadline)
{
	const unsigned int *timing = sata_ehc_deb_timing(&link->eh_context);
	struct ata_port *ap = link->ap;
	struct ahci_port_priv *pp = ap->private_data;
	struct ahci_host_priv *hpriv = ap->host->private_data;
	u8 *d2h_fis = pp->rx_fis + RX_FIS_D2H_REG;
	unsigned long tmo = deadline - jiffies;
	struct ata_taskfile tf;
	bool online;
	int rc, i;

	hpriv->stop_engine(ap);

	for (i = 0; i < 2; i++) {
		u16 val;
		u32 sstatus;
		int port = ap->port_no;
		struct ata_host *host = ap->host;
		struct pci_dev *pdev = to_pci_dev(host->dev);

		 
		ata_tf_init(link->device, &tf);
		tf.status = ATA_BUSY;
		ata_tf_to_fis(&tf, 0, 0, d2h_fis);

		rc = sata_link_hardreset(link, timing, deadline, &online,
				ahci_check_ready);

		if (sata_scr_read(link, SCR_STATUS, &sstatus) != 0 ||
				(sstatus & 0xf) != 1)
			break;

		ata_link_info(link,  "avn bounce port%d\n", port);

		pci_read_config_word(pdev, 0x92, &val);
		val &= ~(1 << port);
		pci_write_config_word(pdev, 0x92, val);
		ata_msleep(ap, 1000);
		val |= 1 << port;
		pci_write_config_word(pdev, 0x92, val);
		deadline += tmo;
	}

	hpriv->start_engine(ap);

	if (online)
		*class = ahci_dev_classify(ap);

	return rc;
}


#ifdef CONFIG_PM
static void ahci_pci_disable_interrupts(struct ata_host *host)
{
	struct ahci_host_priv *hpriv = host->private_data;
	void __iomem *mmio = hpriv->mmio;
	u32 ctl;

	 
	ctl = readl(mmio + HOST_CTL);
	ctl &= ~HOST_IRQ_EN;
	writel(ctl, mmio + HOST_CTL);
	readl(mmio + HOST_CTL);  
}

static int ahci_pci_device_runtime_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct ata_host *host = pci_get_drvdata(pdev);

	ahci_pci_disable_interrupts(host);
	return 0;
}

static int ahci_pci_device_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct ata_host *host = pci_get_drvdata(pdev);
	int rc;

	rc = ahci_pci_reset_controller(host);
	if (rc)
		return rc;
	ahci_pci_init_controller(host);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ahci_pci_device_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct ata_host *host = pci_get_drvdata(pdev);
	struct ahci_host_priv *hpriv = host->private_data;

	if (hpriv->flags & AHCI_HFLAG_NO_SUSPEND) {
		dev_err(&pdev->dev,
			"BIOS update required for suspend/resume\n");
		return -EIO;
	}

	ahci_pci_disable_interrupts(host);
	ata_host_suspend(host, PMSG_SUSPEND);
	return 0;
}

static int ahci_pci_device_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct ata_host *host = pci_get_drvdata(pdev);
	int rc;

	 
	if (is_mcp89_apple(pdev))
		ahci_mcp89_apple_enable(pdev);

	if (pdev->dev.power.power_state.event == PM_EVENT_SUSPEND) {
		rc = ahci_pci_reset_controller(host);
		if (rc)
			return rc;

		ahci_pci_init_controller(host);
	}

	ata_host_resume(host);

	return 0;
}
#endif

#endif  

static int ahci_configure_dma_masks(struct pci_dev *pdev, int using_dac)
{
	const int dma_bits = using_dac ? 64 : 32;
	int rc;

	 
	if (pdev->dma_mask && pdev->dma_mask < DMA_BIT_MASK(32))
		return 0;

	rc = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(dma_bits));
	if (rc)
		dev_err(&pdev->dev, "DMA enable failed\n");
	return rc;
}

static void ahci_pci_print_info(struct ata_host *host)
{
	struct pci_dev *pdev = to_pci_dev(host->dev);
	u16 cc;
	const char *scc_s;

	pci_read_config_word(pdev, 0x0a, &cc);
	if (cc == PCI_CLASS_STORAGE_IDE)
		scc_s = "IDE";
	else if (cc == PCI_CLASS_STORAGE_SATA)
		scc_s = "SATA";
	else if (cc == PCI_CLASS_STORAGE_RAID)
		scc_s = "RAID";
	else
		scc_s = "unknown";

	ahci_print_info(host, scc_s);
}

 
static void ahci_p5wdh_workaround(struct ata_host *host)
{
	static const struct dmi_system_id sysids[] = {
		{
			.ident = "P5W DH Deluxe",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR,
					  "ASUSTEK COMPUTER INC"),
				DMI_MATCH(DMI_PRODUCT_NAME, "P5W DH Deluxe"),
			},
		},
		{ }
	};
	struct pci_dev *pdev = to_pci_dev(host->dev);

	if (pdev->bus->number == 0 && pdev->devfn == PCI_DEVFN(0x1f, 2) &&
	    dmi_check_system(sysids)) {
		struct ata_port *ap = host->ports[1];

		dev_info(&pdev->dev,
			 "enabling ASUS P5W DH Deluxe on-board SIMG4726 workaround\n");

		ap->ops = &ahci_p5wdh_ops;
		ap->link.flags |= ATA_LFLAG_NO_SRST | ATA_LFLAG_ASSUME_ATA;
	}
}

 
static void ahci_mcp89_apple_enable(struct pci_dev *pdev)
{
	u32 val;

	printk(KERN_INFO "ahci: enabling MCP89 AHCI mode\n");

	pci_read_config_dword(pdev, 0xf8, &val);
	val |= 1 << 0x1b;
	 
	 
	pci_write_config_dword(pdev, 0xf8, val);

	pci_read_config_dword(pdev, 0x54c, &val);
	val |= 1 << 0xc;
	pci_write_config_dword(pdev, 0x54c, val);

	pci_read_config_dword(pdev, 0x4a4, &val);
	val &= 0xff;
	val |= 0x01060100;
	pci_write_config_dword(pdev, 0x4a4, val);

	pci_read_config_dword(pdev, 0x54c, &val);
	val &= ~(1 << 0xc);
	pci_write_config_dword(pdev, 0x54c, val);

	pci_read_config_dword(pdev, 0xf8, &val);
	val &= ~(1 << 0x1b);
	pci_write_config_dword(pdev, 0xf8, val);
}

static bool is_mcp89_apple(struct pci_dev *pdev)
{
	return pdev->vendor == PCI_VENDOR_ID_NVIDIA &&
		pdev->device == PCI_DEVICE_ID_NVIDIA_NFORCE_MCP89_SATA &&
		pdev->subsystem_vendor == PCI_VENDOR_ID_APPLE &&
		pdev->subsystem_device == 0xcb89;
}

 
static bool ahci_sb600_enable_64bit(struct pci_dev *pdev)
{
	static const struct dmi_system_id sysids[] = {
		 
		{
			.ident = "ASUS M2A-VM",
			.matches = {
				DMI_MATCH(DMI_BOARD_VENDOR,
					  "ASUSTeK Computer INC."),
				DMI_MATCH(DMI_BOARD_NAME, "M2A-VM"),
			},
			.driver_data = "20071026",	 
		},
		 
		{
			.ident = "MSI K9A2 Platinum",
			.matches = {
				DMI_MATCH(DMI_BOARD_VENDOR,
					  "MICRO-STAR INTER"),
				DMI_MATCH(DMI_BOARD_NAME, "MS-7376"),
			},
		},
		 
		{
			.ident = "MSI K9AGM2",
			.matches = {
				DMI_MATCH(DMI_BOARD_VENDOR,
					  "MICRO-STAR INTER"),
				DMI_MATCH(DMI_BOARD_NAME, "MS-7327"),
			},
		},
		 
		{
			.ident = "ASUS M3A",
			.matches = {
				DMI_MATCH(DMI_BOARD_VENDOR,
					  "ASUSTeK Computer INC."),
				DMI_MATCH(DMI_BOARD_NAME, "M3A"),
			},
		},
		{ }
	};
	const struct dmi_system_id *match;
	int year, month, date;
	char buf[9];

	match = dmi_first_match(sysids);
	if (pdev->bus->number != 0 || pdev->devfn != PCI_DEVFN(0x12, 0) ||
	    !match)
		return false;

	if (!match->driver_data)
		goto enable_64bit;

	dmi_get_date(DMI_BIOS_DATE, &year, &month, &date);
	snprintf(buf, sizeof(buf), "%04d%02d%02d", year, month, date);

	if (strcmp(buf, match->driver_data) >= 0)
		goto enable_64bit;
	else {
		dev_warn(&pdev->dev,
			 "%s: BIOS too old, forcing 32bit DMA, update BIOS\n",
			 match->ident);
		return false;
	}

enable_64bit:
	dev_warn(&pdev->dev, "%s: enabling 64bit DMA\n", match->ident);
	return true;
}

static bool ahci_broken_system_poweroff(struct pci_dev *pdev)
{
	static const struct dmi_system_id broken_systems[] = {
		{
			.ident = "HP Compaq nx6310",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
				DMI_MATCH(DMI_PRODUCT_NAME, "HP Compaq nx6310"),
			},
			 
			.driver_data = (void *)0x1FUL,
		},
		{
			.ident = "HP Compaq 6720s",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
				DMI_MATCH(DMI_PRODUCT_NAME, "HP Compaq 6720s"),
			},
			 
			.driver_data = (void *)0x1FUL,
		},

		{ }	 
	};
	const struct dmi_system_id *dmi = dmi_first_match(broken_systems);

	if (dmi) {
		unsigned long slot = (unsigned long)dmi->driver_data;
		 
		return slot == PCI_SLOT(pdev->devfn);
	}

	return false;
}

static bool ahci_broken_suspend(struct pci_dev *pdev)
{
	static const struct dmi_system_id sysids[] = {
		 
		{
			.ident = "dv4",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
				DMI_MATCH(DMI_PRODUCT_NAME,
					  "HP Pavilion dv4 Notebook PC"),
			},
			.driver_data = "20090105",	 
		},
		{
			.ident = "dv5",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
				DMI_MATCH(DMI_PRODUCT_NAME,
					  "HP Pavilion dv5 Notebook PC"),
			},
			.driver_data = "20090506",	 
		},
		{
			.ident = "dv6",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
				DMI_MATCH(DMI_PRODUCT_NAME,
					  "HP Pavilion dv6 Notebook PC"),
			},
			.driver_data = "20090423",	 
		},
		{
			.ident = "HDX18",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
				DMI_MATCH(DMI_PRODUCT_NAME,
					  "HP HDX18 Notebook PC"),
			},
			.driver_data = "20090430",	 
		},
		 
		{
			.ident = "G725",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "eMachines"),
				DMI_MATCH(DMI_PRODUCT_NAME, "eMachines G725"),
			},
			.driver_data = "20091216",	 
		},
		{ }	 
	};
	const struct dmi_system_id *dmi = dmi_first_match(sysids);
	int year, month, date;
	char buf[9];

	if (!dmi || pdev->bus->number || pdev->devfn != PCI_DEVFN(0x1f, 2))
		return false;

	dmi_get_date(DMI_BIOS_DATE, &year, &month, &date);
	snprintf(buf, sizeof(buf), "%04d%02d%02d", year, month, date);

	return strcmp(buf, dmi->driver_data) < 0;
}

static bool ahci_broken_lpm(struct pci_dev *pdev)
{
	static const struct dmi_system_id sysids[] = {
		 
		{
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
				DMI_MATCH(DMI_PRODUCT_VERSION, "ThinkPad X250"),
			},
			.driver_data = "20180406",  
		},
		{
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
				DMI_MATCH(DMI_PRODUCT_VERSION, "ThinkPad L450"),
			},
			.driver_data = "20180420",  
		},
		{
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
				DMI_MATCH(DMI_PRODUCT_VERSION, "ThinkPad T450s"),
			},
			.driver_data = "20180315",  
		},
		{
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
				DMI_MATCH(DMI_PRODUCT_VERSION, "ThinkPad W541"),
			},
			 
			.driver_data = "20180310",  
		},
		{ }	 
	};
	const struct dmi_system_id *dmi = dmi_first_match(sysids);
	int year, month, date;
	char buf[9];

	if (!dmi)
		return false;

	dmi_get_date(DMI_BIOS_DATE, &year, &month, &date);
	snprintf(buf, sizeof(buf), "%04d%02d%02d", year, month, date);

	return strcmp(buf, dmi->driver_data) < 0;
}

static bool ahci_broken_online(struct pci_dev *pdev)
{
#define ENCODE_BUSDEVFN(bus, slot, func)			\
	(void *)(unsigned long)(((bus) << 8) | PCI_DEVFN((slot), (func)))
	static const struct dmi_system_id sysids[] = {
		 
		{
			.ident = "EP45-DQ6",
			.matches = {
				DMI_MATCH(DMI_BOARD_VENDOR,
					  "Gigabyte Technology Co., Ltd."),
				DMI_MATCH(DMI_BOARD_NAME, "EP45-DQ6"),
			},
			.driver_data = ENCODE_BUSDEVFN(0x0a, 0x00, 0),
		},
		{
			.ident = "EP45-DS5",
			.matches = {
				DMI_MATCH(DMI_BOARD_VENDOR,
					  "Gigabyte Technology Co., Ltd."),
				DMI_MATCH(DMI_BOARD_NAME, "EP45-DS5"),
			},
			.driver_data = ENCODE_BUSDEVFN(0x03, 0x00, 0),
		},
		{ }	 
	};
#undef ENCODE_BUSDEVFN
	const struct dmi_system_id *dmi = dmi_first_match(sysids);
	unsigned int val;

	if (!dmi)
		return false;

	val = (unsigned long)dmi->driver_data;

	return pdev->bus->number == (val >> 8) && pdev->devfn == (val & 0xff);
}

static bool ahci_broken_devslp(struct pci_dev *pdev)
{
	 
	static const struct pci_device_id ids[] = {
		{ PCI_VDEVICE(INTEL, 0x0f23)},  
		{}
	};

	return pci_match_id(ids, pdev);
}

#ifdef CONFIG_ATA_ACPI
static void ahci_gtf_filter_workaround(struct ata_host *host)
{
	static const struct dmi_system_id sysids[] = {
		 
		{
			.ident = "Aspire 3810T",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
				DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 3810T"),
			},
			.driver_data = (void *)ATA_ACPI_FILTER_FPDMA_OFFSET,
		},
		{ }
	};
	const struct dmi_system_id *dmi = dmi_first_match(sysids);
	unsigned int filter;
	int i;

	if (!dmi)
		return;

	filter = (unsigned long)dmi->driver_data;
	dev_info(host->dev, "applying extra ACPI _GTF filter 0x%x for %s\n",
		 filter, dmi->ident);

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];
		struct ata_link *link;
		struct ata_device *dev;

		ata_for_each_link(link, ap, EDGE)
			ata_for_each_dev(dev, link, ALL)
				dev->gtf_filter |= filter;
	}
}
#else
static inline void ahci_gtf_filter_workaround(struct ata_host *host)
{}
#endif

 
static void acer_sa5_271_workaround(struct ahci_host_priv *hpriv,
				    struct pci_dev *pdev)
{
	static const struct dmi_system_id sysids[] = {
		{
			.ident = "Acer Switch Alpha 12",
			.matches = {
				DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
				DMI_MATCH(DMI_PRODUCT_NAME, "Switch SA5-271")
			},
		},
		{ }
	};

	if (dmi_check_system(sysids)) {
		dev_info(&pdev->dev, "enabling Acer Switch Alpha 12 workaround\n");
		if ((hpriv->saved_cap & 0xC734FF00) == 0xC734FF00) {
			hpriv->port_map = 0x7;
			hpriv->cap = 0xC734FF02;
		}
	}
}

#ifdef CONFIG_ARM64
 
static irqreturn_t ahci_thunderx_irq_handler(int irq, void *dev_instance)
{
	struct ata_host *host = dev_instance;
	struct ahci_host_priv *hpriv;
	unsigned int rc = 0;
	void __iomem *mmio;
	u32 irq_stat, irq_masked;
	unsigned int handled = 1;

	hpriv = host->private_data;
	mmio = hpriv->mmio;
	irq_stat = readl(mmio + HOST_IRQ_STAT);
	if (!irq_stat)
		return IRQ_NONE;

	do {
		irq_masked = irq_stat & hpriv->port_map;
		spin_lock(&host->lock);
		rc = ahci_handle_port_intr(host, irq_masked);
		if (!rc)
			handled = 0;
		writel(irq_stat, mmio + HOST_IRQ_STAT);
		irq_stat = readl(mmio + HOST_IRQ_STAT);
		spin_unlock(&host->lock);
	} while (irq_stat);

	return IRQ_RETVAL(handled);
}
#endif

static void ahci_remap_check(struct pci_dev *pdev, int bar,
		struct ahci_host_priv *hpriv)
{
	int i;
	u32 cap;

	 
	if (pdev->vendor != PCI_VENDOR_ID_INTEL ||
	    pci_resource_len(pdev, bar) < SZ_512K ||
	    bar != AHCI_PCI_BAR_STANDARD ||
	    !(readl(hpriv->mmio + AHCI_VSCAP) & 1))
		return;

	cap = readq(hpriv->mmio + AHCI_REMAP_CAP);
	for (i = 0; i < AHCI_MAX_REMAP; i++) {
		if ((cap & (1 << i)) == 0)
			continue;
		if (readl(hpriv->mmio + ahci_remap_dcc(i))
				!= PCI_CLASS_STORAGE_EXPRESS)
			continue;

		 
		hpriv->remapped_nvme++;
	}

	if (!hpriv->remapped_nvme)
		return;

	dev_warn(&pdev->dev, "Found %u remapped NVMe devices.\n",
		 hpriv->remapped_nvme);
	dev_warn(&pdev->dev,
		 "Switch your BIOS from RAID to AHCI mode to use them.\n");

	 
	hpriv->flags |= AHCI_HFLAG_NO_MSI;
}

static int ahci_get_irq_vector(struct ata_host *host, int port)
{
	return pci_irq_vector(to_pci_dev(host->dev), port);
}

static int ahci_init_msi(struct pci_dev *pdev, unsigned int n_ports,
			struct ahci_host_priv *hpriv)
{
	int nvec;

	if (hpriv->flags & AHCI_HFLAG_NO_MSI)
		return -ENODEV;

	 
	if (n_ports > 1) {
		nvec = pci_alloc_irq_vectors(pdev, n_ports, INT_MAX,
				PCI_IRQ_MSIX | PCI_IRQ_MSI);
		if (nvec > 0) {
			if (!(readl(hpriv->mmio + HOST_CTL) & HOST_MRSM)) {
				hpriv->get_irq_vector = ahci_get_irq_vector;
				hpriv->flags |= AHCI_HFLAG_MULTI_MSI;
				return nvec;
			}

			 
			printk(KERN_INFO
				"ahci: MRSM is on, fallback to single MSI\n");
			pci_free_irq_vectors(pdev);
		}
	}

	 
	nvec = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
	if (nvec == 1)
		return nvec;
	return pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSIX);
}

static void ahci_update_initial_lpm_policy(struct ata_port *ap,
					   struct ahci_host_priv *hpriv)
{
	int policy = CONFIG_SATA_MOBILE_LPM_POLICY;


	 
	if (!(hpriv->flags & AHCI_HFLAG_USE_LPM_POLICY))
		return;

	 
	if (mobile_lpm_policy != -1) {
		policy = mobile_lpm_policy;
		goto update_policy;
	}

	if (policy > ATA_LPM_MED_POWER && pm_suspend_default_s2idle()) {
		if (hpriv->cap & HOST_CAP_PART)
			policy = ATA_LPM_MIN_POWER_WITH_PARTIAL;
		else if (hpriv->cap & HOST_CAP_SSC)
			policy = ATA_LPM_MIN_POWER;
	}

update_policy:
	if (policy >= ATA_LPM_UNKNOWN && policy <= ATA_LPM_MIN_POWER)
		ap->target_lpm_policy = policy;
}

static void ahci_intel_pcs_quirk(struct pci_dev *pdev, struct ahci_host_priv *hpriv)
{
	const struct pci_device_id *id = pci_match_id(ahci_pci_tbl, pdev);
	u16 tmp16;

	 
	if (!id || id->vendor != PCI_VENDOR_ID_INTEL)
		return;

	 
	if (((enum board_ids) id->driver_data) >= board_ahci_pcs7)
		return;

	 
	pci_read_config_word(pdev, PCS_6, &tmp16);
	if ((tmp16 & hpriv->port_map) != hpriv->port_map) {
		tmp16 |= hpriv->port_map;
		pci_write_config_word(pdev, PCS_6, tmp16);
	}
}

static ssize_t remapped_nvme_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct ata_host *host = dev_get_drvdata(dev);
	struct ahci_host_priv *hpriv = host->private_data;

	return sysfs_emit(buf, "%u\n", hpriv->remapped_nvme);
}

static DEVICE_ATTR_RO(remapped_nvme);

static int ahci_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	unsigned int board_id = ent->driver_data;
	struct ata_port_info pi = ahci_port_info[board_id];
	const struct ata_port_info *ppi[] = { &pi, NULL };
	struct device *dev = &pdev->dev;
	struct ahci_host_priv *hpriv;
	struct ata_host *host;
	int n_ports, i, rc;
	int ahci_pci_bar = AHCI_PCI_BAR_STANDARD;

	WARN_ON((int)ATA_MAX_QUEUE > AHCI_MAX_CMDS);

	ata_print_version_once(&pdev->dev, DRV_VERSION);

	 
	if (pdev->vendor == PCI_VENDOR_ID_MARVELL && !marvell_enable)
		return -ENODEV;

	 
	if (is_mcp89_apple(pdev))
		ahci_mcp89_apple_enable(pdev);

	 
	if (pdev->vendor == PCI_VENDOR_ID_PROMISE)
		dev_info(&pdev->dev,
			 "PDC42819 can only drive SATA devices with this driver\n");

	 
	if (pdev->vendor == PCI_VENDOR_ID_STMICRO && pdev->device == 0xCC06)
		ahci_pci_bar = AHCI_PCI_BAR_STA2X11;
	else if (pdev->vendor == 0x1c44 && pdev->device == 0x8000)
		ahci_pci_bar = AHCI_PCI_BAR_ENMOTUS;
	else if (pdev->vendor == PCI_VENDOR_ID_CAVIUM) {
		if (pdev->device == 0xa01c)
			ahci_pci_bar = AHCI_PCI_BAR_CAVIUM;
		if (pdev->device == 0xa084)
			ahci_pci_bar = AHCI_PCI_BAR_CAVIUM_GEN5;
	} else if (pdev->vendor == PCI_VENDOR_ID_LOONGSON) {
		if (pdev->device == 0x7a08)
			ahci_pci_bar = AHCI_PCI_BAR_LOONGSON;
	}

	 
	rc = pcim_enable_device(pdev);
	if (rc)
		return rc;

	if (pdev->vendor == PCI_VENDOR_ID_INTEL &&
	    (pdev->device == 0x2652 || pdev->device == 0x2653)) {
		u8 map;

		 
		pci_read_config_byte(pdev, ICH_MAP, &map);
		if (map & 0x3) {
			dev_info(&pdev->dev,
				 "controller is in combined mode, can't enable AHCI mode\n");
			return -ENODEV;
		}
	}

	 
	rc = pcim_iomap_regions_request_all(pdev, 1 << ahci_pci_bar, DRV_NAME);
	if (rc == -EBUSY)
		pcim_pin_device(pdev);
	if (rc)
		return rc;

	hpriv = devm_kzalloc(dev, sizeof(*hpriv), GFP_KERNEL);
	if (!hpriv)
		return -ENOMEM;
	hpriv->flags |= (unsigned long)pi.private_data;

	 
	if (board_id == board_ahci_mcp65 &&
	    (pdev->revision == 0xa1 || pdev->revision == 0xa2))
		hpriv->flags |= AHCI_HFLAG_NO_MSI;

	 
	if (board_id == board_ahci_sb700 && pdev->revision >= 0x40)
		hpriv->flags &= ~AHCI_HFLAG_IGN_SERR_INTERNAL;

	 
	if (ahci_sb600_enable_64bit(pdev))
		hpriv->flags &= ~AHCI_HFLAG_32BIT_ONLY;

	hpriv->mmio = pcim_iomap_table(pdev)[ahci_pci_bar];

	 
	ahci_remap_check(pdev, ahci_pci_bar, hpriv);

	sysfs_add_file_to_group(&pdev->dev.kobj,
				&dev_attr_remapped_nvme.attr,
				NULL);

	 
	if (ahci_broken_devslp(pdev))
		hpriv->flags |= AHCI_HFLAG_NO_DEVSLP;

#ifdef CONFIG_ARM64
	if (pdev->vendor == PCI_VENDOR_ID_HUAWEI &&
	    pdev->device == 0xa235 &&
	    pdev->revision < 0x30)
		hpriv->flags |= AHCI_HFLAG_NO_SXS;

	if (pdev->vendor == 0x177d && pdev->device == 0xa01c)
		hpriv->irq_handler = ahci_thunderx_irq_handler;
#endif

	 
	ahci_pci_save_initial_config(pdev, hpriv);

	 
	if (hpriv->cap & HOST_CAP_NCQ) {
		pi.flags |= ATA_FLAG_NCQ;
		 
		if (!(hpriv->flags & AHCI_HFLAG_NO_FPDMA_AA))
			pi.flags |= ATA_FLAG_FPDMA_AA;

		 
		pi.flags |= ATA_FLAG_FPDMA_AUX;
	}

	if (hpriv->cap & HOST_CAP_PMP)
		pi.flags |= ATA_FLAG_PMP;

	ahci_set_em_messages(hpriv, &pi);

	if (ahci_broken_system_poweroff(pdev)) {
		pi.flags |= ATA_FLAG_NO_POWEROFF_SPINDOWN;
		dev_info(&pdev->dev,
			"quirky BIOS, skipping spindown on poweroff\n");
	}

	if (ahci_broken_lpm(pdev)) {
		pi.flags |= ATA_FLAG_NO_LPM;
		dev_warn(&pdev->dev,
			 "BIOS update required for Link Power Management support\n");
	}

	if (ahci_broken_suspend(pdev)) {
		hpriv->flags |= AHCI_HFLAG_NO_SUSPEND;
		dev_warn(&pdev->dev,
			 "BIOS update required for suspend/resume\n");
	}

	if (ahci_broken_online(pdev)) {
		hpriv->flags |= AHCI_HFLAG_SRST_TOUT_IS_OFFLINE;
		dev_info(&pdev->dev,
			 "online status unreliable, applying workaround\n");
	}


	 
	acer_sa5_271_workaround(hpriv, pdev);

	 
	n_ports = max(ahci_nr_ports(hpriv->cap), fls(hpriv->port_map));

	host = ata_host_alloc_pinfo(&pdev->dev, ppi, n_ports);
	if (!host)
		return -ENOMEM;
	host->private_data = hpriv;

	if (ahci_init_msi(pdev, n_ports, hpriv) < 0) {
		 
		pci_intx(pdev, 1);
	}
	hpriv->irq = pci_irq_vector(pdev, 0);

	if (!(hpriv->cap & HOST_CAP_SSS) || ahci_ignore_sss)
		host->flags |= ATA_HOST_PARALLEL_SCAN;
	else
		dev_info(&pdev->dev, "SSS flag set, parallel bus scan disabled\n");

	if (!(hpriv->cap & HOST_CAP_PART))
		host->flags |= ATA_HOST_NO_PART;

	if (!(hpriv->cap & HOST_CAP_SSC))
		host->flags |= ATA_HOST_NO_SSC;

	if (!(hpriv->cap2 & HOST_CAP2_SDS))
		host->flags |= ATA_HOST_NO_DEVSLP;

	if (pi.flags & ATA_FLAG_EM)
		ahci_reset_em(host);

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		ata_port_pbar_desc(ap, ahci_pci_bar, -1, "abar");
		ata_port_pbar_desc(ap, ahci_pci_bar,
				   0x100 + ap->port_no * 0x80, "port");

		 
		if (ap->flags & ATA_FLAG_EM)
			ap->em_message_type = hpriv->em_msg_type;

		ahci_update_initial_lpm_policy(ap, hpriv);

		 
		if (!(hpriv->port_map & (1 << i)))
			ap->ops = &ata_dummy_port_ops;
	}

	 
	ahci_p5wdh_workaround(host);

	 
	ahci_gtf_filter_workaround(host);

	 
	rc = ahci_configure_dma_masks(pdev, hpriv->cap & HOST_CAP_64);
	if (rc)
		return rc;

	rc = ahci_pci_reset_controller(host);
	if (rc)
		return rc;

	ahci_pci_init_controller(host);
	ahci_pci_print_info(host);

	pci_set_master(pdev);

	rc = ahci_host_activate(host, &ahci_sht);
	if (rc)
		return rc;

	pm_runtime_put_noidle(&pdev->dev);
	return 0;
}

static void ahci_shutdown_one(struct pci_dev *pdev)
{
	ata_pci_shutdown_one(pdev);
}

static void ahci_remove_one(struct pci_dev *pdev)
{
	sysfs_remove_file_from_group(&pdev->dev.kobj,
				     &dev_attr_remapped_nvme.attr,
				     NULL);
	pm_runtime_get_noresume(&pdev->dev);
	ata_pci_remove_one(pdev);
}

module_pci_driver(ahci_pci_driver);

MODULE_AUTHOR("Jeff Garzik");
MODULE_DESCRIPTION("AHCI SATA low-level driver");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, ahci_pci_tbl);
MODULE_VERSION(DRV_VERSION);
