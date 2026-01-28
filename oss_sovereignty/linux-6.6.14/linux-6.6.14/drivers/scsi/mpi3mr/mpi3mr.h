#ifndef MPI3MR_H_INCLUDED
#define MPI3MR_H_INCLUDED
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/blk-mq-pci.h>
#include <linux/delay.h>
#include <linux/dmapool.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/utsname.h>
#include <linux/workqueue.h>
#include <asm/unaligned.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include <uapi/scsi/scsi_bsg_mpi3mr.h>
#include <scsi/scsi_transport_sas.h>
#include "mpi/mpi30_transport.h"
#include "mpi/mpi30_cnfg.h"
#include "mpi/mpi30_image.h"
#include "mpi/mpi30_init.h"
#include "mpi/mpi30_ioc.h"
#include "mpi/mpi30_sas.h"
#include "mpi/mpi30_pci.h"
#include "mpi3mr_debug.h"
extern spinlock_t mrioc_list_lock;
extern struct list_head mrioc_list;
extern int prot_mask;
extern atomic64_t event_counter;
#define MPI3MR_DRIVER_VERSION	"8.5.0.0.0"
#define MPI3MR_DRIVER_RELDATE	"24-July-2023"
#define MPI3MR_DRIVER_NAME	"mpi3mr"
#define MPI3MR_DRIVER_LICENSE	"GPL"
#define MPI3MR_DRIVER_AUTHOR	"Broadcom Inc. <mpi3mr-linuxdrv.pdl@broadcom.com>"
#define MPI3MR_DRIVER_DESC	"MPI3 Storage Controller Device Driver"
#define MPI3MR_NAME_LENGTH	32
#define IOCNAME			"%s: "
#define MPI3MR_DEFAULT_MAX_IO_SIZE	(1 * 1024 * 1024)
#define MPI3MR_PAGE_SIZE_4K		4096
#define MPI3MR_DEFAULT_SGL_ENTRIES	256
#define MPI3MR_MAX_SGL_ENTRIES		2048
#define MPI3MR_MAX_CMDS_LUN	128
#define MPI3MR_MAX_CDB_LENGTH	32
#define MPI3MR_ADMIN_REQ_Q_SIZE		(2 * MPI3MR_PAGE_SIZE_4K)
#define MPI3MR_ADMIN_REPLY_Q_SIZE	(4 * MPI3MR_PAGE_SIZE_4K)
#define MPI3MR_ADMIN_REQ_FRAME_SZ	128
#define MPI3MR_ADMIN_REPLY_FRAME_SZ	16
#define MPI3MR_OP_REQ_Q_QD		512
#define MPI3MR_OP_REP_Q_QD		1024
#define MPI3MR_OP_REP_Q_QD4K		4096
#define MPI3MR_OP_REQ_Q_SEG_SIZE	4096
#define MPI3MR_OP_REP_Q_SEG_SIZE	4096
#define MPI3MR_MAX_SEG_LIST_SIZE	4096
#define MPI3MR_HOSTTAG_INVALID		0xFFFF
#define MPI3MR_HOSTTAG_INITCMDS		1
#define MPI3MR_HOSTTAG_BSG_CMDS		2
#define MPI3MR_HOSTTAG_PEL_ABORT	3
#define MPI3MR_HOSTTAG_PEL_WAIT		4
#define MPI3MR_HOSTTAG_BLK_TMS		5
#define MPI3MR_HOSTTAG_CFG_CMDS		6
#define MPI3MR_HOSTTAG_TRANSPORT_CMDS	7
#define MPI3MR_NUM_DEVRMCMD		16
#define MPI3MR_HOSTTAG_DEVRMCMD_MIN	(MPI3MR_HOSTTAG_TRANSPORT_CMDS + 1)
#define MPI3MR_HOSTTAG_DEVRMCMD_MAX	(MPI3MR_HOSTTAG_DEVRMCMD_MIN + \
						MPI3MR_NUM_DEVRMCMD - 1)
#define MPI3MR_INTERNAL_CMDS_RESVD	MPI3MR_HOSTTAG_DEVRMCMD_MAX
#define MPI3MR_NUM_EVTACKCMD		4
#define MPI3MR_HOSTTAG_EVTACKCMD_MIN	(MPI3MR_HOSTTAG_DEVRMCMD_MAX + 1)
#define MPI3MR_HOSTTAG_EVTACKCMD_MAX	(MPI3MR_HOSTTAG_EVTACKCMD_MIN + \
					MPI3MR_NUM_EVTACKCMD - 1)
