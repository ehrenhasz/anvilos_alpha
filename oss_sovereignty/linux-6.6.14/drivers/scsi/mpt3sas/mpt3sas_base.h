#ifndef MPT3SAS_BASE_H_INCLUDED
#define MPT3SAS_BASE_H_INCLUDED
#include "mpi/mpi2_type.h"
#include "mpi/mpi2.h"
#include "mpi/mpi2_ioc.h"
#include "mpi/mpi2_cnfg.h"
#include "mpi/mpi2_init.h"
#include "mpi/mpi2_raid.h"
#include "mpi/mpi2_tool.h"
#include "mpi/mpi2_sas.h"
#include "mpi/mpi2_pci.h"
#include "mpi/mpi2_image.h"
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport_sas.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_eh.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/irq_poll.h>
#include "mpt3sas_debug.h"
#include "mpt3sas_trigger_diag.h"
#include "mpt3sas_trigger_pages.h"
#define MPT3SAS_DRIVER_NAME		"mpt3sas"
#define MPT3SAS_AUTHOR "Avago Technologies <MPT-FusionLinux.pdl@avagotech.com>"
#define MPT3SAS_DESCRIPTION	"LSI MPT Fusion SAS 3.0 Device Driver"
#define MPT3SAS_DRIVER_VERSION		"43.100.00.00"
#define MPT3SAS_MAJOR_VERSION		43
#define MPT3SAS_MINOR_VERSION		100
#define MPT3SAS_BUILD_VERSION		0
#define MPT3SAS_RELEASE_VERSION	00
#define MPT2SAS_DRIVER_NAME		"mpt2sas"
#define MPT2SAS_DESCRIPTION	"LSI MPT Fusion SAS 2.0 Device Driver"
#define MPT2SAS_DRIVER_VERSION		"20.102.00.00"
#define MPT2SAS_MAJOR_VERSION		20
#define MPT2SAS_MINOR_VERSION		102
#define MPT2SAS_BUILD_VERSION		0
#define MPT2SAS_RELEASE_VERSION	00
#define MPT3SAS_DEFAULT_COREDUMP_TIMEOUT_SECONDS	(15)  
#define MPT3SAS_COREDUMP_LOOP_DONE                     (0xFF)
#define MPT3SAS_TIMESYNC_TIMEOUT_SECONDS		(10)  
#define MPT3SAS_TIMESYNC_UPDATE_INTERVAL		(900)  
#define MPT3SAS_TIMESYNC_UNIT_MASK			(0x80)  
#define MPT3SAS_TIMESYNC_MASK				(0x7F)  
#define SECONDS_PER_MIN					(60)
#define SECONDS_PER_HOUR				(3600)
#define MPT3SAS_COREDUMP_LOOP_DONE			(0xFF)
#define MPI26_SET_IOC_PARAMETER_SYNC_TIMESTAMP		(0x81)
#define MPT_MAX_PHYS_SEGMENTS	SG_CHUNK_SIZE
#define MPT_MIN_PHYS_SEGMENTS	16
#define MPT_KDUMP_MIN_PHYS_SEGMENTS	32
#define MCPU_MAX_CHAINS_PER_IO	3
#ifdef CONFIG_SCSI_MPT3SAS_MAX_SGE
#define MPT3SAS_SG_DEPTH		CONFIG_SCSI_MPT3SAS_MAX_SGE
#else
#define MPT3SAS_SG_DEPTH		MPT_MAX_PHYS_SEGMENTS
#endif
#ifdef CONFIG_SCSI_MPT2SAS_MAX_SGE
#define MPT2SAS_SG_DEPTH		CONFIG_SCSI_MPT2SAS_MAX_SGE
#else
#define MPT2SAS_SG_DEPTH		MPT_MAX_PHYS_SEGMENTS
#endif
#define MPT3SAS_SATA_QUEUE_DEPTH	32
#define MPT3SAS_SAS_QUEUE_DEPTH		254
#define MPT3SAS_RAID_QUEUE_DEPTH	128
#define MPT3SAS_KDUMP_SCSI_IO_DEPTH	200
#define MPT3SAS_RAID_MAX_SECTORS	8192
#define MPT3SAS_HOST_PAGE_SIZE_4K	12
#define MPT3SAS_NVME_QUEUE_DEPTH	128
#define MPT_NAME_LENGTH			32	 
#define MPT_STRING_LENGTH		64
#define MPI_FRAME_START_OFFSET		256
#define REPLY_FREE_POOL_SIZE		512  
#define MPT_MAX_CALLBACKS		32
#define MPT_MAX_HBA_NUM_PHYS		32
#define INTERNAL_CMDS_COUNT		10	 
#define INTERNAL_SCSIIO_CMDS_COUNT	3
#define MPI3_HIM_MASK			0xFFFFFFFF  
#define MPT3SAS_INVALID_DEVICE_HANDLE	0xFFFF
#define MAX_CHAIN_ELEMT_SZ		16
#define DEFAULT_NUM_FWCHAIN_ELEMTS	8
#define IO_UNIT_CONTROL_SHUTDOWN_TIMEOUT 6
#define FW_IMG_HDR_READ_TIMEOUT	15
#define IOC_OPERATIONAL_WAIT_COUNT	10
#define	NVME_PRP_SIZE			8	 
#define	NVME_ERROR_RESPONSE_SIZE	16	 
#define NVME_TASK_ABORT_MIN_TIMEOUT	6
#define NVME_TASK_ABORT_MAX_TIMEOUT	60
#define NVME_TASK_MNGT_CUSTOM_MASK	(0x0010)
#define	NVME_PRP_PAGE_SIZE		4096	 
struct mpt3sas_nvme_cmd {
	u8	rsvd[24];
	__le64	prp1;
	__le64	prp2;
};
#define ioc_err(ioc, fmt, ...)						\
	pr_err("%s: " fmt, (ioc)->name, ##__VA_ARGS__)
#define ioc_notice(ioc, fmt, ...)					\
	pr_notice("%s: " fmt, (ioc)->name, ##__VA_ARGS__)
#define ioc_warn(ioc, fmt, ...)						\
	pr_warn("%s: " fmt, (ioc)->name, ##__VA_ARGS__)
#define ioc_info(ioc, fmt, ...)						\
	pr_info("%s: " fmt, (ioc)->name, ##__VA_ARGS__)
#define MPT2_WARPDRIVE_LOGENTRY		(0x8002)
#define MPT2_WARPDRIVE_LC_SSDT			(0x41)
#define MPT2_WARPDRIVE_LC_SSDLW		(0x43)
#define MPT2_WARPDRIVE_LC_SSDLF		(0x44)
#define MPT2_WARPDRIVE_LC_BRMF			(0x4D)
#define MPT_TARGET_FLAGS_RAID_COMPONENT	0x01
#define MPT_TARGET_FLAGS_VOLUME		0x02
#define MPT_TARGET_FLAGS_DELETED	0x04
#define MPT_TARGET_FASTPATH_IO		0x08
#define MPT_TARGET_FLAGS_PCIE_DEVICE	0x10
#define SAS2_PCI_DEVICE_B0_REVISION	(0x01)
#define SAS3_PCI_DEVICE_C0_REVISION	(0x02)
#define MPI26_ATLAS_PCIe_SWITCH_DEVID	(0x00B2)
#define MPT2SAS_INTEL_RMS25JB080_BRANDING    \
	"Intel(R) Integrated RAID Module RMS25JB080"
#define MPT2SAS_INTEL_RMS25JB040_BRANDING    \
	"Intel(R) Integrated RAID Module RMS25JB040"
#define MPT2SAS_INTEL_RMS25KB080_BRANDING    \
	"Intel(R) Integrated RAID Module RMS25KB080"
#define MPT2SAS_INTEL_RMS25KB040_BRANDING    \
	"Intel(R) Integrated RAID Module RMS25KB040"
#define MPT2SAS_INTEL_RMS25LB040_BRANDING	\
	"Intel(R) Integrated RAID Module RMS25LB040"
#define MPT2SAS_INTEL_RMS25LB080_BRANDING	\
	"Intel(R) Integrated RAID Module RMS25LB080"
#define MPT2SAS_INTEL_RMS2LL080_BRANDING	\
	"Intel Integrated RAID Module RMS2LL080"
#define MPT2SAS_INTEL_RMS2LL040_BRANDING	\
	"Intel Integrated RAID Module RMS2LL040"
#define MPT2SAS_INTEL_RS25GB008_BRANDING       \
	"Intel(R) RAID Controller RS25GB008"
#define MPT2SAS_INTEL_SSD910_BRANDING          \
	"Intel(R) SSD 910 Series"
#define MPT3SAS_INTEL_RMS3JC080_BRANDING       \
	"Intel(R) Integrated RAID Module RMS3JC080"
#define MPT3SAS_INTEL_RS3GC008_BRANDING       \
	"Intel(R) RAID Controller RS3GC008"
#define MPT3SAS_INTEL_RS3FC044_BRANDING       \
	"Intel(R) RAID Controller RS3FC044"
#define MPT3SAS_INTEL_RS3UC080_BRANDING       \
	"Intel(R) RAID Controller RS3UC080"
#define MPT2SAS_INTEL_RMS25JB080_SSDID		0x3516
#define MPT2SAS_INTEL_RMS25JB040_SSDID		0x3517
#define MPT2SAS_INTEL_RMS25KB080_SSDID		0x3518
#define MPT2SAS_INTEL_RMS25KB040_SSDID		0x3519
#define MPT2SAS_INTEL_RMS25LB040_SSDID		0x351A
#define MPT2SAS_INTEL_RMS25LB080_SSDID		0x351B
#define MPT2SAS_INTEL_RMS2LL080_SSDID		0x350E
#define MPT2SAS_INTEL_RMS2LL040_SSDID		0x350F
#define MPT2SAS_INTEL_RS25GB008_SSDID		0x3000
#define MPT2SAS_INTEL_SSD910_SSDID		0x3700
#define MPT3SAS_INTEL_RMS3JC080_SSDID		0x3521
#define MPT3SAS_INTEL_RS3GC008_SSDID		0x3522
#define MPT3SAS_INTEL_RS3FC044_SSDID		0x3523
#define MPT3SAS_INTEL_RS3UC080_SSDID		0x3524
#define MPT2SAS_DELL_BRANDING_SIZE                 32
#define MPT2SAS_DELL_6GBPS_SAS_HBA_BRANDING        "Dell 6Gbps SAS HBA"
#define MPT2SAS_DELL_PERC_H200_ADAPTER_BRANDING    "Dell PERC H200 Adapter"
#define MPT2SAS_DELL_PERC_H200_INTEGRATED_BRANDING "Dell PERC H200 Integrated"
#define MPT2SAS_DELL_PERC_H200_MODULAR_BRANDING    "Dell PERC H200 Modular"
#define MPT2SAS_DELL_PERC_H200_EMBEDDED_BRANDING   "Dell PERC H200 Embedded"
#define MPT2SAS_DELL_PERC_H200_BRANDING            "Dell PERC H200"
#define MPT2SAS_DELL_6GBPS_SAS_BRANDING            "Dell 6Gbps SAS"
#define MPT3SAS_DELL_12G_HBA_BRANDING       \
	"Dell 12Gbps HBA"
#define MPT2SAS_DELL_6GBPS_SAS_HBA_SSDID	0x1F1C
#define MPT2SAS_DELL_PERC_H200_ADAPTER_SSDID	0x1F1D
#define MPT2SAS_DELL_PERC_H200_INTEGRATED_SSDID	0x1F1E
#define MPT2SAS_DELL_PERC_H200_MODULAR_SSDID	0x1F1F
#define MPT2SAS_DELL_PERC_H200_EMBEDDED_SSDID	0x1F20
#define MPT2SAS_DELL_PERC_H200_SSDID		0x1F21
#define MPT2SAS_DELL_6GBPS_SAS_SSDID		0x1F22
#define MPT3SAS_DELL_12G_HBA_SSDID		0x1F46
#define MPT3SAS_CISCO_12G_8E_HBA_BRANDING		\
	"Cisco 9300-8E 12G SAS HBA"
#define MPT3SAS_CISCO_12G_8I_HBA_BRANDING		\
	"Cisco 9300-8i 12G SAS HBA"
#define MPT3SAS_CISCO_12G_AVILA_HBA_BRANDING	\
	"Cisco 12G Modular SAS Pass through Controller"
#define MPT3SAS_CISCO_12G_COLUSA_MEZZANINE_HBA_BRANDING		\
	"UCS C3X60 12G SAS Pass through Controller"
#define MPT3SAS_CISCO_12G_8E_HBA_SSDID  0x14C
#define MPT3SAS_CISCO_12G_8I_HBA_SSDID  0x154
#define MPT3SAS_CISCO_12G_AVILA_HBA_SSDID  0x155
#define MPT3SAS_CISCO_12G_COLUSA_MEZZANINE_HBA_SSDID  0x156
#define MPT3_DIAG_BUFFER_IS_REGISTERED	(0x01)
#define MPT3_DIAG_BUFFER_IS_RELEASED	(0x02)
#define MPT3_DIAG_BUFFER_IS_DIAG_RESET	(0x04)
#define MPT3_DIAG_BUFFER_IS_DRIVER_ALLOCATED (0x08)
#define MPT3_DIAG_BUFFER_IS_APP_OWNED (0x10)
#define MPT2SAS_HP_3PAR_SSVID                0x1590
#define MPT2SAS_HP_2_4_INTERNAL_BRANDING	\
	"HP H220 Host Bus Adapter"
#define MPT2SAS_HP_2_4_EXTERNAL_BRANDING	\
	"HP H221 Host Bus Adapter"
#define MPT2SAS_HP_1_4_INTERNAL_1_4_EXTERNAL_BRANDING	\
	"HP H222 Host Bus Adapter"
#define MPT2SAS_HP_EMBEDDED_2_4_INTERNAL_BRANDING	\
	"HP H220i Host Bus Adapter"
#define MPT2SAS_HP_DAUGHTER_2_4_INTERNAL_BRANDING	\
	"HP H210i Host Bus Adapter"
#define MPT2SAS_HP_2_4_INTERNAL_SSDID			0x0041
#define MPT2SAS_HP_2_4_EXTERNAL_SSDID			0x0042
#define MPT2SAS_HP_1_4_INTERNAL_1_4_EXTERNAL_SSDID	0x0043
#define MPT2SAS_HP_EMBEDDED_2_4_INTERNAL_SSDID		0x0044
#define MPT2SAS_HP_DAUGHTER_2_4_INTERNAL_SSDID		0x0046
#define MAX_COMBINED_MSIX_VECTORS(gen35) ((gen35 == 1) ? 16 : 8)
#define MPT3_SUP_REPLY_POST_HOST_INDEX_REG_COUNT_G3	12
#define MPT3_SUP_REPLY_POST_HOST_INDEX_REG_COUNT_G35	16
#define MPT3_SUP_REPLY_POST_HOST_INDEX_REG_OFFSET	(0x10)
#define MPT3_MIN_IRQS					1
#define MFG10_OEM_ID_INVALID                   (0x00000000)
#define MFG10_OEM_ID_DELL                      (0x00000001)
#define MFG10_OEM_ID_FSC                       (0x00000002)
#define MFG10_OEM_ID_SUN                       (0x00000003)
#define MFG10_OEM_ID_IBM                       (0x00000004)
#define MFG10_GF0_OCE_DISABLED                 (0x00000001)
#define MFG10_GF0_R1E_DRIVE_COUNT              (0x00000002)
#define MFG10_GF0_R10_DISPLAY                  (0x00000004)
#define MFG10_GF0_SSD_DATA_SCRUB_DISABLE       (0x00000008)
#define MFG10_GF0_SINGLE_DRIVE_R0              (0x00000010)
#define VIRTUAL_IO_FAILED_RETRY			(0x32010081)
#define MPT3SAS_DEVICE_HIGH_IOPS_DEPTH		8
#define MPT3SAS_HIGH_IOPS_REPLY_QUEUES		8
#define MPT3SAS_HIGH_IOPS_BATCH_COUNT		16
#define MPT3SAS_GEN35_MAX_MSIX_QUEUES		128
#define RDPQ_MAX_INDEX_IN_ONE_CHUNK		16
struct Mpi2ManufacturingPage10_t {
	MPI2_CONFIG_PAGE_HEADER	Header;		 
	U8	OEMIdentifier;			 
	U8	Reserved1;			 
	U16	Reserved2;			 
	U32	Reserved3;			 
	U32	GenericFlags0;			 
	U32	GenericFlags1;			 
	U32	Reserved4;			 
	U32	OEMSpecificFlags0;		 
	U32	OEMSpecificFlags1;		 
	U32	Reserved5[18];			 
};
struct Mpi2ManufacturingPage11_t {
	MPI2_CONFIG_PAGE_HEADER Header;		 
	__le32	Reserved1;			 
	u8	Reserved2;			 
	u8	EEDPTagMode;			 
	u8	Reserved3;			 
	u8	Reserved4;			 
	__le32	Reserved5[8];			 
	u16	AddlFlags2;			 
	u8	AddlFlags3;			 
	u8	Reserved6;			 
	__le32	Reserved7[7];			 
	u8	NVMeAbortTO;			 
	u8	NumPerDevEvents;		 
	u8	HostTraceBufferDecrementSizeKB;	 
	u8	HostTraceBufferFlags;		 
	u16	HostTraceBufferMaxSizeKB;	 
	u16	HostTraceBufferMinSizeKB;	 
	u8	CoreDumpTOSec;			 
	u8	TimeSyncInterval;		 
	u16	Reserved9;			 
	__le32	Reserved10;			 
};
struct MPT3SAS_TARGET {
	struct scsi_target *starget;
	u64	sas_address;
	struct _raid_device *raid_device;
	u16	handle;
	int	num_luns;
	u32	flags;
	u8	deleted;
	u8	tm_busy;
	struct hba_port *port;
	struct _sas_device *sas_dev;
	struct _pcie_device *pcie_dev;
};
#define MPT_DEVICE_FLAGS_INIT		0x01
#define MFG_PAGE10_HIDE_SSDS_MASK	(0x00000003)
#define MFG_PAGE10_HIDE_ALL_DISKS	(0x00)
#define MFG_PAGE10_EXPOSE_ALL_DISKS	(0x01)
#define MFG_PAGE10_HIDE_IF_VOL_PRESENT	(0x02)
struct MPT3SAS_DEVICE {
	struct MPT3SAS_TARGET *sas_target;
	unsigned int	lun;
	u32	flags;
	u8	configured_lun;
	u8	block;
	u8	tlr_snoop_check;
	u8	ignore_delay_remove;
	u8	ncq_prio_enable;
	unsigned long ata_command_pending;
};
#define MPT3_CMD_NOT_USED	0x8000	 
#define MPT3_CMD_COMPLETE	0x0001	 
#define MPT3_CMD_PENDING	0x0002	 
#define MPT3_CMD_REPLY_VALID	0x0004	 
#define MPT3_CMD_RESET		0x0008	 
#define MPT3_CMD_COMPLETE_ASYNC 0x0010   
struct _internal_cmd {
	struct mutex mutex;
	struct completion done;
	void	*reply;
	void	*sense;
	u16	status;
	u16	smid;
};
struct _sas_device {
	struct list_head list;
	struct scsi_target *starget;
	u64	sas_address;
	u64	device_name;
	u16	handle;
	u64	sas_address_parent;
	u16	enclosure_handle;
	u64	enclosure_logical_id;
	u16	volume_handle;
	u64	volume_wwid;
	u32	device_info;
	int	id;
	int	channel;
	u16	slot;
	u8	phy;
	u8	responding;
	u8	fast_path;
	u8	pfa_led_on;
	u8	pend_sas_rphy_add;
	u8	enclosure_level;
	u8	chassis_slot;
	u8	is_chassis_slot_valid;
	u8	connector_name[5];
	struct kref refcount;
	u8	port_type;
	struct hba_port *port;
	struct sas_rphy *rphy;
};
static inline void sas_device_get(struct _sas_device *s)
{
	kref_get(&s->refcount);
}
static inline void sas_device_free(struct kref *r)
{
	kfree(container_of(r, struct _sas_device, refcount));
}
static inline void sas_device_put(struct _sas_device *s)
{
	kref_put(&s->refcount, sas_device_free);
}
struct _pcie_device {
	struct list_head list;
	struct scsi_target *starget;
	u64	wwid;
	u16	handle;
	u32	device_info;
	int	id;
	int	channel;
	u16	slot;
	u8	port_num;
	u8	responding;
	u8	fast_path;
	u32	nvme_mdts;
	u16	enclosure_handle;
	u64	enclosure_logical_id;
	u8	enclosure_level;
	u8	connector_name[4];
	u8	*serial_number;
	u8	reset_timeout;
	u8	access_status;
	u16	shutdown_latency;
	struct kref refcount;
};
static inline void pcie_device_get(struct _pcie_device *p)
{
	kref_get(&p->refcount);
}
static inline void pcie_device_free(struct kref *r)
{
	kfree(container_of(r, struct _pcie_device, refcount));
}
static inline void pcie_device_put(struct _pcie_device *p)
{
	kref_put(&p->refcount, pcie_device_free);
}
#define MPT_MAX_WARPDRIVE_PDS		8
struct _raid_device {
	struct list_head list;
	struct scsi_target *starget;
	struct scsi_device *sdev;
	u64	wwid;
	u16	handle;
	u16	block_sz;
	int	id;
	int	channel;
	u8	volume_type;
	u8	num_pds;
	u8	responding;
	u8	percent_complete;
	u8	direct_io_enabled;
	u8	stripe_exponent;
	u8	block_exponent;
	u64	max_lba;
	u32	stripe_sz;
	u32	device_info;
	u16	pd_handle[MPT_MAX_WARPDRIVE_PDS];
};
struct _boot_device {
	int channel;
	void *device;
};
struct _sas_port {
	struct list_head port_list;
	u8	num_phys;
	struct sas_identify remote_identify;
	struct sas_rphy *rphy;
	struct sas_port *port;
	struct hba_port *hba_port;
	struct list_head phy_list;
};
struct _sas_phy {
	struct list_head port_siblings;
	struct sas_identify identify;
	struct sas_identify remote_identify;
	struct sas_phy *phy;
	u8	phy_id;
	u16	handle;
	u16	attached_handle;
	u8	phy_belongs_to_port;
	u8	hba_vphy;
	struct hba_port *port;
};
struct _sas_node {
	struct list_head list;
	struct device *parent_dev;
	u8	num_phys;
	u64	sas_address;
	u16	handle;
	u64	sas_address_parent;
	u16	enclosure_handle;
	u64	enclosure_logical_id;
	u8	responding;
	u8	nr_phys_allocated;
	struct hba_port *port;
	struct	_sas_phy *phy;
	struct list_head sas_port_list;
	struct sas_rphy *rphy;
};
struct _enclosure_node {
	struct list_head list;
	Mpi2SasEnclosurePage0_t pg0;
};
enum reset_type {
	FORCE_BIG_HAMMER,
	SOFT_RESET,
};
struct pcie_sg_list {
	void            *pcie_sgl;
	dma_addr_t      pcie_sgl_dma;
};
struct chain_tracker {
	void *chain_buffer;
	dma_addr_t chain_buffer_dma;
};
struct chain_lookup {
	struct chain_tracker *chains_per_smid;
	atomic_t	chain_offset;
};
struct scsiio_tracker {
	u16	smid;
	struct scsi_cmnd *scmd;
	u8	cb_idx;
	u8	direct_io;
	struct pcie_sg_list pcie_sg_list;
	struct list_head chain_list;
	u16     msix_io;
};
struct request_tracker {
	u16	smid;
	u8	cb_idx;
	struct list_head tracker_list;
};
struct _tr_list {
	struct list_head list;
	u16	handle;
	u16	state;
};
struct _sc_list {
	struct list_head list;
	u16     handle;
};
struct _event_ack_list {
	struct list_head list;
	U16     Event;
	U32     EventContext;
};
struct adapter_reply_queue {
	struct MPT3SAS_ADAPTER	*ioc;
	u8			msix_index;
	u32			reply_post_host_index;
	Mpi2ReplyDescriptorsUnion_t *reply_post_free;
	char			name[MPT_NAME_LENGTH];
	atomic_t		busy;
	u32			os_irq;
	struct irq_poll         irqpoll;
	bool			irq_poll_scheduled;
	bool			irq_line_enable;
	bool			is_iouring_poll_q;
	struct list_head	list;
};
struct io_uring_poll_queue {
	atomic_t	busy;
	atomic_t	pause;
	struct adapter_reply_queue *reply_q;
};
typedef void (*MPT_ADD_SGE)(void *paddr, u32 flags_length, dma_addr_t dma_addr);
typedef int (*MPT_BUILD_SG_SCMD)(struct MPT3SAS_ADAPTER *ioc,
	struct scsi_cmnd *scmd, u16 smid, struct _pcie_device *pcie_device);
typedef void (*MPT_BUILD_SG)(struct MPT3SAS_ADAPTER *ioc, void *psge,
		dma_addr_t data_out_dma, size_t data_out_sz,
		dma_addr_t data_in_dma, size_t data_in_sz);
typedef void (*MPT_BUILD_ZERO_LEN_SGE)(struct MPT3SAS_ADAPTER *ioc,
		void *paddr);
typedef void (*NVME_BUILD_PRP)(struct MPT3SAS_ADAPTER *ioc, u16 smid,
	Mpi26NVMeEncapsulatedRequest_t *nvme_encap_request,
	dma_addr_t data_out_dma, size_t data_out_sz, dma_addr_t data_in_dma,
	size_t data_in_sz);
typedef void (*PUT_SMID_IO_FP_HIP) (struct MPT3SAS_ADAPTER *ioc, u16 smid,
	u16 funcdep);
typedef void (*PUT_SMID_DEFAULT) (struct MPT3SAS_ADAPTER *ioc, u16 smid);
typedef u32 (*BASE_READ_REG) (const void __iomem *addr);
typedef u8 (*GET_MSIX_INDEX) (struct MPT3SAS_ADAPTER *ioc,
	struct scsi_cmnd *scmd);
union mpi3_version_union {
	MPI2_VERSION_STRUCT		Struct;
	u32				Word;
};
struct mpt3sas_facts {
	u16			MsgVersion;
	u16			HeaderVersion;
	u8			IOCNumber;
	u8			VP_ID;
	u8			VF_ID;
	u16			IOCExceptions;
	u16			IOCStatus;
	u32			IOCLogInfo;
	u8			MaxChainDepth;
	u8			WhoInit;
	u8			NumberOfPorts;
	u8			MaxMSIxVectors;
	u16			RequestCredit;
	u16			ProductID;
	u32			IOCCapabilities;
	union mpi3_version_union	FWVersion;
	u16			IOCRequestFrameSize;
	u16			IOCMaxChainSegmentSize;
	u16			MaxInitiators;
	u16			MaxTargets;
	u16			MaxSasExpanders;
	u16			MaxEnclosures;
	u16			ProtocolFlags;
	u16			HighPriorityCredit;
	u16			MaxReplyDescriptorPostQueueDepth;
	u8			ReplyFrameSize;
	u8			MaxVolumes;
	u16			MaxDevHandle;
	u16			MaxPersistentEntries;
	u16			MinDevHandle;
	u8			CurrentHostPageSize;
};
struct mpt3sas_port_facts {
	u8			PortNumber;
	u8			VP_ID;
	u8			VF_ID;
	u8			PortType;
	u16			MaxPostedCmdBuffers;
};
struct reply_post_struct {
	Mpi2ReplyDescriptorsUnion_t	*reply_post_free;
	dma_addr_t			reply_post_free_dma;
};
struct virtual_phy {
	struct	list_head list;
	u64	sas_address;
	u32	phy_mask;
	u8	flags;
};
#define MPT_VPHY_FLAG_DIRTY_PHY	0x01
struct hba_port {
	struct list_head list;
	u64	sas_address;
	u32	phy_mask;
	u8      port_id;
	u8	flags;
	u32	vphys_mask;
	struct list_head vphys_list;
};
#define HBA_PORT_FLAG_DIRTY_PORT       0x01
#define HBA_PORT_FLAG_NEW_PORT         0x02
#define MULTIPATH_DISABLED_PORT_ID     0xFF
struct htb_rel_query {
	u16	buffer_rel_condition;
	u16	reserved;
	u32	trigger_type;
	u32	trigger_info_dwords[2];
};
#define MPT3_DIAG_BUFFER_NOT_RELEASED	(0x00)
#define MPT3_DIAG_BUFFER_RELEASED	(0x01)
#define MPT3_DIAG_BUFFER_REL_IOCTL	(0x02 | MPT3_DIAG_BUFFER_RELEASED)
#define MPT3_DIAG_BUFFER_REL_TRIGGER	(0x04 | MPT3_DIAG_BUFFER_RELEASED)
#define MPT3_DIAG_BUFFER_REL_SYSFS	(0x08 | MPT3_DIAG_BUFFER_RELEASED)
#define MPT_DIAG_RESET_ISSUED_BY_DRIVER 0x00000000
#define MPT_DIAG_RESET_ISSUED_BY_USER	0x00000001
typedef void (*MPT3SAS_FLUSH_RUNNING_CMDS)(struct MPT3SAS_ADAPTER *ioc);
struct MPT3SAS_ADAPTER {
	struct list_head list;
	struct Scsi_Host *shost;
	u8		id;
	int		cpu_count;
	char		name[MPT_NAME_LENGTH];
	char		driver_name[MPT_NAME_LENGTH - 8];
	char		tmp_string[MPT_STRING_LENGTH];
	struct pci_dev	*pdev;
	Mpi2SystemInterfaceRegs_t __iomem *chip;
	phys_addr_t	chip_phys;
	int		logging_level;
	int		fwfault_debug;
	u8		ir_firmware;
	int		bars;
	u8		mask_interrupts;
	char		fault_reset_work_q_name[20];
	struct workqueue_struct *fault_reset_work_q;
	struct delayed_work fault_reset_work;
	char		firmware_event_name[20];
	struct workqueue_struct	*firmware_event_thread;
	spinlock_t	fw_event_lock;
	struct list_head fw_event_list;
	struct fw_event_work	*current_event;
	u8		fw_events_cleanup;
	int		aen_event_read_flag;
	u8		broadcast_aen_busy;
	u16		broadcast_aen_pending;
	u8		shost_recovery;
	u8		got_task_abort_from_ioctl;
	struct mutex	reset_in_progress_mutex;
	spinlock_t	ioc_reset_in_progress_lock;
	u8		ioc_link_reset_in_progress;
	u8		ignore_loginfos;
	u8		remove_host;
	u8		pci_error_recovery;
	u8		wait_for_discovery_to_complete;
	u8		is_driver_loading;
	u8		port_enable_failed;
	u8		start_scan;
	u16		start_scan_failed;
	u8		msix_enable;
	u16		msix_vector_count;
	u8		*cpu_msix_table;
	u16		cpu_msix_table_sz;
	resource_size_t __iomem **reply_post_host_index;
	u32		ioc_reset_count;
	MPT3SAS_FLUSH_RUNNING_CMDS schedule_dead_ioc_flush_running_cmds;
	u32             non_operational_loop;
	u8              ioc_coredump_loop;
	u32		timestamp_update_count;
	u32		time_sync_interval;
	atomic64_t      total_io_cnt;
	atomic64_t	high_iops_outstanding;
	bool            msix_load_balance;
	u16		thresh_hold;
	u8		high_iops_queues;
	u8		iopoll_q_start_index;
	u32             drv_internal_flags;
	u32		drv_support_bitmap;
	u32             dma_mask;
	bool		enable_sdev_max_qd;
	bool		use_32bit_dma;
	struct io_uring_poll_queue *io_uring_poll_queues;
	u8		scsi_io_cb_idx;
	u8		tm_cb_idx;
	u8		transport_cb_idx;
	u8		scsih_cb_idx;
	u8		ctl_cb_idx;
	u8		base_cb_idx;
	u8		port_enable_cb_idx;
	u8		config_cb_idx;
	u8		tm_tr_cb_idx;
	u8		tm_tr_volume_cb_idx;
	u8		tm_sas_control_cb_idx;
	struct _internal_cmd base_cmds;
	struct _internal_cmd port_enable_cmds;
	struct _internal_cmd transport_cmds;
	struct _internal_cmd scsih_cmds;
	struct _internal_cmd tm_cmds;
	struct _internal_cmd ctl_cmds;
	struct _internal_cmd config_cmds;
	MPT_ADD_SGE	base_add_sg_single;
	MPT_BUILD_SG_SCMD build_sg_scmd;
	MPT_BUILD_SG    build_sg;
	MPT_BUILD_ZERO_LEN_SGE build_zero_len_sge;
	u16             sge_size_ieee;
	u16		hba_mpi_version_belonged;
	MPT_BUILD_SG    build_sg_mpi;
	MPT_BUILD_ZERO_LEN_SGE build_zero_len_sge_mpi;
	NVME_BUILD_PRP  build_nvme_prp;
	u32		event_type[MPI2_EVENT_NOTIFY_EVENTMASK_WORDS];
	u32		event_context;
	void		*event_log;
	u32		event_masks[MPI2_EVENT_NOTIFY_EVENTMASK_WORDS];
	u8		tm_custom_handling;
	u8		nvme_abort_timeout;
	u16		max_shutdown_latency;
	u16		max_wideport_qd;
	u16		max_narrowport_qd;
	u16		max_nvme_qd;
	u8		max_sata_qd;
	struct mpt3sas_facts facts;
	struct mpt3sas_facts prev_fw_facts;
	struct mpt3sas_port_facts *pfacts;
	Mpi2ManufacturingPage0_t manu_pg0;
	struct Mpi2ManufacturingPage10_t manu_pg10;
	struct Mpi2ManufacturingPage11_t manu_pg11;
	Mpi2BiosPage2_t	bios_pg2;
	Mpi2BiosPage3_t	bios_pg3;
	Mpi2IOCPage8_t ioc_pg8;
	Mpi2IOUnitPage0_t iounit_pg0;
	Mpi2IOUnitPage1_t iounit_pg1;
	Mpi2IOUnitPage8_t iounit_pg8;
	Mpi2IOCPage1_t	ioc_pg1_copy;
	struct _boot_device req_boot_device;
	struct _boot_device req_alt_boot_device;
	struct _boot_device current_boot_device;
	struct _sas_node sas_hba;
	struct list_head sas_expander_list;
	struct list_head enclosure_list;
	spinlock_t	sas_node_lock;
	struct list_head sas_device_list;
	struct list_head sas_device_init_list;
	spinlock_t	sas_device_lock;
	struct list_head pcie_device_list;
	struct list_head pcie_device_init_list;
	spinlock_t      pcie_device_lock;
	struct list_head raid_device_list;
	spinlock_t	raid_device_lock;
	u8		io_missing_delay;
	u16		device_missing_delay;
	int		sas_id;
	int		pcie_target_id;
	void		*blocking_handles;
	void		*pd_handles;
	u16		pd_handles_sz;
	void		*pend_os_device_add;
	u16		pend_os_device_add_sz;
	u16		config_page_sz;
	void		*config_page;
	dma_addr_t	config_page_dma;
	void		*config_vaddr;
	u16		hba_queue_depth;
	u16		sge_size;
	u16		scsiio_depth;
	u16		request_sz;
	u8		*request;
	dma_addr_t	request_dma;
	u32		request_dma_sz;
	struct pcie_sg_list *pcie_sg_lookup;
	spinlock_t	scsi_lookup_lock;
	int		pending_io_count;
	wait_queue_head_t reset_wq;
	u16		*io_queue_num;
	struct dma_pool *pcie_sgl_dma_pool;
	u32		page_size;
	struct chain_lookup *chain_lookup;
	struct list_head free_chain_list;
	struct dma_pool *chain_dma_pool;
	ulong		chain_pages;
	u16		max_sges_in_main_message;
	u16		max_sges_in_chain_message;
	u16		chains_needed_per_io;
	u32		chain_depth;
	u16		chain_segment_sz;
	u16		chains_per_prp_buffer;
	u16		hi_priority_smid;
	u8		*hi_priority;
	dma_addr_t	hi_priority_dma;
	u16		hi_priority_depth;
	struct request_tracker *hpr_lookup;
	struct list_head hpr_free_list;
	u16		internal_smid;
	u8		*internal;
	dma_addr_t	internal_dma;
	u16		internal_depth;
	struct request_tracker *internal_lookup;
	struct list_head internal_free_list;
	u8		*sense;
	dma_addr_t	sense_dma;
	struct dma_pool *sense_dma_pool;
	u16		reply_sz;
	u8		*reply;
	dma_addr_t	reply_dma;
	u32		reply_dma_max_address;
	u32		reply_dma_min_address;
	struct dma_pool *reply_dma_pool;
	u16		reply_free_queue_depth;
	__le32		*reply_free;
	dma_addr_t	reply_free_dma;
	struct dma_pool *reply_free_dma_pool;
	u32		reply_free_host_index;
	u16		reply_post_queue_depth;
	struct reply_post_struct *reply_post;
	u8		rdpq_array_capable;
	u8		rdpq_array_enable;
	u8		rdpq_array_enable_assigned;
	struct dma_pool *reply_post_free_dma_pool;
	struct dma_pool *reply_post_free_array_dma_pool;
	Mpi2IOCInitRDPQArrayEntry *reply_post_free_array;
	dma_addr_t reply_post_free_array_dma;
	u8		reply_queue_count;
	struct list_head reply_queue_list;
	u8		combined_reply_queue;
	u8		combined_reply_index_count;
	u8		smp_affinity_enable;
	resource_size_t	__iomem **replyPostRegisterIndex;
	struct list_head delayed_tr_list;
	struct list_head delayed_tr_volume_list;
	struct list_head delayed_sc_list;
	struct list_head delayed_event_ack_list;
	u8		temp_sensors_count;
	struct mutex pci_access_mutex;
	u8		*diag_buffer[MPI2_DIAG_BUF_TYPE_COUNT];
	u32		diag_buffer_sz[MPI2_DIAG_BUF_TYPE_COUNT];
	dma_addr_t	diag_buffer_dma[MPI2_DIAG_BUF_TYPE_COUNT];
	u8		diag_buffer_status[MPI2_DIAG_BUF_TYPE_COUNT];
	u32		unique_id[MPI2_DIAG_BUF_TYPE_COUNT];
	u32		product_specific[MPI2_DIAG_BUF_TYPE_COUNT][23];
	u32		diagnostic_flags[MPI2_DIAG_BUF_TYPE_COUNT];
	u32		ring_buffer_offset;
	u32		ring_buffer_sz;
	struct htb_rel_query htb_rel;
	u8 reset_from_user;
	u8		is_warpdrive;
	u8		is_mcpu_endpoint;
	u8		hide_ir_msg;
	u8		mfg_pg10_hide_flag;
	u8		hide_drives;
	spinlock_t	diag_trigger_lock;
	u8		diag_trigger_active;
	u8		atomic_desc_capable;
	BASE_READ_REG	base_readl;
	BASE_READ_REG	base_readl_ext_retry;
	struct SL_WH_MASTER_TRIGGER_T diag_trigger_master;
	struct SL_WH_EVENT_TRIGGERS_T diag_trigger_event;
	struct SL_WH_SCSI_TRIGGERS_T diag_trigger_scsi;
	struct SL_WH_MPI_TRIGGERS_T diag_trigger_mpi;
	u8		supports_trigger_pages;
	void		*device_remove_in_progress;
	u16		device_remove_in_progress_sz;
	u8		is_gen35_ioc;
	u8		is_aero_ioc;
	struct dentry	*debugfs_root;
	struct dentry	*ioc_dump;
	PUT_SMID_IO_FP_HIP put_smid_scsi_io;
	PUT_SMID_IO_FP_HIP put_smid_fast_path;
	PUT_SMID_IO_FP_HIP put_smid_hi_priority;
	PUT_SMID_DEFAULT put_smid_default;
	GET_MSIX_INDEX get_msix_index_for_smlio;
	u8		multipath_on_hba;
	struct list_head port_table_list;
};
struct mpt3sas_debugfs_buffer {
	void	*buf;
	u32	len;
};
#define MPT_DRV_SUPPORT_BITMAP_MEMMOVE 0x00000001
#define MPT_DRV_SUPPORT_BITMAP_ADDNLQUERY	0x00000002
#define MPT_DRV_INTERNAL_FIRST_PE_ISSUED		0x00000001
typedef u8 (*MPT_CALLBACK)(struct MPT3SAS_ADAPTER *ioc, u16 smid, u8 msix_index,
	u32 reply);
struct ATTO_SAS_NVRAM {
	u8		Signature[4];
	u8		Version;
#define ATTO_SASNVR_VERSION		0
	u8		Checksum;
#define ATTO_SASNVR_CKSUM_SEED	0x5A
	u8		Pad[10];
	u8		SasAddr[8];
#define ATTO_SAS_ADDR_ALIGN		64
	u8		Reserved[232];
};
#define ATTO_SAS_ADDR_DEVNAME_BIAS		63
union ATTO_SAS_ADDRESS {
	U8		b[8];
	U16		w[4];
	U32		d[2];
	U64		q;
};
extern struct list_head mpt3sas_ioc_list;
extern char    driver_name[MPT_NAME_LENGTH];
extern spinlock_t gioc_lock;
void mpt3sas_base_start_watchdog(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_base_stop_watchdog(struct MPT3SAS_ADAPTER *ioc);
int mpt3sas_base_attach(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_base_detach(struct MPT3SAS_ADAPTER *ioc);
int mpt3sas_base_map_resources(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_base_free_resources(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_free_enclosure_list(struct MPT3SAS_ADAPTER *ioc);
int mpt3sas_base_hard_reset_handler(struct MPT3SAS_ADAPTER *ioc,
	enum reset_type type);
void *mpt3sas_base_get_msg_frame(struct MPT3SAS_ADAPTER *ioc, u16 smid);
void *mpt3sas_base_get_sense_buffer(struct MPT3SAS_ADAPTER *ioc, u16 smid);
__le32 mpt3sas_base_get_sense_buffer_dma(struct MPT3SAS_ADAPTER *ioc,
	u16 smid);
void *mpt3sas_base_get_pcie_sgl(struct MPT3SAS_ADAPTER *ioc, u16 smid);
dma_addr_t mpt3sas_base_get_pcie_sgl_dma(struct MPT3SAS_ADAPTER *ioc, u16 smid);
void mpt3sas_base_sync_reply_irqs(struct MPT3SAS_ADAPTER *ioc, u8 poll);
void mpt3sas_base_mask_interrupts(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_base_unmask_interrupts(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_base_put_smid_fast_path(struct MPT3SAS_ADAPTER *ioc, u16 smid,
	u16 handle);
void mpt3sas_base_put_smid_hi_priority(struct MPT3SAS_ADAPTER *ioc, u16 smid,
	u16 msix_task);
void mpt3sas_base_put_smid_nvme_encap(struct MPT3SAS_ADAPTER *ioc, u16 smid);
void mpt3sas_base_put_smid_default(struct MPT3SAS_ADAPTER *ioc, u16 smid);
u16 mpt3sas_base_get_smid_hpr(struct MPT3SAS_ADAPTER *ioc, u8 cb_idx);
u16 mpt3sas_base_get_smid_scsiio(struct MPT3SAS_ADAPTER *ioc, u8 cb_idx,
		struct scsi_cmnd *scmd);
void mpt3sas_base_clear_st(struct MPT3SAS_ADAPTER *ioc,
		struct scsiio_tracker *st);
u16 mpt3sas_base_get_smid(struct MPT3SAS_ADAPTER *ioc, u8 cb_idx);
void mpt3sas_base_free_smid(struct MPT3SAS_ADAPTER *ioc, u16 smid);
void mpt3sas_base_initialize_callback_handler(void);
u8 mpt3sas_base_register_callback_handler(MPT_CALLBACK cb_func);
void mpt3sas_base_release_callback_handler(u8 cb_idx);
u8 mpt3sas_base_done(struct MPT3SAS_ADAPTER *ioc, u16 smid, u8 msix_index,
	u32 reply);
u8 mpt3sas_port_enable_done(struct MPT3SAS_ADAPTER *ioc, u16 smid,
	u8 msix_index, u32 reply);
void *mpt3sas_base_get_reply_virt_addr(struct MPT3SAS_ADAPTER *ioc,
	u32 phys_addr);
u32 mpt3sas_base_get_iocstate(struct MPT3SAS_ADAPTER *ioc, int cooked);
void mpt3sas_base_fault_info(struct MPT3SAS_ADAPTER *ioc , u16 fault_code);
#define mpt3sas_print_fault_code(ioc, fault_code) \
do { pr_err("%s fault info from func: %s\n", ioc->name, __func__); \
	mpt3sas_base_fault_info(ioc, fault_code); } while (0)
void mpt3sas_base_coredump_info(struct MPT3SAS_ADAPTER *ioc, u16 fault_code);
#define mpt3sas_print_coredump_info(ioc, fault_code) \
do { pr_err("%s fault info from func: %s\n", ioc->name, __func__); \
	mpt3sas_base_coredump_info(ioc, fault_code); } while (0)
int mpt3sas_base_wait_for_coredump_completion(struct MPT3SAS_ADAPTER *ioc,
		const char *caller);
int mpt3sas_base_sas_iounit_control(struct MPT3SAS_ADAPTER *ioc,
	Mpi2SasIoUnitControlReply_t *mpi_reply,
	Mpi2SasIoUnitControlRequest_t *mpi_request);
int mpt3sas_base_scsi_enclosure_processor(struct MPT3SAS_ADAPTER *ioc,
	Mpi2SepReply_t *mpi_reply, Mpi2SepRequest_t *mpi_request);
void mpt3sas_base_validate_event_type(struct MPT3SAS_ADAPTER *ioc,
	u32 *event_type);
void mpt3sas_halt_firmware(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_base_update_missing_delay(struct MPT3SAS_ADAPTER *ioc,
	u16 device_missing_delay, u8 io_missing_delay);
int mpt3sas_base_check_for_fault_and_issue_reset(
	struct MPT3SAS_ADAPTER *ioc);
int mpt3sas_port_enable(struct MPT3SAS_ADAPTER *ioc);
void
mpt3sas_wait_for_commands_to_complete(struct MPT3SAS_ADAPTER *ioc);
u8 mpt3sas_base_check_cmd_timeout(struct MPT3SAS_ADAPTER *ioc,
	u8 status, void *mpi_request, int sz);
#define mpt3sas_check_cmd_timeout(ioc, status, mpi_request, sz, issue_reset) \
do {	ioc_err(ioc, "In func: %s\n", __func__); \
	issue_reset = mpt3sas_base_check_cmd_timeout(ioc, \
	status, mpi_request, sz); } while (0)
int mpt3sas_wait_for_ioc(struct MPT3SAS_ADAPTER *ioc, int wait_count);
int mpt3sas_base_make_ioc_ready(struct MPT3SAS_ADAPTER *ioc, enum reset_type type);
void mpt3sas_base_free_irq(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_base_disable_msix(struct MPT3SAS_ADAPTER *ioc);
int mpt3sas_blk_mq_poll(struct Scsi_Host *shost, unsigned int queue_num);
void mpt3sas_base_pause_mq_polling(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_base_resume_mq_polling(struct MPT3SAS_ADAPTER *ioc);
struct scsi_cmnd *mpt3sas_scsih_scsi_lookup_get(struct MPT3SAS_ADAPTER *ioc,
	u16 smid);
u8 mpt3sas_scsih_event_callback(struct MPT3SAS_ADAPTER *ioc, u8 msix_index,
	u32 reply);
void mpt3sas_scsih_pre_reset_handler(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_scsih_clear_outstanding_scsi_tm_commands(
	struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_scsih_reset_done_handler(struct MPT3SAS_ADAPTER *ioc);
int mpt3sas_scsih_issue_tm(struct MPT3SAS_ADAPTER *ioc, u16 handle,
	uint channel, uint id, u64 lun, u8 type, u16 smid_task,
	u16 msix_task, u8 timeout, u8 tr_method);
int mpt3sas_scsih_issue_locked_tm(struct MPT3SAS_ADAPTER *ioc, u16 handle,
	uint channel, uint id, u64 lun, u8 type, u16 smid_task,
	u16 msix_task, u8 timeout, u8 tr_method);
void mpt3sas_scsih_set_tm_flag(struct MPT3SAS_ADAPTER *ioc, u16 handle);
void mpt3sas_scsih_clear_tm_flag(struct MPT3SAS_ADAPTER *ioc, u16 handle);
void mpt3sas_expander_remove(struct MPT3SAS_ADAPTER *ioc, u64 sas_address,
	struct hba_port *port);
void mpt3sas_device_remove_by_sas_address(struct MPT3SAS_ADAPTER *ioc,
	u64 sas_address, struct hba_port *port);
u8 mpt3sas_check_for_pending_internal_cmds(struct MPT3SAS_ADAPTER *ioc,
	u16 smid);
struct hba_port *
mpt3sas_get_port_by_id(struct MPT3SAS_ADAPTER *ioc, u8 port,
	u8 bypass_dirty_port_flag);
struct _sas_node *mpt3sas_scsih_expander_find_by_handle(
	struct MPT3SAS_ADAPTER *ioc, u16 handle);
struct _sas_node *mpt3sas_scsih_expander_find_by_sas_address(
	struct MPT3SAS_ADAPTER *ioc, u64 sas_address,
	struct hba_port *port);
struct _sas_device *mpt3sas_get_sdev_by_addr(
	 struct MPT3SAS_ADAPTER *ioc, u64 sas_address,
	 struct hba_port *port);
struct _sas_device *__mpt3sas_get_sdev_by_addr(
	 struct MPT3SAS_ADAPTER *ioc, u64 sas_address,
	 struct hba_port *port);
struct _sas_device *mpt3sas_get_sdev_by_handle(struct MPT3SAS_ADAPTER *ioc,
	u16 handle);
struct _pcie_device *mpt3sas_get_pdev_by_handle(struct MPT3SAS_ADAPTER *ioc,
	u16 handle);
void mpt3sas_port_enable_complete(struct MPT3SAS_ADAPTER *ioc);
struct _raid_device *
mpt3sas_raid_device_find_by_handle(struct MPT3SAS_ADAPTER *ioc, u16 handle);
void mpt3sas_scsih_change_queue_depth(struct scsi_device *sdev, int qdepth);
struct _sas_device *
__mpt3sas_get_sdev_by_rphy(struct MPT3SAS_ADAPTER *ioc, struct sas_rphy *rphy);
struct virtual_phy *
mpt3sas_get_vphy_by_phy(struct MPT3SAS_ADAPTER *ioc,
	struct hba_port *port, u32 phy);
u8 mpt3sas_config_done(struct MPT3SAS_ADAPTER *ioc, u16 smid, u8 msix_index,
	u32 reply);
int mpt3sas_config_get_number_hba_phys(struct MPT3SAS_ADAPTER *ioc,
	u8 *num_phys);
int mpt3sas_config_get_manufacturing_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2ManufacturingPage0_t *config_page);
int mpt3sas_config_get_manufacturing_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2ManufacturingPage1_t *config_page);
int mpt3sas_config_get_manufacturing_pg7(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2ManufacturingPage7_t *config_page,
	u16 sz);
int mpt3sas_config_get_manufacturing_pg10(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply,
	struct Mpi2ManufacturingPage10_t *config_page);
int mpt3sas_config_get_manufacturing_pg11(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply,
	struct Mpi2ManufacturingPage11_t  *config_page);
int mpt3sas_config_set_manufacturing_pg11(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply,
	struct Mpi2ManufacturingPage11_t *config_page);
int mpt3sas_config_get_bios_pg2(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2BiosPage2_t *config_page);
int mpt3sas_config_get_bios_pg3(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2BiosPage3_t *config_page);
int mpt3sas_config_set_bios_pg4(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2BiosPage4_t *config_page,
	int sz_config_page);
int mpt3sas_config_get_bios_pg4(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2BiosPage4_t *config_page,
	int sz_config_page);
int mpt3sas_config_get_iounit_pg0(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2IOUnitPage0_t *config_page);
int mpt3sas_config_get_sas_device_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2SasDevicePage0_t *config_page,
	u32 form, u32 handle);
int mpt3sas_config_get_sas_device_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2SasDevicePage1_t *config_page,
	u32 form, u32 handle);
int mpt3sas_config_get_pcie_device_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26PCIeDevicePage0_t *config_page,
	u32 form, u32 handle);
int mpt3sas_config_get_pcie_device_pg2(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26PCIeDevicePage2_t *config_page,
	u32 form, u32 handle);
int mpt3sas_config_get_pcie_iounit_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26PCIeIOUnitPage1_t *config_page,
	u16 sz);
int mpt3sas_config_get_sas_iounit_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2SasIOUnitPage0_t *config_page,
	u16 sz);
int mpt3sas_config_get_iounit_pg1(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2IOUnitPage1_t *config_page);
int mpt3sas_config_get_iounit_pg3(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2IOUnitPage3_t *config_page, u16 sz);
int mpt3sas_config_set_iounit_pg1(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2IOUnitPage1_t *config_page);
int mpt3sas_config_get_iounit_pg8(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2IOUnitPage8_t *config_page);
int mpt3sas_config_get_sas_iounit_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2SasIOUnitPage1_t *config_page,
	u16 sz);
int mpt3sas_config_set_sas_iounit_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2SasIOUnitPage1_t *config_page,
	u16 sz);
int mpt3sas_config_get_ioc_pg1(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2IOCPage1_t *config_page);
int mpt3sas_config_set_ioc_pg1(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2IOCPage1_t *config_page);
int mpt3sas_config_get_ioc_pg8(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2IOCPage8_t *config_page);
int mpt3sas_config_get_expander_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2ExpanderPage0_t *config_page,
	u32 form, u32 handle);
int mpt3sas_config_get_expander_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2ExpanderPage1_t *config_page,
	u32 phy_number, u16 handle);
int mpt3sas_config_get_enclosure_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2SasEnclosurePage0_t *config_page,
	u32 form, u32 handle);
int mpt3sas_config_get_phy_pg0(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2SasPhyPage0_t *config_page, u32 phy_number);
int mpt3sas_config_get_phy_pg1(struct MPT3SAS_ADAPTER *ioc, Mpi2ConfigReply_t
	*mpi_reply, Mpi2SasPhyPage1_t *config_page, u32 phy_number);
int mpt3sas_config_get_raid_volume_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2RaidVolPage1_t *config_page, u32 form,
	u32 handle);
int mpt3sas_config_get_number_pds(struct MPT3SAS_ADAPTER *ioc, u16 handle,
	u8 *num_pds);
int mpt3sas_config_get_raid_volume_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2RaidVolPage0_t *config_page, u32 form,
	u32 handle, u16 sz);
int mpt3sas_config_get_phys_disk_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi2RaidPhysDiskPage0_t *config_page,
	u32 form, u32 form_specific);
