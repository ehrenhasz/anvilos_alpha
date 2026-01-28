


#ifndef _AHCI_H
#define _AHCI_H

#include <linux/pci.h>
#include <linux/clk.h>
#include <linux/libata.h>
#include <linux/phy/phy.h>
#include <linux/regulator/consumer.h>
#include <linux/bits.h>


#define EM_CTRL_MSG_TYPE              0x000f0000


#define EM_MSG_LED_HBA_PORT           0x0000000f
#define EM_MSG_LED_PMP_SLOT           0x0000ff00
#define EM_MSG_LED_VALUE              0xffff0000
#define EM_MSG_LED_VALUE_ACTIVITY     0x00070000
#define EM_MSG_LED_VALUE_OFF          0xfff80000
#define EM_MSG_LED_VALUE_ON           0x00010000

enum {
	AHCI_MAX_PORTS		= 32,
	AHCI_MAX_SG		= 168, 
	AHCI_DMA_BOUNDARY	= 0xffffffff,
	AHCI_MAX_CMDS		= 32,
	AHCI_CMD_SZ		= 32,
	AHCI_CMD_SLOT_SZ	= AHCI_MAX_CMDS * AHCI_CMD_SZ,
	AHCI_RX_FIS_SZ		= 256,
	AHCI_CMD_TBL_CDB	= 0x40,
	AHCI_CMD_TBL_HDR_SZ	= 0x80,
	AHCI_CMD_TBL_SZ		= AHCI_CMD_TBL_HDR_SZ + (AHCI_MAX_SG * 16),
	AHCI_CMD_TBL_AR_SZ	= AHCI_CMD_TBL_SZ * AHCI_MAX_CMDS,
	AHCI_PORT_PRIV_DMA_SZ	= AHCI_CMD_SLOT_SZ + AHCI_CMD_TBL_AR_SZ +
				  AHCI_RX_FIS_SZ,
	AHCI_PORT_PRIV_FBS_DMA_SZ	= AHCI_CMD_SLOT_SZ +
					  AHCI_CMD_TBL_AR_SZ +
					  (AHCI_RX_FIS_SZ * 16),
	AHCI_IRQ_ON_SG		= BIT(31),
	AHCI_CMD_ATAPI		= BIT(5),
	AHCI_CMD_WRITE		= BIT(6),
	AHCI_CMD_PREFETCH	= BIT(7),
	AHCI_CMD_RESET		= BIT(8),
	AHCI_CMD_CLR_BUSY	= BIT(10),

	RX_FIS_PIO_SETUP	= 0x20,	
	RX_FIS_D2H_REG		= 0x40,	
	RX_FIS_SDB		= 0x58, 
	RX_FIS_UNK		= 0x60, 

	
	HOST_CAP		= 0x00, 
	HOST_CTL		= 0x04, 
	HOST_IRQ_STAT		= 0x08, 
	HOST_PORTS_IMPL		= 0x0c, 
	HOST_VERSION		= 0x10, 
	HOST_EM_LOC		= 0x1c, 
	HOST_EM_CTL		= 0x20, 
	HOST_CAP2		= 0x24, 

	
	HOST_RESET		= BIT(0),  
	HOST_IRQ_EN		= BIT(1),  
	HOST_MRSM		= BIT(2),  
	HOST_AHCI_EN		= BIT(31), 

	
	HOST_CAP_SXS		= BIT(5),  
	HOST_CAP_EMS		= BIT(6),  
	HOST_CAP_CCC		= BIT(7),  
	HOST_CAP_PART		= BIT(13), 
	HOST_CAP_SSC		= BIT(14), 
	HOST_CAP_PIO_MULTI	= BIT(15), 
	HOST_CAP_FBS		= BIT(16), 
	HOST_CAP_PMP		= BIT(17), 
	HOST_CAP_ONLY		= BIT(18), 
	HOST_CAP_CLO		= BIT(24), 
	HOST_CAP_LED		= BIT(25), 
	HOST_CAP_ALPM		= BIT(26), 
	HOST_CAP_SSS		= BIT(27), 
	HOST_CAP_MPS		= BIT(28), 
	HOST_CAP_SNTF		= BIT(29), 
	HOST_CAP_NCQ		= BIT(30), 
	HOST_CAP_64		= BIT(31), 

	
	HOST_CAP2_BOH		= BIT(0),  
	HOST_CAP2_NVMHCI	= BIT(1),  
	HOST_CAP2_APST		= BIT(2),  
	HOST_CAP2_SDS		= BIT(3),  
	HOST_CAP2_SADM		= BIT(4),  
	HOST_CAP2_DESO		= BIT(5),  

	
	PORT_LST_ADDR		= 0x00, 
	PORT_LST_ADDR_HI	= 0x04, 
	PORT_FIS_ADDR		= 0x08, 
	PORT_FIS_ADDR_HI	= 0x0c, 
	PORT_IRQ_STAT		= 0x10, 
	PORT_IRQ_MASK		= 0x14, 
	PORT_CMD		= 0x18, 
	PORT_TFDATA		= 0x20,	
	PORT_SIG		= 0x24,	
	PORT_CMD_ISSUE		= 0x38, 
	PORT_SCR_STAT		= 0x28, 
	PORT_SCR_CTL		= 0x2c, 
	PORT_SCR_ERR		= 0x30, 
	PORT_SCR_ACT		= 0x34, 
	PORT_SCR_NTF		= 0x3c, 
	PORT_FBS		= 0x40, 
	PORT_DEVSLP		= 0x44, 

	
	PORT_IRQ_COLD_PRES	= BIT(31), 
	PORT_IRQ_TF_ERR		= BIT(30), 
	PORT_IRQ_HBUS_ERR	= BIT(29), 
	PORT_IRQ_HBUS_DATA_ERR	= BIT(28), 
	PORT_IRQ_IF_ERR		= BIT(27), 
	PORT_IRQ_IF_NONFATAL	= BIT(26), 
	PORT_IRQ_OVERFLOW	= BIT(24), 
	PORT_IRQ_BAD_PMP	= BIT(23), 