#define MPI3MR_HOST_IOS_KDUMP		128
#define MPI3MR_INTADMCMD_TIMEOUT		60
#define MPI3MR_PORTENABLE_TIMEOUT		300
#define MPI3MR_PORTENABLE_POLL_INTERVAL		5
#define MPI3MR_ABORTTM_TIMEOUT			60
#define MPI3MR_RESETTM_TIMEOUT			60
#define MPI3MR_RESET_HOST_IOWAIT_TIMEOUT	5
#define MPI3MR_TSUPDATE_INTERVAL		900
#define MPI3MR_DEFAULT_SHUTDOWN_TIME		120
#define	MPI3MR_RAID_ERRREC_RESET_TIMEOUT	180
#define MPI3MR_PREPARE_FOR_RESET_TIMEOUT	180
#define MPI3MR_RESET_ACK_TIMEOUT		30
#define MPI3MR_MUR_TIMEOUT			120
#define MPI3MR_WATCHDOG_INTERVAL		1000  
#define MPI3MR_DEFAULT_CFG_PAGE_SZ		1024  
#define MPI3MR_RESET_TOPOLOGY_SETTLE_TIME	10
#define MPI3MR_SCMD_TIMEOUT    (60 * HZ)
#define MPI3MR_EH_SCMD_TIMEOUT (60 * HZ)
#define MPI3MR_CMD_NOTUSED	0x8000
#define MPI3MR_CMD_COMPLETE	0x0001
#define MPI3MR_CMD_PENDING	0x0002
#define MPI3MR_CMD_REPLY_VALID	0x0004
#define MPI3MR_CMD_RESET	0x0008
#define MPI3MR_NUM_EVT_REPLIES	64
#define MPI3MR_SENSE_BUF_SZ	256
#define MPI3MR_SENSEBUF_FACTOR	3
#define MPI3MR_CHAINBUF_FACTOR	3
#define MPI3MR_CHAINBUFDIX_FACTOR	2
#define MPI3MR_INVALID_DEV_HANDLE	0xFFFF
#define MPI3MR_HOSTDIAG_UNLOCK_RETRY_COUNT	5
#define MPI3MR_MAX_RESET_RETRY_COUNT		3
#define MPI3MR_RI_MASK_RESPCODE		(0x000000FF)
#define MPI3MR_RSP_IO_QUEUED_ON_IOC \
			MPI3_SCSITASKMGMT_RSPCODE_IO_QUEUED_ON_IOC
#define MPI3MR_DEFAULT_MDTS	(128 * 1024)
#define MPI3MR_DEFAULT_PGSZEXP         (12)
#define MPI3MR_DEV_RMHS_RETRY_COUNT 3
#define MPI3MR_PEL_RETRY_COUNT 3
#define MPI3MR_DEFAULT_SDEV_QD	32
#define MPI3MR_IRQ_POLL_SLEEP			2
#define MPI3MR_IRQ_POLL_TRIGGER_IOCOUNT		8
#define MPI3MR_CTLR_SECURITY_STATUS_MASK	0x0C
#define MPI3MR_CTLR_SECURE_DBG_STATUS_MASK	0x02
#define MPI3MR_INVALID_DEVICE			0x00
#define MPI3MR_CONFIG_SECURE_DEVICE		0x04
#define MPI3MR_HARD_SECURE_DEVICE		0x08
#define MPI3MR_TAMPERED_DEVICE			0x0C
#define MPI3MR_SGEFLAGS_SYSTEM_SIMPLE_END_OF_LIST \
	(MPI3_SGE_FLAGS_ELEMENT_TYPE_SIMPLE | MPI3_SGE_FLAGS_DLAS_SYSTEM | \
	MPI3_SGE_FLAGS_END_OF_LIST)