int mpt3sas_config_get_volume_handle(struct MPT3SAS_ADAPTER *ioc, u16 pd_handle,
	u16 *volume_handle);
int mpt3sas_config_get_volume_wwid(struct MPT3SAS_ADAPTER *ioc,
	u16 volume_handle, u64 *wwid);
int
mpt3sas_config_get_driver_trigger_pg0(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage0_t *config_page);
int
mpt3sas_config_get_driver_trigger_pg1(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage1_t *config_page);
int
mpt3sas_config_get_driver_trigger_pg2(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage2_t *config_page);
int
mpt3sas_config_get_driver_trigger_pg3(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage3_t *config_page);
int
mpt3sas_config_get_driver_trigger_pg4(struct MPT3SAS_ADAPTER *ioc,
	Mpi2ConfigReply_t *mpi_reply, Mpi26DriverTriggerPage4_t *config_page);
int
mpt3sas_config_update_driver_trigger_pg1(struct MPT3SAS_ADAPTER *ioc,
	struct SL_WH_MASTER_TRIGGER_T *master_tg, bool set);
int
mpt3sas_config_update_driver_trigger_pg2(struct MPT3SAS_ADAPTER *ioc,
	struct SL_WH_EVENT_TRIGGERS_T *event_tg, bool set);
int
mpt3sas_config_update_driver_trigger_pg3(struct MPT3SAS_ADAPTER *ioc,
	struct SL_WH_SCSI_TRIGGERS_T *scsi_tg, bool set);