	PORT_IRQ_PHYRDY		= BIT(22), 
	PORT_IRQ_DMPS		= BIT(7),  
	PORT_IRQ_CONNECT	= BIT(6),  
	PORT_IRQ_SG_DONE	= BIT(5),  
	PORT_IRQ_UNK_FIS	= BIT(4),  
	PORT_IRQ_SDB_FIS	= BIT(3),  
	PORT_IRQ_DMAS_FIS	= BIT(2),  
	PORT_IRQ_PIOS_FIS	= BIT(1),  
	PORT_IRQ_D2H_REG_FIS	= BIT(0),  

	PORT_IRQ_FREEZE		= PORT_IRQ_HBUS_ERR |
				  PORT_IRQ_IF_ERR |
				  PORT_IRQ_CONNECT |
				  PORT_IRQ_PHYRDY |
				  PORT_IRQ_UNK_FIS |
				  PORT_IRQ_BAD_PMP,
	PORT_IRQ_ERROR		= PORT_IRQ_FREEZE |
				  PORT_IRQ_TF_ERR |
				  PORT_IRQ_HBUS_DATA_ERR,
	DEF_PORT_IRQ		= PORT_IRQ_ERROR | PORT_IRQ_SG_DONE |
				  PORT_IRQ_SDB_FIS | PORT_IRQ_DMAS_FIS |
				  PORT_IRQ_PIOS_FIS | PORT_IRQ_D2H_REG_FIS,

	
	PORT_CMD_ASP		= BIT(27), 
	PORT_CMD_ALPE		= BIT(26), 
	PORT_CMD_ATAPI		= BIT(24), 
	PORT_CMD_FBSCP		= BIT(22), 
	PORT_CMD_ESP		= BIT(21), 
	PORT_CMD_CPD		= BIT(20), 
	PORT_CMD_MPSP		= BIT(19), 
	PORT_CMD_HPCP		= BIT(18), 
	PORT_CMD_PMP		= BIT(17), 
	PORT_CMD_LIST_ON	= BIT(15), 
	PORT_CMD_FIS_ON		= BIT(14), 
	PORT_CMD_FIS_RX		= BIT(4),  
	PORT_CMD_CLO		= BIT(3),  
	PORT_CMD_POWER_ON	= BIT(2),  
	PORT_CMD_SPIN_UP	= BIT(1),  
	PORT_CMD_START		= BIT(0),  

	PORT_CMD_ICC_MASK	= (0xfu << 28), 
	PORT_CMD_ICC_ACTIVE	= (0x1u << 28), 
	PORT_CMD_ICC_PARTIAL	= (0x2u << 28), 
	PORT_CMD_ICC_SLUMBER	= (0x6u << 28), 

	
	PORT_CMD_CAP		= PORT_CMD_HPCP | PORT_CMD_MPSP |
				  PORT_CMD_CPD | PORT_CMD_ESP | PORT_CMD_FBSCP,

	
	PORT_FBS_DWE_OFFSET	= 16, 
	PORT_FBS_ADO_OFFSET	= 12, 
	PORT_FBS_DEV_OFFSET	= 8,  
	PORT_FBS_DEV_MASK	= (0xf << PORT_FBS_DEV_OFFSET),  
	PORT_FBS_SDE		= BIT(2), 
	PORT_FBS_DEC		= BIT(1), 
	PORT_FBS_EN		= BIT(0), 

	
	PORT_DEVSLP_DM_OFFSET	= 25,             
	PORT_DEVSLP_DM_MASK	= (0xf << 25),    
	PORT_DEVSLP_DITO_OFFSET	= 15,             
	PORT_DEVSLP_MDAT_OFFSET	= 10,             
	PORT_DEVSLP_DETO_OFFSET	= 2,              
	PORT_DEVSLP_DSP		= BIT(1),         
	PORT_DEVSLP_ADSE	= BIT(0),         

	

#define AHCI_HFLAGS(flags)		.private_data	= (void *)(flags)