#define REPLY_QUEUE_IDX_TO_MSIX_IDX(qidx, offset)	(qidx + offset)
#define MPI3MR_MAX_APP_XFER_SIZE	(1 * 1024 * 1024)
#define MPI3MR_MAX_APP_XFER_SEGMENTS	512
#define MPI3MR_MAX_APP_XFER_SECTORS	(2048 + 512)
#define MPI3MR_WRITE_SAME_MAX_LEN_256_BLKS 256
#define MPI3MR_WRITE_SAME_MAX_LEN_2048_BLKS 2048
struct mpi3mr_nvme_pt_sge {
	u64 base_addr;
	u32 length;
	u16 rsvd;
	u8 rsvd1;
	u8 sgl_type;
};
struct mpi3mr_buf_map {
	void *bsg_buf;
	u32 bsg_buf_len;
	void *kern_buf;
	u32 kern_buf_len;
	dma_addr_t kern_buf_dma;
	u8 data_dir;
};
enum mpi3mr_iocstate {
	MRIOC_STATE_READY = 1,
	MRIOC_STATE_RESET,
	MRIOC_STATE_FAULT,
	MRIOC_STATE_BECOMING_READY,
	MRIOC_STATE_RESET_REQUESTED,
	MRIOC_STATE_UNRECOVERABLE,
};
enum mpi3mr_reset_reason {
	MPI3MR_RESET_FROM_BRINGUP = 1,
	MPI3MR_RESET_FROM_FAULT_WATCH = 2,
	MPI3MR_RESET_FROM_APP = 3,
	MPI3MR_RESET_FROM_EH_HOS = 4,
	MPI3MR_RESET_FROM_TM_TIMEOUT = 5,
	MPI3MR_RESET_FROM_APP_TIMEOUT = 6,
	MPI3MR_RESET_FROM_MUR_FAILURE = 7,
	MPI3MR_RESET_FROM_CTLR_CLEANUP = 8,
	MPI3MR_RESET_FROM_CIACTIV_FAULT = 9,
	MPI3MR_RESET_FROM_PE_TIMEOUT = 10,
	MPI3MR_RESET_FROM_TSU_TIMEOUT = 11,
	MPI3MR_RESET_FROM_DELREQQ_TIMEOUT = 12,
	MPI3MR_RESET_FROM_DELREPQ_TIMEOUT = 13,
	MPI3MR_RESET_FROM_CREATEREPQ_TIMEOUT = 14,
	MPI3MR_RESET_FROM_CREATEREQQ_TIMEOUT = 15,
	MPI3MR_RESET_FROM_IOCFACTS_TIMEOUT = 16,
	MPI3MR_RESET_FROM_IOCINIT_TIMEOUT = 17,
	MPI3MR_RESET_FROM_EVTNOTIFY_TIMEOUT = 18,
	MPI3MR_RESET_FROM_EVTACK_TIMEOUT = 19,
	MPI3MR_RESET_FROM_CIACTVRST_TIMER = 20,
	MPI3MR_RESET_FROM_GETPKGVER_TIMEOUT = 21,
	MPI3MR_RESET_FROM_PELABORT_TIMEOUT = 22,
	MPI3MR_RESET_FROM_SYSFS = 23,
	MPI3MR_RESET_FROM_SYSFS_TIMEOUT = 24,
	MPI3MR_RESET_FROM_FIRMWARE = 27,
	MPI3MR_RESET_FROM_CFG_REQ_TIMEOUT = 29,
	MPI3MR_RESET_FROM_SAS_TRANSPORT_TIMEOUT = 30,
};
enum queue_type {
	MPI3MR_DEFAULT_QUEUE = 0,
	MPI3MR_POLL_QUEUE,
};
struct mpi3mr_compimg_ver {
	u16 build_num;
	u16 cust_id;
	u8 ph_minor;
	u8 ph_major;
	u8 gen_minor;
	u8 gen_major;
};
struct mpi3mr_ioc_facts {
	u32 ioc_capabilities;
	struct mpi3mr_compimg_ver fw_ver;
	u32 mpi_version;
	u16 max_reqs;
	u16 product_id;
	u16 op_req_sz;
	u16 reply_sz;
	u16 exceptions;
	u16 max_perids;
	u16 max_pds;
	u16 max_sasexpanders;
	u32 max_data_length;
	u16 max_sasinitiators;
	u16 max_enclosures;
	u16 max_pcie_switches;
	u16 max_nvme;
	u16 max_vds;
	u16 max_hpds;
	u16 max_advhpds;
	u16 max_raid_pds;
	u16 min_devhandle;
	u16 max_devhandle;
	u16 max_op_req_q;
	u16 max_op_reply_q;
	u16 shutdown_timeout;
	u8 ioc_num;
	u8 who_init;
	u16 max_msix_vectors;
	u8 personality;
	u8 dma_mask;
	u8 protocol_flags;
	u8 sge_mod_mask;
	u8 sge_mod_value;
	u8 sge_mod_shift;
	u8 max_dev_per_tg;
	u16 max_io_throttle_group;
	u16 io_throttle_data_length;
	u16 io_throttle_low;
	u16 io_throttle_high;
};
struct segments {
	void *segment;
	dma_addr_t segment_dma;
};
struct op_req_qinfo {
	u16 ci;
	u16 pi;
	u16 num_requests;
	u16 qid;
	u16 reply_qid;
	u16 num_segments;
	u16 segment_qd;
	spinlock_t q_lock;
	struct segments *q_segments;
	void *q_segment_list;
	dma_addr_t q_segment_list_dma;
};
struct op_reply_qinfo {
	u16 ci;
	u16 qid;
	u16 num_replies;
	u16 num_segments;
	u16 segment_qd;
	struct segments *q_segments;
	void *q_segment_list;
	dma_addr_t q_segment_list_dma;
	u8 ephase;
	atomic_t pend_ios;
	bool enable_irq_poll;
	atomic_t in_use;
	enum queue_type qtype;
};
struct mpi3mr_intr_info {
	struct mpi3mr_ioc *mrioc;
	int os_irq;
	u16 msix_index;
	struct op_reply_qinfo *op_reply_q;
	char name[MPI3MR_NAME_LENGTH];
};
struct mpi3mr_throttle_group_info {
	u8 io_divert;
	u8 need_qd_reduction;
	u8 qd_reduction;
	u16 fw_qd;
	u16 modified_qd;
	u16 id;
	u32 high;
	u32 low;
	atomic_t pend_large_data_sz;
};
#define MPI3MR_HBA_PORT_FLAG_DIRTY	0x01
struct mpi3mr_hba_port {
	struct list_head list;
	u8 port_id;
	u8 flags;
};
struct mpi3mr_sas_port {
	struct list_head port_list;
	u8 num_phys;
	u8 marked_responding;
	int lowest_phy;
	u32 phy_mask;
	struct mpi3mr_hba_port *hba_port;
	struct sas_identify remote_identify;
	struct sas_rphy *rphy;
	struct sas_port *port;
	struct list_head phy_list;
};
struct mpi3mr_sas_phy {
	struct list_head port_siblings;
	struct sas_identify identify;
	struct sas_identify remote_identify;
	struct sas_phy *phy;
	u8 phy_id;
	u16 handle;
	u16 attached_handle;
	u8 phy_belongs_to_port;
	struct mpi3mr_hba_port *hba_port;
};
struct mpi3mr_sas_node {
	struct list_head list;
	struct device *parent_dev;
	u8 num_phys;
	u64 sas_address;
	u16 handle;
	u64 sas_address_parent;
	u16 enclosure_handle;
	u64 enclosure_logical_id;
	u8 non_responding;
	u8 host_node;
	struct mpi3mr_hba_port *hba_port;
	struct mpi3mr_sas_phy *phy;
	struct list_head sas_port_list;
	struct sas_rphy *rphy;
};
struct mpi3mr_enclosure_node {
	struct list_head list;
	struct mpi3_enclosure_page0 pg0;
};
struct tgt_dev_sas_sata {
	u64 sas_address;
	u64 sas_address_parent;
	u16 dev_info;
	u8 phy_id;
	u8 attached_phy_id;
	u8 sas_transport_attached;
	u8 pend_sas_rphy_add;
	struct mpi3mr_hba_port *hba_port;
	struct sas_rphy *rphy;
};
struct tgt_dev_pcie {
	u32 mdts;
	u16 capb;
	u8 pgsz;
	u8 abort_to;
	u8 reset_to;
	u16 dev_info;
};
struct tgt_dev_vd {
	u8 state;
	u8 tg_qd_reduction;
	u16 tg_id;
	u32 tg_high;
	u32 tg_low;
	struct mpi3mr_throttle_group_info *tg;
};
union _form_spec_inf {
	struct tgt_dev_sas_sata sas_sata_inf;
	struct tgt_dev_pcie pcie_inf;
	struct tgt_dev_vd vd_inf;
};
enum mpi3mr_dev_state {
	MPI3MR_DEV_CREATED = 1,
	MPI3MR_DEV_REMOVE_HS_STARTED = 2,
	MPI3MR_DEV_DELETED = 3,
};
struct mpi3mr_tgt_dev {
	struct list_head list;
	struct scsi_target *starget;
	u16 dev_handle;
	u16 parent_handle;
	u16 slot;
	u16 encl_handle;
	u16 perst_id;
	u16 devpg0_flag;
	u8 dev_type;
	u8 is_hidden;
	u8 host_exposed;
	u8 io_unit_port;
	u8 non_stl;
	u8 io_throttle_enabled;
	u16 wslen;
	u16 q_depth;
	u64 wwid;
	u64 enclosure_logical_id;
	union _form_spec_inf dev_spec;
	struct kref ref_count;
	enum mpi3mr_dev_state state;
};
static inline void mpi3mr_tgtdev_get(struct mpi3mr_tgt_dev *s)
{
	kref_get(&s->ref_count);
}
static inline void mpi3mr_free_tgtdev(struct kref *r)
{
	kfree(container_of(r, struct mpi3mr_tgt_dev, ref_count));
}
static inline void mpi3mr_tgtdev_put(struct mpi3mr_tgt_dev *s)
{
	kref_put(&s->ref_count, mpi3mr_free_tgtdev);
}
struct mpi3mr_stgt_priv_data {
	struct scsi_target *starget;
	u16 dev_handle;
	u16 perst_id;
	u32 num_luns;
	atomic_t block_io;
	u8 dev_removed;
	u8 dev_removedelay;
	u8 dev_type;
	u8 dev_nvme_dif;
	u16 wslen;
	u8 io_throttle_enabled;
	u8 io_divert;
	struct mpi3mr_throttle_group_info *throttle_group;
	struct mpi3mr_tgt_dev *tgt_dev;
	u32 pend_count;
};
struct mpi3mr_sdev_priv_data {
	struct mpi3mr_stgt_priv_data *tgt_priv_data;
	u32 lun_id;
	u8 ncq_prio_enable;
	u32 pend_count;
	u16 wslen;
};
struct mpi3mr_drv_cmd {
	struct mutex mutex;
	struct completion done;
	void *reply;
	u8 *sensebuf;
	u8 iou_rc;
	u16 state;
	u16 dev_handle;
	u16 ioc_status;
	u32 ioc_loginfo;
	u8 is_waiting;
	u8 is_sense;
	u8 retry_count;
	u16 host_tag;
	void (*callback)(struct mpi3mr_ioc *mrioc,
	    struct mpi3mr_drv_cmd *drv_cmd);
};
struct dma_memory_desc {
	u32 size;
	void *addr;
	dma_addr_t dma_addr;
};
struct chain_element {
	void *addr;
	dma_addr_t dma_addr;
};
struct scmd_priv {
	u16 host_tag;
	u8 in_lld_scope;
	u8 meta_sg_valid;
	struct scsi_cmnd *scmd;
	u16 req_q_idx;
	int chain_idx;
	int meta_chain_idx;
	u8 mpi3mr_scsiio_req[MPI3MR_ADMIN_REQ_FRAME_SZ];
};
struct mpi3mr_ioc {
	struct list_head list;
	struct pci_dev *pdev;
	struct Scsi_Host *shost;
	u8 id;
	int cpu_count;
	bool enable_segqueue;
	u32 irqpoll_sleep;
	char name[MPI3MR_NAME_LENGTH];
	char driver_name[MPI3MR_NAME_LENGTH];
	volatile struct mpi3_sysif_registers __iomem *sysif_regs;
	resource_size_t sysif_regs_phys;
	int bars;
	u64 dma_mask;
	u16 msix_count;
	u8 intr_enabled;
	u16 num_admin_req;
	u32 admin_req_q_sz;
	u16 admin_req_pi;
	u16 admin_req_ci;
	void *admin_req_base;
	dma_addr_t admin_req_dma;
	spinlock_t admin_req_lock;
	u16 num_admin_replies;
	u32 admin_reply_q_sz;
	u16 admin_reply_ci;
	u8 admin_reply_ephase;
	void *admin_reply_base;
	dma_addr_t admin_reply_dma;
	atomic_t admin_reply_q_in_use;
	u32 ready_timeout;
	struct mpi3mr_intr_info *intr_info;
	u16 intr_info_count;
	bool is_intr_info_set;
	u16 num_queues;
	u16 num_op_req_q;
	struct op_req_qinfo *req_qinfo;
	u16 num_op_reply_q;
	struct op_reply_qinfo *op_reply_qinfo;
	struct mpi3mr_drv_cmd init_cmds;
	struct mpi3mr_drv_cmd cfg_cmds;
	struct mpi3mr_ioc_facts facts;
	u16 op_reply_desc_sz;
	u32 num_reply_bufs;
	struct dma_pool *reply_buf_pool;
	u8 *reply_buf;
	dma_addr_t reply_buf_dma;
	dma_addr_t reply_buf_dma_max_address;
	u16 reply_free_qsz;
	u16 reply_sz;
	struct dma_pool *reply_free_q_pool;
	__le64 *reply_free_q;
	dma_addr_t reply_free_q_dma;
	spinlock_t reply_free_queue_lock;
	u32 reply_free_queue_host_index;
	u32 num_sense_bufs;
	struct dma_pool *sense_buf_pool;
	u8 *sense_buf;
	dma_addr_t sense_buf_dma;
	u16 sense_buf_q_sz;
	struct dma_pool *sense_buf_q_pool;
	__le64 *sense_buf_q;
	dma_addr_t sense_buf_q_dma;
	spinlock_t sbq_lock;
	u32 sbq_host_index;
	u32 event_masks[MPI3_EVENT_NOTIFY_EVENTMASK_WORDS];
	char fwevt_worker_name[MPI3MR_NAME_LENGTH];
	struct workqueue_struct	*fwevt_worker_thread;
	spinlock_t fwevt_lock;
	struct list_head fwevt_list;
	char watchdog_work_q_name[20];
	struct workqueue_struct *watchdog_work_q;
	struct delayed_work watchdog_work;
	spinlock_t watchdog_lock;
	u8 is_driver_loading;
	u8 scan_started;
	u16 scan_failed;
	u8 stop_drv_processing;
	u8 device_refresh_on;
	u16 max_host_ios;
	spinlock_t tgtdev_lock;
	struct list_head tgtdev_list;
	u16 max_sgl_entries;
	u32 chain_buf_count;
	struct dma_pool *chain_buf_pool;
	struct chain_element *chain_sgl_list;
	unsigned long *chain_bitmap;
	spinlock_t chain_buf_lock;
	struct mpi3mr_drv_cmd bsg_cmds;
	struct mpi3mr_drv_cmd host_tm_cmds;
	struct mpi3mr_drv_cmd dev_rmhs_cmds[MPI3MR_NUM_DEVRMCMD];
	struct mpi3mr_drv_cmd evtack_cmds[MPI3MR_NUM_EVTACKCMD];
	unsigned long *devrem_bitmap;
	u16 dev_handle_bitmap_bits;
	unsigned long *removepend_bitmap;
	struct list_head delayed_rmhs_list;
	unsigned long *evtack_cmds_bitmap;
	struct list_head delayed_evtack_cmds_list;
	u32 ts_update_counter;
	u8 reset_in_progress;
	u8 unrecoverable;
	int prev_reset_result;
	struct mutex reset_mutex;
	wait_queue_head_t reset_waitq;
	u8 prepare_for_reset;
	u16 prepare_for_reset_timeout_counter;
	void *prp_list_virt;
	dma_addr_t prp_list_dma;
	u32 prp_sz;
	u16 diagsave_timeout;
	int logging_level;
	u16 flush_io_count;
	struct mpi3mr_fwevt *current_event;
	struct mpi3_driver_info_layout driver_info;
	u16 change_count;
	u8 pel_enabled;
	u8 pel_abort_requested;
	u8 pel_class;
	u16 pel_locale;
	struct mpi3mr_drv_cmd pel_cmds;
	struct mpi3mr_drv_cmd pel_abort_cmd;
	u32 pel_newest_seqnum;
	void *pel_seqnum_virt;
	dma_addr_t pel_seqnum_dma;
	u32 pel_seqnum_sz;
	u16 op_reply_q_offset;
	u16 default_qcount;
	u16 active_poll_qcount;
	u16 requested_poll_qcount;
	struct device bsg_dev;
	struct request_queue *bsg_queue;
	u8 stop_bsgs;
	u8 *logdata_buf;
	u16 logdata_buf_idx;
	u16 logdata_entry_sz;
	atomic_t pend_large_data_sz;
	u32 io_throttle_data_length;
	u32 io_throttle_high;
	u32 io_throttle_low;
	u16 num_io_throttle_group;
	struct mpi3mr_throttle_group_info *throttle_groups;
	void *cfg_page;
	dma_addr_t cfg_page_dma;
	u16 cfg_page_sz;
	u8 sas_transport_enabled;
	u8 scsi_device_channel;
	struct mpi3mr_drv_cmd transport_cmds;
	struct mpi3mr_sas_node sas_hba;
	struct list_head sas_expander_list;
	spinlock_t sas_node_lock;
	struct list_head hba_port_table_list;
	struct list_head enclosure_list;
};
struct mpi3mr_fwevt {
	struct list_head list;
	struct work_struct work;
	struct mpi3mr_ioc *mrioc;
	u16 event_id;
	bool send_ack;
	bool process_evt;
	u32 evt_ctx;
	u16 event_data_size;
	bool pending_at_sml;
	bool discard;
	struct kref ref_count;
	char event_data[] __aligned(4);
};
struct delayed_dev_rmhs_node {
	struct list_head list;
	u16 handle;
	u8 iou_rc;
};
struct delayed_evt_ack_node {
	struct list_head list;
	u8 event;
	u32 event_ctx;
};
int mpi3mr_setup_resources(struct mpi3mr_ioc *mrioc);
void mpi3mr_cleanup_resources(struct mpi3mr_ioc *mrioc);
int mpi3mr_init_ioc(struct mpi3mr_ioc *mrioc);
int mpi3mr_reinit_ioc(struct mpi3mr_ioc *mrioc, u8 is_resume);
void mpi3mr_cleanup_ioc(struct mpi3mr_ioc *mrioc);
int mpi3mr_issue_port_enable(struct mpi3mr_ioc *mrioc, u8 async);
int mpi3mr_admin_request_post(struct mpi3mr_ioc *mrioc, void *admin_req,
u16 admin_req_sz, u8 ignore_reset);
int mpi3mr_op_request_post(struct mpi3mr_ioc *mrioc,
			   struct op_req_qinfo *opreqq, u8 *req);