int
mpt3sas_config_update_driver_trigger_pg4(struct MPT3SAS_ADAPTER *ioc,
	struct SL_WH_MPI_TRIGGERS_T *mpi_tg, bool set);
extern const struct attribute_group *mpt3sas_host_groups[];
extern const struct attribute_group *mpt3sas_dev_groups[];
void mpt3sas_ctl_init(ushort hbas_to_enumerate);
void mpt3sas_ctl_exit(ushort hbas_to_enumerate);
u8 mpt3sas_ctl_done(struct MPT3SAS_ADAPTER *ioc, u16 smid, u8 msix_index,
	u32 reply);
void mpt3sas_ctl_pre_reset_handler(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_ctl_clear_outstanding_ioctls(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_ctl_reset_done_handler(struct MPT3SAS_ADAPTER *ioc);
u8 mpt3sas_ctl_event_callback(struct MPT3SAS_ADAPTER *ioc,
	u8 msix_index, u32 reply);
void mpt3sas_ctl_add_to_event_log(struct MPT3SAS_ADAPTER *ioc,
	Mpi2EventNotificationReply_t *mpi_reply);
void mpt3sas_enable_diag_buffer(struct MPT3SAS_ADAPTER *ioc,
	u8 bits_to_register);
int mpt3sas_send_diag_release(struct MPT3SAS_ADAPTER *ioc, u8 buffer_type,
	u8 *issue_reset);
extern struct scsi_transport_template *mpt3sas_transport_template;
u8 mpt3sas_transport_done(struct MPT3SAS_ADAPTER *ioc, u16 smid, u8 msix_index,
	u32 reply);
struct _sas_port *mpt3sas_transport_port_add(struct MPT3SAS_ADAPTER *ioc,
	u16 handle, u64 sas_address, struct hba_port *port);
void mpt3sas_transport_port_remove(struct MPT3SAS_ADAPTER *ioc, u64 sas_address,
	u64 sas_address_parent, struct hba_port *port);
int mpt3sas_transport_add_host_phy(struct MPT3SAS_ADAPTER *ioc, struct _sas_phy
	*mpt3sas_phy, Mpi2SasPhyPage0_t phy_pg0, struct device *parent_dev);
int mpt3sas_transport_add_expander_phy(struct MPT3SAS_ADAPTER *ioc,
	struct _sas_phy *mpt3sas_phy, Mpi2ExpanderPage1_t expander_pg1,
	struct device *parent_dev);
void mpt3sas_transport_update_links(struct MPT3SAS_ADAPTER *ioc,
	u64 sas_address, u16 handle, u8 phy_number, u8 link_rate,
	struct hba_port *port);
extern struct sas_function_template mpt3sas_transport_functions;
extern struct scsi_transport_template *mpt3sas_transport_template;
void
mpt3sas_transport_del_phy_from_an_existing_port(struct MPT3SAS_ADAPTER *ioc,
	struct _sas_node *sas_node, struct _sas_phy *mpt3sas_phy);
void
mpt3sas_transport_add_phy_to_an_existing_port(struct MPT3SAS_ADAPTER *ioc,
	struct _sas_node *sas_node, struct _sas_phy *mpt3sas_phy,
	u64 sas_address, struct hba_port *port);
void mpt3sas_send_trigger_data_event(struct MPT3SAS_ADAPTER *ioc,
	struct SL_WH_TRIGGERS_EVENT_DATA_T *event_data);
void mpt3sas_process_trigger_data(struct MPT3SAS_ADAPTER *ioc,
	struct SL_WH_TRIGGERS_EVENT_DATA_T *event_data);
void mpt3sas_trigger_master(struct MPT3SAS_ADAPTER *ioc,
	u32 trigger_bitmask);
void mpt3sas_trigger_event(struct MPT3SAS_ADAPTER *ioc, u16 event,
	u16 log_entry_qualifier);
void mpt3sas_trigger_scsi(struct MPT3SAS_ADAPTER *ioc, u8 sense_key,
	u8 asc, u8 ascq);
void mpt3sas_trigger_mpi(struct MPT3SAS_ADAPTER *ioc, u16 ioc_status,
	u32 loginfo);
u8 mpt3sas_get_num_volumes(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_init_warpdrive_properties(struct MPT3SAS_ADAPTER *ioc,
	struct _raid_device *raid_device);
void
mpt3sas_setup_direct_io(struct MPT3SAS_ADAPTER *ioc, struct scsi_cmnd *scmd,
	struct _raid_device *raid_device, Mpi25SCSIIORequest_t *mpi_request);
bool scsih_ncq_prio_supp(struct scsi_device *sdev);
void mpt3sas_setup_debugfs(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_destroy_debugfs(struct MPT3SAS_ADAPTER *ioc);
void mpt3sas_init_debugfs(void);
void mpt3sas_exit_debugfs(void);
static inline int
mpt3sas_scsih_is_pcie_scsi_device(u32 device_info)
{
	if ((device_info &
	    MPI26_PCIE_DEVINFO_MASK_DEVICE_TYPE) == MPI26_PCIE_DEVINFO_SCSI)
		return 1;
	else
		return 0;
}
#endif  