	AHCI_HFLAG_NO_NCQ		= BIT(0),
	AHCI_HFLAG_IGN_IRQ_IF_ERR	= BIT(1), 
	AHCI_HFLAG_IGN_SERR_INTERNAL	= BIT(2), 
	AHCI_HFLAG_32BIT_ONLY		= BIT(3), 
	AHCI_HFLAG_MV_PATA		= BIT(4), 
	AHCI_HFLAG_NO_MSI		= BIT(5), 
	AHCI_HFLAG_NO_PMP		= BIT(6), 
	AHCI_HFLAG_SECT255		= BIT(8), 
	AHCI_HFLAG_YES_NCQ		= BIT(9), 
	AHCI_HFLAG_NO_SUSPEND		= BIT(10), 
	AHCI_HFLAG_SRST_TOUT_IS_OFFLINE	= BIT(11), 
	AHCI_HFLAG_NO_SNTF		= BIT(12), 
	AHCI_HFLAG_NO_FPDMA_AA		= BIT(13), 
	AHCI_HFLAG_YES_FBS		= BIT(14), 
	AHCI_HFLAG_DELAY_ENGINE		= BIT(15), 
	AHCI_HFLAG_NO_DEVSLP		= BIT(17), 
	AHCI_HFLAG_NO_FBS		= BIT(18), 

#ifdef CONFIG_PCI_MSI
	AHCI_HFLAG_MULTI_MSI		= BIT(20), 
#else
	
	AHCI_HFLAG_MULTI_MSI		= 0,
#endif
	AHCI_HFLAG_WAKE_BEFORE_STOP	= BIT(22), 
	AHCI_HFLAG_YES_ALPM		= BIT(23), 
	AHCI_HFLAG_NO_WRITE_TO_RO	= BIT(24), 
	AHCI_HFLAG_USE_LPM_POLICY	= BIT(25), 
	AHCI_HFLAG_SUSPEND_PHYS		= BIT(26), 
	AHCI_HFLAG_NO_SXS		= BIT(28), 

	

	AHCI_FLAG_COMMON		= ATA_FLAG_SATA | ATA_FLAG_PIO_DMA |
					  ATA_FLAG_ACPI_SATA | ATA_FLAG_AN,

	ICH_MAP				= 0x90, 
	PCS_6				= 0x92, 
	PCS_7				= 0x94, 

	
	EM_MAX_SLOTS			= SATA_PMP_MAX_PORTS,
	EM_MAX_RETRY			= 5,

	
	EM_CTL_RST		= BIT(9), 
	EM_CTL_TM		= BIT(8), 
	EM_CTL_MR		= BIT(0), 
	EM_CTL_ALHD		= BIT(26), 
	EM_CTL_XMT		= BIT(25), 
	EM_CTL_SMB		= BIT(24), 
	EM_CTL_SGPIO		= BIT(19), 
	EM_CTL_SES		= BIT(18), 
	EM_CTL_SAFTE		= BIT(17), 
	EM_CTL_LED		= BIT(16), 

	
	EM_MSG_TYPE_LED		= BIT(0), 
	EM_MSG_TYPE_SAFTE	= BIT(1), 
	EM_MSG_TYPE_SES2	= BIT(2), 
	EM_MSG_TYPE_SGPIO	= BIT(3), 
};

struct ahci_cmd_hdr {
	__le32			opts;
	__le32			status;
	__le32			tbl_addr;
	__le32			tbl_addr_hi;
	__le32			reserved[4];
};

struct ahci_sg {
	__le32			addr;
	__le32			addr_hi;
	__le32			reserved;
	__le32			flags_size;
};

struct ahci_em_priv {
	enum sw_activity blink_policy;
	struct timer_list timer;
	unsigned long saved_activity;
	unsigned long activity;
	unsigned long led_state;
	struct ata_link *link;
};

struct ahci_port_priv {
	struct ata_link		*active_link;
	struct ahci_cmd_hdr	*cmd_slot;
	dma_addr_t		cmd_slot_dma;
	void			*cmd_tbl;
	dma_addr_t		cmd_tbl_dma;
	void			*rx_fis;
	dma_addr_t		rx_fis_dma;
	