void mpi3mr_add_sg_single(void *paddr, u8 flags, u32 length,
			  dma_addr_t dma_addr);
void mpi3mr_build_zero_len_sge(void *paddr);
void *mpi3mr_get_sensebuf_virt_addr(struct mpi3mr_ioc *mrioc,
				     dma_addr_t phys_addr);
void *mpi3mr_get_reply_virt_addr(struct mpi3mr_ioc *mrioc,
				     dma_addr_t phys_addr);
void mpi3mr_repost_sense_buf(struct mpi3mr_ioc *mrioc,
				     u64 sense_buf_dma);
void mpi3mr_memset_buffers(struct mpi3mr_ioc *mrioc);
void mpi3mr_free_mem(struct mpi3mr_ioc *mrioc);
void mpi3mr_os_handle_events(struct mpi3mr_ioc *mrioc,
			     struct mpi3_event_notification_reply *event_reply);
void mpi3mr_process_op_reply_desc(struct mpi3mr_ioc *mrioc,
				  struct mpi3_default_reply_descriptor *reply_desc,
				  u64 *reply_dma, u16 qidx);
void mpi3mr_start_watchdog(struct mpi3mr_ioc *mrioc);
void mpi3mr_stop_watchdog(struct mpi3mr_ioc *mrioc);
int mpi3mr_soft_reset_handler(struct mpi3mr_ioc *mrioc,
			      u32 reset_reason, u8 snapdump);
void mpi3mr_ioc_disable_intr(struct mpi3mr_ioc *mrioc);
void mpi3mr_ioc_enable_intr(struct mpi3mr_ioc *mrioc);
enum mpi3mr_iocstate mpi3mr_get_iocstate(struct mpi3mr_ioc *mrioc);
int mpi3mr_process_event_ack(struct mpi3mr_ioc *mrioc, u8 event,
			  u32 event_ctx);
void mpi3mr_wait_for_host_io(struct mpi3mr_ioc *mrioc, u32 timeout);
void mpi3mr_cleanup_fwevt_list(struct mpi3mr_ioc *mrioc);
void mpi3mr_flush_host_io(struct mpi3mr_ioc *mrioc);
void mpi3mr_invalidate_devhandles(struct mpi3mr_ioc *mrioc);
void mpi3mr_rfresh_tgtdevs(struct mpi3mr_ioc *mrioc);
void mpi3mr_flush_delayed_cmd_lists(struct mpi3mr_ioc *mrioc);
void mpi3mr_check_rh_fault_ioc(struct mpi3mr_ioc *mrioc, u32 reason_code);
void mpi3mr_print_fault_info(struct mpi3mr_ioc *mrioc);
void mpi3mr_check_rh_fault_ioc(struct mpi3mr_ioc *mrioc, u32 reason_code);
int mpi3mr_process_op_reply_q(struct mpi3mr_ioc *mrioc,
	struct op_reply_qinfo *op_reply_q);