	unsigned int		ncq_saw_d2h:1;
	unsigned int		ncq_saw_dmas:1;
	unsigned int		ncq_saw_sdb:1;
	spinlock_t		lock;		
	u32 			intr_mask;	
	bool			fbs_supported;	
	bool			fbs_enabled;	
	int			fbs_last_dev;	
	
	struct ahci_em_priv	em_priv[EM_MAX_SLOTS];
	char			*irq_desc;	
};

struct ahci_host_priv {
	
	unsigned int		flags;		
	u32			mask_port_map;	

	void __iomem *		mmio;		
	u32			cap;		
	u32			cap2;		
	u32			version;	
	u32			port_map;	
	u32			saved_cap;	
	u32			saved_cap2;	
	u32			saved_port_map;	
	u32			saved_port_cap[AHCI_MAX_PORTS]; 
	u32 			em_loc; 
	u32			em_buf_sz;	
	u32			em_msg_type;	
	u32			remapped_nvme;	
	bool			got_runtime_pm; 
	unsigned int		n_clks;
	struct clk_bulk_data	*clks;		
	unsigned int		f_rsts;
	struct reset_control	*rsts;		
	struct regulator	**target_pwrs;	
	struct regulator	*ahci_regulator;
	struct regulator	*phy_regulator;
	
	struct phy		**phys;
	unsigned		nports;		
	void			*plat_data;	
	unsigned int		irq;		
	
	void			(*start_engine)(struct ata_port *ap);
	
	int			(*stop_engine)(struct ata_port *ap);

	irqreturn_t 		(*irq_handler)(int irq, void *dev_instance);

	
	int			(*get_irq_vector)(struct ata_host *host,
						  int port);
};

extern int ahci_ignore_sss;

extern const struct attribute_group *ahci_shost_groups[];
extern const struct attribute_group *ahci_sdev_groups[];


#define AHCI_SHT(drv_name)						\
	__ATA_BASE_SHT(drv_name),					\
	.can_queue		= AHCI_MAX_CMDS,			\
	.sg_tablesize		= AHCI_MAX_SG,				\
	.dma_boundary		= AHCI_DMA_BOUNDARY,			\
	.shost_groups		= ahci_shost_groups,			\
	.sdev_groups		= ahci_sdev_groups,			\
	.change_queue_depth     = ata_scsi_change_queue_depth,		\
	.tag_alloc_policy       = BLK_TAG_ALLOC_RR,             	\
	.slave_configure        = ata_scsi_slave_config

extern struct ata_port_operations ahci_ops;
extern struct ata_port_operations ahci_platform_ops;
extern struct ata_port_operations ahci_pmp_retry_srst_ops;

unsigned int ahci_dev_classify(struct ata_port *ap);
void ahci_fill_cmd_slot(struct ahci_port_priv *pp, unsigned int tag,
			u32 opts);
void ahci_save_initial_config(struct device *dev,
			      struct ahci_host_priv *hpriv);
void ahci_init_controller(struct ata_host *host);
int ahci_reset_controller(struct ata_host *host);

int ahci_do_softreset(struct ata_link *link, unsigned int *class,
		      int pmp, unsigned long deadline,
		      int (*check_ready)(struct ata_link *link));

int ahci_do_hardreset(struct ata_link *link, unsigned int *class,
		      unsigned long deadline, bool *online);

unsigned int ahci_qc_issue(struct ata_queued_cmd *qc);
int ahci_stop_engine(struct ata_port *ap);
void ahci_start_fis_rx(struct ata_port *ap);
void ahci_start_engine(struct ata_port *ap);
int ahci_check_ready(struct ata_link *link);
int ahci_kick_engine(struct ata_port *ap);
int ahci_port_resume(struct ata_port *ap);
void ahci_set_em_messages(struct ahci_host_priv *hpriv,
			  struct ata_port_info *pi);
int ahci_reset_em(struct ata_host *host);
void ahci_print_info(struct ata_host *host, const char *scc_s);
int ahci_host_activate(struct ata_host *host, const struct scsi_host_template *sht);
void ahci_error_handler(struct ata_port *ap);
u32 ahci_handle_port_intr(struct ata_host *host, u32 irq_masked);

static inline void __iomem *__ahci_port_base(struct ahci_host_priv *hpriv,
					     unsigned int port_no)
{
	void __iomem *mmio = hpriv->mmio;

	return mmio + 0x100 + (port_no * 0x80);
}

static inline void __iomem *ahci_port_base(struct ata_port *ap)
{
	struct ahci_host_priv *hpriv = ap->host->private_data;

	return __ahci_port_base(hpriv, ap->port_no);
}

static inline int ahci_nr_ports(u32 cap)
{
	return (cap & 0x1f) + 1;
}

#endif 