int mpi3mr_blk_mq_poll(struct Scsi_Host *shost, unsigned int queue_num);
void mpi3mr_bsg_init(struct mpi3mr_ioc *mrioc);
void mpi3mr_bsg_exit(struct mpi3mr_ioc *mrioc);
int mpi3mr_issue_tm(struct mpi3mr_ioc *mrioc, u8 tm_type,
	u16 handle, uint lun, u16 htag, ulong timeout,
	struct mpi3mr_drv_cmd *drv_cmd,
	u8 *resp_code, struct scsi_cmnd *scmd);
struct mpi3mr_tgt_dev *mpi3mr_get_tgtdev_by_handle(
	struct mpi3mr_ioc *mrioc, u16 handle);
void mpi3mr_pel_get_seqnum_complete(struct mpi3mr_ioc *mrioc,
	struct mpi3mr_drv_cmd *drv_cmd);
int mpi3mr_pel_get_seqnum_post(struct mpi3mr_ioc *mrioc,
	struct mpi3mr_drv_cmd *drv_cmd);
void mpi3mr_app_save_logdata(struct mpi3mr_ioc *mrioc, char *event_data,
	u16 event_data_size);
struct mpi3mr_enclosure_node *mpi3mr_enclosure_find_by_handle(
	struct mpi3mr_ioc *mrioc, u16 handle);
extern const struct attribute_group *mpi3mr_host_groups[];
extern const struct attribute_group *mpi3mr_dev_groups[];
extern struct sas_function_template mpi3mr_transport_functions;
extern struct scsi_transport_template *mpi3mr_transport_template;
int mpi3mr_cfg_get_dev_pg0(struct mpi3mr_ioc *mrioc, u16 *ioc_status,
	struct mpi3_device_page0 *dev_pg0, u16 pg_sz, u32 form, u32 form_spec);
int mpi3mr_cfg_get_sas_phy_pg0(struct mpi3mr_ioc *mrioc, u16 *ioc_status,
	struct mpi3_sas_phy_page0 *phy_pg0, u16 pg_sz, u32 form,
	u32 form_spec);
int mpi3mr_cfg_get_sas_phy_pg1(struct mpi3mr_ioc *mrioc, u16 *ioc_status,
	struct mpi3_sas_phy_page1 *phy_pg1, u16 pg_sz, u32 form,
	u32 form_spec);
int mpi3mr_cfg_get_sas_exp_pg0(struct mpi3mr_ioc *mrioc, u16 *ioc_status,
	struct mpi3_sas_expander_page0 *exp_pg0, u16 pg_sz, u32 form,
	u32 form_spec);
int mpi3mr_cfg_get_sas_exp_pg1(struct mpi3mr_ioc *mrioc, u16 *ioc_status,
	struct mpi3_sas_expander_page1 *exp_pg1, u16 pg_sz, u32 form,
	u32 form_spec);
int mpi3mr_cfg_get_enclosure_pg0(struct mpi3mr_ioc *mrioc, u16 *ioc_status,
	struct mpi3_enclosure_page0 *encl_pg0, u16 pg_sz, u32 form,
	u32 form_spec);
int mpi3mr_cfg_get_sas_io_unit_pg0(struct mpi3mr_ioc *mrioc,
	struct mpi3_sas_io_unit_page0 *sas_io_unit_pg0, u16 pg_sz);
int mpi3mr_cfg_get_sas_io_unit_pg1(struct mpi3mr_ioc *mrioc,
	struct mpi3_sas_io_unit_page1 *sas_io_unit_pg1, u16 pg_sz);
int mpi3mr_cfg_set_sas_io_unit_pg1(struct mpi3mr_ioc *mrioc,
	struct mpi3_sas_io_unit_page1 *sas_io_unit_pg1, u16 pg_sz);
int mpi3mr_cfg_get_driver_pg1(struct mpi3mr_ioc *mrioc,
	struct mpi3_driver_page1 *driver_pg1, u16 pg_sz);
u8 mpi3mr_is_expander_device(u16 device_info);
int mpi3mr_expander_add(struct mpi3mr_ioc *mrioc, u16 handle);
void mpi3mr_expander_remove(struct mpi3mr_ioc *mrioc, u64 sas_address,
	struct mpi3mr_hba_port *hba_port);
struct mpi3mr_sas_node *__mpi3mr_expander_find_by_handle(struct mpi3mr_ioc
	*mrioc, u16 handle);
struct mpi3mr_hba_port *mpi3mr_get_hba_port_by_id(struct mpi3mr_ioc *mrioc,
	u8 port_id);
void mpi3mr_sas_host_refresh(struct mpi3mr_ioc *mrioc);
void mpi3mr_sas_host_add(struct mpi3mr_ioc *mrioc);
void mpi3mr_update_links(struct mpi3mr_ioc *mrioc,
	u64 sas_address_parent, u16 handle, u8 phy_number, u8 link_rate,
	struct mpi3mr_hba_port *hba_port);
void mpi3mr_remove_tgtdev_from_host(struct mpi3mr_ioc *mrioc,
	struct mpi3mr_tgt_dev *tgtdev);
int mpi3mr_report_tgtdev_to_sas_transport(struct mpi3mr_ioc *mrioc,
	struct mpi3mr_tgt_dev *tgtdev);
void mpi3mr_remove_tgtdev_from_sas_transport(struct mpi3mr_ioc *mrioc,
	struct mpi3mr_tgt_dev *tgtdev);
struct mpi3mr_tgt_dev *__mpi3mr_get_tgtdev_by_addr_and_rphy(
	struct mpi3mr_ioc *mrioc, u64 sas_address, struct sas_rphy *rphy);
void mpi3mr_print_device_event_notice(struct mpi3mr_ioc *mrioc,
	bool device_add);
void mpi3mr_refresh_sas_ports(struct mpi3mr_ioc *mrioc);
void mpi3mr_refresh_expanders(struct mpi3mr_ioc *mrioc);
void mpi3mr_add_event_wait_for_device_refresh(struct mpi3mr_ioc *mrioc);
void mpi3mr_flush_drv_cmds(struct mpi3mr_ioc *mrioc);
void mpi3mr_flush_cmds_for_unrecovered_controller(struct mpi3mr_ioc *mrioc);
void mpi3mr_free_enclosure_list(struct mpi3mr_ioc *mrioc);
int mpi3mr_process_admin_reply_q(struct mpi3mr_ioc *mrioc);
void mpi3mr_expander_node_remove(struct mpi3mr_ioc *mrioc,
	struct mpi3mr_sas_node *sas_expander);
#endif  
