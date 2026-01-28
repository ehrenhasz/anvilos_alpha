#include <scsi/scsi_host.h>
#include <linux/hashtable.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SCSI_LPFC_DEBUG_FS)
#define CONFIG_SCSI_LPFC_DEBUG_FS
#endif
struct lpfc_sli2_slim;
#define ELX_MODEL_NAME_SIZE	80
#define LPFC_PCI_DEV_LP		0x1
#define LPFC_PCI_DEV_OC		0x2
#define LPFC_SLI_REV2		2
#define LPFC_SLI_REV3		3
#define LPFC_SLI_REV4		4
#define LPFC_MAX_TARGET		4096	 
#define LPFC_MAX_DISC_THREADS	64	 
#define LPFC_MAX_NS_RETRY	3	 
#define LPFC_CMD_PER_LUN	3	 
#define LPFC_DEFAULT_SG_SEG_CNT 64	 
#define LPFC_DEFAULT_XPSGL_SIZE	256
#define LPFC_MAX_SG_TABLESIZE	0xffff
#define LPFC_MIN_SG_SLI4_BUF_SZ	0x800	 
#define LPFC_MAX_BG_SLI4_SEG_CNT_DIF 128  
#define LPFC_MAX_SG_SEG_CNT_DIF 512	 
#define LPFC_MAX_SG_SEG_CNT	4096	 
#define LPFC_MIN_SG_SEG_CNT	32	 
#define LPFC_MAX_SGL_SEG_CNT	512	 
#define LPFC_MAX_BPL_SEG_CNT	4096	 
#define LPFC_MAX_NVME_SEG_CNT	256	 
#define LPFC_MAX_SGE_SIZE       0x80000000  
#define LPFC_IOCB_LIST_CNT	2250	 
#define LPFC_Q_RAMP_UP_INTERVAL 120      
#define LPFC_VNAME_LEN		100	 
#define LPFC_TGTQ_RAMPUP_PCENT	5	 
#define LPFC_MIN_TGT_QDEPTH	10
#define LPFC_MAX_TGT_QDEPTH	0xFFFF
#define QUEUE_RAMP_DOWN_INTERVAL	(msecs_to_jiffies(1000 * 1))
#define LPFC_DISC_IOCB_BUFF_COUNT 20
#define LPFC_HB_MBOX_INTERVAL   5	 
#define LPFC_HB_MBOX_TIMEOUT    30	 
#define LPFC_ERATT_POLL_INTERVAL	5  
#define putPaddrLow(addr)    ((uint32_t) (0xffffffff & (u64)(addr)))
#define putPaddrHigh(addr)   ((uint32_t) (0xffffffff & (((u64)(addr))>>32)))
#define getPaddr(high, low)  ((dma_addr_t)( \
			     (( (u64)(high)<<16 ) << 16)|( (u64)(low))))
#define LPFC_DRVR_TIMEOUT	16	 
#define FC_MAX_ADPTMSG		64
#define MAX_HBAEVT	32
#define MAX_HBAS_NO_RESET 16
#define LPFC_MSIX_VECTORS	2
#define LPFC_DATA_READY		0	 
#define LPFC_LBUF_SZ		128
#define LPFC_MBX_NO_WAIT	0
#define LPFC_MBX_WAIT		1
#define LPFC_CFG_PARAM_MAGIC_NUM 0xFEAA0005
#define LPFC_PORT_CFG_NAME "/cfg/port.cfg"
#define lpfc_rangecheck(val, min, max) \
	((uint)(val) >= (uint)(min) && (val) <= (max))
enum lpfc_polling_flags {
	ENABLE_FCP_RING_POLLING = 0x1,
	DISABLE_FCP_RING_INT    = 0x2
};
struct perf_prof {
	uint16_t cmd_cpu[40];
	uint16_t rsp_cpu[40];
	uint16_t qh_cpu[40];
	uint16_t wqidx[40];
};
#define LPFC_FC4_TYPE_BITMASK	0x00000100
struct lpfc_dmabuf {
	struct list_head list;
	void *virt;		 
	dma_addr_t phys;	 
	uint32_t   buffer_tag;	 
};
struct lpfc_nvmet_ctxbuf {
	struct list_head list;
	struct lpfc_async_xchg_ctx *context;
	struct lpfc_iocbq *iocbq;
	struct lpfc_sglq *sglq;
	struct work_struct defer_work;
};
struct lpfc_dma_pool {
	struct lpfc_dmabuf   *elements;
	uint32_t    max_count;
	uint32_t    current_count;
};
struct hbq_dmabuf {
	struct lpfc_dmabuf hbuf;
	struct lpfc_dmabuf dbuf;
	uint16_t total_size;
	uint16_t bytes_recv;
	uint32_t tag;
	struct lpfc_cq_event cq_event;
	unsigned long time_stamp;
	void *context;
};
struct rqb_dmabuf {
	struct lpfc_dmabuf hbuf;
	struct lpfc_dmabuf dbuf;
	uint16_t total_size;
	uint16_t bytes_recv;
	uint16_t idx;
	struct lpfc_queue *hrq;	   
	struct lpfc_queue *drq;	   
};
#define MEM_PRI		0x100
typedef struct lpfc_vpd {
	uint32_t status;	 
	uint32_t length;	 
	struct {
		uint32_t rsvd1;	 
		uint32_t biuRev;
		uint32_t smRev;
		uint32_t smFwRev;
		uint32_t endecRev;
		uint16_t rBit;
		uint8_t fcphHigh;
		uint8_t fcphLow;
		uint8_t feaLevelHigh;
		uint8_t feaLevelLow;
		uint32_t postKernRev;
		uint32_t opFwRev;
		uint8_t opFwName[16];
		uint32_t sli1FwRev;
		uint8_t sli1FwName[16];
		uint32_t sli2FwRev;
		uint8_t sli2FwName[16];
	} rev;
	struct {
#ifdef __BIG_ENDIAN_BITFIELD
		uint32_t rsvd3  :20;   
		uint32_t rsvd2	: 3;   
		uint32_t cbg	: 1;   
		uint32_t cmv	: 1;   
		uint32_t ccrp   : 1;   
		uint32_t csah   : 1;   
		uint32_t chbs   : 1;   
		uint32_t cinb   : 1;   
		uint32_t cerbm	: 1;   
		uint32_t cmx	: 1;   
		uint32_t cmr	: 1;   
#else	 
		uint32_t cmr	: 1;   
		uint32_t cmx	: 1;   
		uint32_t cerbm	: 1;   
		uint32_t cinb   : 1;   
		uint32_t chbs   : 1;   
		uint32_t csah   : 1;   
		uint32_t ccrp   : 1;   
		uint32_t cmv	: 1;   
		uint32_t cbg	: 1;   
		uint32_t rsvd2	: 3;   
		uint32_t rsvd3  :20;   
#endif
	} sli3Feat;
} lpfc_vpd_t;
struct lpfc_stats {
	uint32_t elsLogiCol;
	uint32_t elsRetryExceeded;
	uint32_t elsXmitRetry;
	uint32_t elsDelayRetry;
	uint32_t elsRcvDrop;
	uint32_t elsRcvFrame;
	uint32_t elsRcvRSCN;
	uint32_t elsRcvRNID;
	uint32_t elsRcvFARP;
	uint32_t elsRcvFARPR;
	uint32_t elsRcvFLOGI;
	uint32_t elsRcvPLOGI;
	uint32_t elsRcvADISC;
	uint32_t elsRcvPDISC;
	uint32_t elsRcvFAN;
	uint32_t elsRcvLOGO;
	uint32_t elsRcvPRLO;
	uint32_t elsRcvPRLI;
	uint32_t elsRcvLIRR;
	uint32_t elsRcvRLS;
	uint32_t elsRcvRPL;
	uint32_t elsRcvRRQ;
	uint32_t elsRcvRTV;
	uint32_t elsRcvECHO;
	uint32_t elsRcvLCB;
	uint32_t elsRcvRDP;
	uint32_t elsRcvRDF;
	uint32_t elsXmitFLOGI;
	uint32_t elsXmitFDISC;
	uint32_t elsXmitPLOGI;
	uint32_t elsXmitPRLI;
	uint32_t elsXmitADISC;
	uint32_t elsXmitLOGO;
	uint32_t elsXmitSCR;
	uint32_t elsXmitRSCN;
	uint32_t elsXmitRNID;
	uint32_t elsXmitFARP;
	uint32_t elsXmitFARPR;
	uint32_t elsXmitACC;
	uint32_t elsXmitLSRJT;
	uint32_t frameRcvBcast;
	uint32_t frameRcvMulti;
	uint32_t strayXmitCmpl;
	uint32_t frameXmitDelay;
	uint32_t xriCmdCmpl;
	uint32_t xriStatErr;
	uint32_t LinkUp;
	uint32_t LinkDown;
	uint32_t LinkMultiEvent;
	uint32_t NoRcvBuf;
	uint32_t fcpCmd;
	uint32_t fcpCmpl;
	uint32_t fcpRspErr;
	uint32_t fcpRemoteStop;
	uint32_t fcpPortRjt;
	uint32_t fcpPortBusy;
	uint32_t fcpError;
	uint32_t fcpLocalErr;
};
struct lpfc_hba;
#define LPFC_VMID_TIMER   300	 
#define LPFC_MAX_VMID_SIZE      256
union lpfc_vmid_io_tag {
	u32 app_id;	 
	u8 cs_ctl_vmid;	 
};
#define JIFFIES_PER_HR	(HZ * 60 * 60)
struct lpfc_vmid {
	u8 flag;
#define LPFC_VMID_SLOT_FREE     0x0
#define LPFC_VMID_SLOT_USED     0x1
#define LPFC_VMID_REQ_REGISTER  0x2
#define LPFC_VMID_REGISTERED    0x4
#define LPFC_VMID_DE_REGISTER   0x8
	char host_vmid[LPFC_MAX_VMID_SIZE];
	union lpfc_vmid_io_tag un;
	struct hlist_node hnode;
	u64 io_rd_cnt;
	u64 io_wr_cnt;
	u8 vmid_len;
	u8 delete_inactive;  
	u32 hash_index;
	u64 __percpu *last_io_time;
};
#define lpfc_vmid_is_type_priority_tag(vport)\
	(vport->vmid_priority_tagging ? 1 : 0)
#define LPFC_VMID_HASH_SIZE     256
#define LPFC_VMID_HASH_MASK     255
#define LPFC_VMID_HASH_SHIFT    6
struct lpfc_vmid_context {
	struct lpfc_vmid *vmp;
	struct lpfc_nodelist *nlp;
	bool instantiated;
};
struct lpfc_vmid_priority_range {
	u8 low;
	u8 high;
	u8 qos;
};
struct lpfc_vmid_priority_info {
	u32 num_descriptors;
	struct lpfc_vmid_priority_range *vmid_range;
};
#define QFPA_EVEN_ONLY 0x01
#define QFPA_ODD_ONLY  0x02
#define QFPA_EVEN_ODD  0x03
enum discovery_state {
	LPFC_VPORT_UNKNOWN     =  0,     
	LPFC_VPORT_FAILED      =  1,     
	LPFC_LOCAL_CFG_LINK    =  6,     
	LPFC_FLOGI             =  7,     
	LPFC_FDISC             =  8,     
	LPFC_FABRIC_CFG_LINK   =  9,     
	LPFC_NS_REG            =  10,    
	LPFC_NS_QRY            =  11,    
	LPFC_BUILD_DISC_LIST   =  12,    
	LPFC_DISC_AUTH         =  13,    
	LPFC_VPORT_READY       =  32,
};
enum hba_state {
	LPFC_LINK_UNKNOWN    =   0,    
	LPFC_WARM_START      =   1,    
	LPFC_INIT_START      =   2,    
	LPFC_INIT_MBX_CMDS   =   3,    
	LPFC_LINK_DOWN       =   4,    
	LPFC_LINK_UP         =   5,    
	LPFC_CLEAR_LA        =   6,    
	LPFC_HBA_READY       =  32,
	LPFC_HBA_ERROR       =  -1
};
struct lpfc_trunk_link_state {
	enum hba_state state;
	uint8_t fault;
};
struct lpfc_trunk_link  {
	struct lpfc_trunk_link_state link0,
				     link1,
				     link2,
				     link3;
	u32 phy_lnk_speed;
};
struct lpfc_cgn_param {
	uint32_t cgn_param_magic;
	uint8_t  cgn_param_version;	 
	uint8_t  cgn_param_mode;	 
#define LPFC_CFG_OFF		0
#define LPFC_CFG_MANAGED	1
#define LPFC_CFG_MONITOR	2
	uint8_t  cgn_rsvd1;
	uint8_t  cgn_rsvd2;
	uint8_t  cgn_param_level0;
	uint8_t  cgn_param_level1;
	uint8_t  cgn_param_level2;
	uint8_t  byte11;
	uint8_t  byte12;
	uint8_t  byte13;
	uint8_t  byte14;
	uint8_t  byte15;
};
#define LPFC_MAX_CGN_DAYS 10
struct lpfc_cgn_ts {
	uint8_t month;
	uint8_t day;
	uint8_t year;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
};
struct lpfc_cgn_info {
	__le16   cgn_info_size;		 
	uint8_t  cgn_info_version;	 
#define LPFC_CGN_INFO_V1	1
#define LPFC_CGN_INFO_V2	2
#define LPFC_CGN_INFO_V3	3
#define LPFC_CGN_INFO_V4	4
	uint8_t  cgn_info_mode;		 
	uint8_t  cgn_info_detect;
	uint8_t  cgn_info_action;
	uint8_t  cgn_info_level0;
	uint8_t  cgn_info_level1;
	uint8_t  cgn_info_level2;
	struct lpfc_cgn_ts base_time;
	uint8_t  cgn_index_minute;
	uint8_t  cgn_index_hour;
	uint8_t  cgn_index_day;
	__le16   cgn_warn_freq;
	__le16   cgn_alarm_freq;
	__le16   cgn_lunq;
	uint8_t  cgn_pad1[8];
	__le16   cgn_drvr_min[60];
	__le32   cgn_drvr_hr[24];
	__le32   cgn_drvr_day[LPFC_MAX_CGN_DAYS];
	__le16   cgn_warn_min[60];
	__le32   cgn_warn_hr[24];
	__le32   cgn_warn_day[LPFC_MAX_CGN_DAYS];
	__le32   cgn_latency_min[60];
	__le32   cgn_latency_hr[24];
	__le32   cgn_latency_day[LPFC_MAX_CGN_DAYS];
	__le16   cgn_bw_min[60];
	__le16   cgn_bw_hr[24];
	__le16   cgn_bw_day[LPFC_MAX_CGN_DAYS];
	__le16   cgn_alarm_min[60];
	__le32   cgn_alarm_hr[24];
	__le32   cgn_alarm_day[LPFC_MAX_CGN_DAYS];
	struct_group(cgn_stat,
		uint8_t  cgn_stat_npm;		 
		struct lpfc_cgn_ts stat_start;	 
		uint8_t cgn_pad2;
		__le32   cgn_notification;
		__le32   cgn_peer_notification;
		__le32   link_integ_notification;
		__le32   delivery_notification;
		struct lpfc_cgn_ts stat_fpin;	 
		struct lpfc_cgn_ts stat_peer;	 
		struct lpfc_cgn_ts stat_lnk;	 
		struct lpfc_cgn_ts stat_delivery;	 
	);
	__le32   cgn_info_crc;
#define LPFC_CGN_CRC32_MAGIC_NUMBER	0x1EDC6F41
#define LPFC_CGN_CRC32_SEED		0xFFFFFFFF
};
#define LPFC_CGN_INFO_SZ	(sizeof(struct lpfc_cgn_info) -  \
				sizeof(uint32_t))
struct lpfc_cgn_stat {
	atomic64_t total_bytes;
	atomic64_t rcv_bytes;
	atomic64_t rx_latency;
#define LPFC_CGN_NOT_SENT	0xFFFFFFFFFFFFFFFFLL
	atomic_t rx_io_cnt;
};
struct lpfc_cgn_acqe_stat {
	atomic64_t alarm;
	atomic64_t warn;
};
struct lpfc_vport {
	struct lpfc_hba *phba;
	struct list_head listentry;
	uint8_t port_type;
#define LPFC_PHYSICAL_PORT 1
#define LPFC_NPIV_PORT  2
#define LPFC_FABRIC_PORT 3
	enum discovery_state port_state;
	uint16_t vpi;
	uint16_t vfi;
	uint8_t vpi_state;
#define LPFC_VPI_REGISTERED	0x1
	uint32_t fc_flag;	 
#define FC_PT2PT                0x1	  
#define FC_PT2PT_PLOGI          0x2	  
#define FC_DISC_TMO             0x4	  
#define FC_PUBLIC_LOOP          0x8	  
#define FC_LBIT                 0x10	  
#define FC_RSCN_MODE            0x20	  
#define FC_NLP_MORE             0x40	  
#define FC_OFFLINE_MODE         0x80	  
#define FC_FABRIC               0x100	  
#define FC_VPORT_LOGO_RCVD      0x200     
#define FC_RSCN_DISCOVERY       0x400	  
#define FC_LOGO_RCVD_DID_CHNG   0x800     
#define FC_PT2PT_NO_NVME        0x1000    
#define FC_SCSI_SCAN_TMO        0x4000	  
#define FC_ABORT_DISCOVERY      0x8000	  
#define FC_NDISC_ACTIVE         0x10000	  
#define FC_BYPASSED_MODE        0x20000	  
#define FC_VPORT_NEEDS_REG_VPI	0x80000   
#define FC_RSCN_DEFERRED	0x100000  
#define FC_VPORT_NEEDS_INIT_VPI 0x200000  
#define FC_VPORT_CVL_RCVD	0x400000  
#define FC_VFI_REGISTERED	0x800000  
#define FC_FDISC_COMPLETED	0x1000000 
#define FC_DISC_DELAYED		0x2000000 
	uint32_t ct_flags;
#define FC_CT_RFF_ID		0x1	  
#define FC_CT_RNN_ID		0x2	  
#define FC_CT_RSNN_NN		0x4	  
#define FC_CT_RSPN_ID		0x8	  
#define FC_CT_RFT_ID		0x10	  
#define FC_CT_RPRT_DEFER	0x20	  
	struct list_head fc_nodes;
	uint16_t fc_plogi_cnt;
	uint16_t fc_adisc_cnt;
	uint16_t fc_reglogin_cnt;
	uint16_t fc_prli_cnt;
	uint16_t fc_unmap_cnt;
	uint16_t fc_map_cnt;
	uint16_t fc_npr_cnt;
	uint16_t fc_unused_cnt;
	struct serv_parm fc_sparam;	 
	uint32_t fc_myDID;	 
	uint32_t fc_prevDID;	 
	struct lpfc_name fabric_portname;
	struct lpfc_name fabric_nodename;
	int32_t stopped;    
	uint8_t fc_linkspeed;	 
	uint32_t num_disc_nodes;	 
	uint32_t gidft_inp;		 
	uint32_t fc_nlp_cnt;	 
	uint32_t fc_rscn_id_cnt;	 
	uint32_t fc_rscn_flush;		 
	struct lpfc_dmabuf *fc_rscn_id_list[FC_MAX_HOLD_RSCN];
	struct lpfc_name fc_nodename;	 
	struct lpfc_name fc_portname;	 
	struct lpfc_work_evt disc_timeout_evt;
	struct timer_list fc_disctmo;	 
	uint8_t fc_ns_retry;	 
	uint32_t fc_prli_sent;	 
	spinlock_t work_port_lock;
	uint32_t work_port_events;  
#define WORKER_DISC_TMO                0x1	 
#define WORKER_ELS_TMO                 0x2	 
#define WORKER_DELAYED_DISC_TMO        0x8	 
#define WORKER_MBOX_TMO                0x100	 
#define WORKER_HB_TMO                  0x200	 
#define WORKER_FABRIC_BLOCK_TMO        0x400	 
#define WORKER_RAMP_DOWN_QUEUE         0x800	 
#define WORKER_RAMP_UP_QUEUE           0x1000	 
#define WORKER_SERVICE_TXQ             0x2000	 
#define WORKER_CHECK_INACTIVE_VMID     0x4000	 
#define WORKER_CHECK_VMID_ISSUE_QFPA   0x8000	 
	struct timer_list els_tmofunc;
	struct timer_list delayed_disc_tmo;
	uint8_t load_flag;
#define FC_LOADING		0x1	 
#define FC_UNLOADING		0x2	 
#define FC_ALLOW_FDMI		0x4	 
#define FC_ALLOW_VMID		0x8	 
#define FC_DEREGISTER_ALL_APP_ID	0x10	 
	uint32_t cfg_scan_down;
	uint32_t cfg_lun_queue_depth;
	uint32_t cfg_nodev_tmo;
	uint32_t cfg_devloss_tmo;
	uint32_t cfg_restrict_login;
	uint32_t cfg_peer_port_login;
	uint32_t cfg_fcp_class;
	uint32_t cfg_use_adisc;
	uint32_t cfg_discovery_threads;
	uint32_t cfg_log_verbose;
	uint32_t cfg_enable_fc4_type;
	uint32_t cfg_max_luns;
	uint32_t cfg_enable_da_id;
	uint32_t cfg_max_scsicmpl_time;
	uint32_t cfg_tgt_queue_depth;
	uint32_t cfg_first_burst_size;
	uint32_t dev_loss_tmo_changed;
	u8 lpfc_vmid_host_uuid[16];
	u32 max_vmid;	 
	u32 cur_vmid_cnt;	 
#define LPFC_MIN_VMID	4
#define LPFC_MAX_VMID	255
	u32 vmid_inactivity_timeout;	 
	u32 vmid_priority_tagging;
#define LPFC_VMID_PRIO_TAG_DISABLE	0  
#define LPFC_VMID_PRIO_TAG_SUP_TARGETS	1  
#define LPFC_VMID_PRIO_TAG_ALL_TARGETS	2  
	unsigned long *vmid_priority_range;
#define LPFC_VMID_MAX_PRIORITY_RANGE    256
#define LPFC_VMID_PRIORITY_BITMAP_SIZE  32
	u8 vmid_flag;
#define LPFC_VMID_IN_USE		0x1
#define LPFC_VMID_ISSUE_QFPA		0x2
#define LPFC_VMID_QFPA_CMPL		0x4
#define LPFC_VMID_QOS_ENABLED		0x8
#define LPFC_VMID_TIMER_ENBLD		0x10
#define LPFC_VMID_TYPE_PRIO		0x20
	struct fc_qfpa_res *qfpa_res;
	struct fc_vport *fc_vport;
	struct lpfc_vmid *vmid;
	DECLARE_HASHTABLE(hash_table, 8);
	rwlock_t vmid_lock;
	struct lpfc_vmid_priority_info vmid_priority;
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	struct dentry *debug_disc_trc;
	struct dentry *debug_nodelist;
	struct dentry *debug_nvmestat;
	struct dentry *debug_scsistat;
	struct dentry *debug_ioktime;
	struct dentry *debug_hdwqstat;
	struct dentry *vport_debugfs_root;
	struct lpfc_debugfs_trc *disc_trc;
	atomic_t disc_trc_cnt;
#endif
	struct list_head rcv_buffer_list;
	unsigned long rcv_buffer_time_stamp;
	uint32_t vport_flag;
#define STATIC_VPORT		0x1
#define FAWWPN_PARAM_CHG	0x2
	uint16_t fdmi_num_disc;
	uint32_t fdmi_hba_mask;
	uint32_t fdmi_port_mask;
	struct nvme_fc_local_port *localport;
	uint8_t  nvmei_support;  
	uint32_t last_fcp_wqidx;
	uint32_t rcv_flogi_cnt;  
};
struct hbq_s {
	uint16_t entry_count;	   
	uint16_t buffer_count;	   
	uint32_t next_hbqPutIdx;   
	uint32_t hbqPutIdx;	   
	uint32_t local_hbqGetIdx;  
	void    *hbq_virt;	   
	struct list_head hbq_buffer_list;   
	struct hbq_dmabuf *(*hbq_alloc_buffer) (struct lpfc_hba *);
	void               (*hbq_free_buffer) (struct lpfc_hba *,
					       struct hbq_dmabuf *);
};
#define LPFC_ELS_HBQ	0
#define LPFC_MAX_HBQS	1
enum hba_temp_state {
	HBA_NORMAL_TEMP,
	HBA_OVER_TEMP
};
enum intr_type_t {
	NONE = 0,
	INTx,
	MSI,
	MSIX,
};
#define LPFC_CT_CTX_MAX		64
struct unsol_rcv_ct_ctx {
	uint32_t ctxt_id;
	uint32_t SID;
	uint32_t valid;
#define UNSOL_INVALID		0
#define UNSOL_VALID		1
	uint16_t oxid;
	uint16_t rxid;
};
#define LPFC_USER_LINK_SPEED_AUTO	0	 
#define LPFC_USER_LINK_SPEED_1G		1	 
#define LPFC_USER_LINK_SPEED_2G		2	 
#define LPFC_USER_LINK_SPEED_4G		4	 
#define LPFC_USER_LINK_SPEED_8G		8	 
#define LPFC_USER_LINK_SPEED_10G	10	 
#define LPFC_USER_LINK_SPEED_16G	16	 
#define LPFC_USER_LINK_SPEED_32G	32	 
#define LPFC_USER_LINK_SPEED_64G	64	 
#define LPFC_USER_LINK_SPEED_MAX	LPFC_USER_LINK_SPEED_64G
#define LPFC_LINK_SPEED_STRING "0, 1, 2, 4, 8, 10, 16, 32, 64"
enum nemb_type {
	nemb_mse = 1,
	nemb_hbd
};
enum mbox_type {
	mbox_rd = 1,
	mbox_wr
};
enum dma_type {
	dma_mbox = 1,
	dma_ebuf
};
enum sta_type {
	sta_pre_addr = 1,
	sta_pos_addr
};
struct lpfc_mbox_ext_buf_ctx {
	uint32_t state;
#define LPFC_BSG_MBOX_IDLE		0
#define LPFC_BSG_MBOX_HOST              1
#define LPFC_BSG_MBOX_PORT		2
#define LPFC_BSG_MBOX_DONE		3
#define LPFC_BSG_MBOX_ABTS		4
	enum nemb_type nembType;
	enum mbox_type mboxType;
	uint32_t numBuf;
	uint32_t mbxTag;
	uint32_t seqNum;
	struct lpfc_dmabuf *mbx_dmabuf;
	struct list_head ext_dmabuf_list;
};
struct lpfc_epd_pool {
	struct list_head list;
	u32 count;
	spinlock_t lock;	 
};
enum ras_state {
	INACTIVE,
	REG_INPROGRESS,
	ACTIVE
};
struct lpfc_ras_fwlog {
	uint8_t *fwlog_buff;
	uint32_t fw_buffcount;  
#define LPFC_RAS_BUFF_ENTERIES  16       
#define LPFC_RAS_MAX_ENTRY_SIZE (64 * 1024)
#define LPFC_RAS_MIN_BUFF_POST_SIZE (256 * 1024)
#define LPFC_RAS_MAX_BUFF_POST_SIZE (1024 * 1024)
	uint32_t fw_loglevel;  
	struct lpfc_dmabuf lwpd;
	struct list_head fwlog_buff_list;
	bool ras_hwsupport;  
	bool ras_enabled;    
#define LPFC_RAS_DISABLE_LOGGING 0x00
#define LPFC_RAS_ENABLE_LOGGING 0x01
	enum ras_state state;     
};
#define DBG_LOG_STR_SZ 256
#define DBG_LOG_SZ 256
struct dbg_log_ent {
	char log[DBG_LOG_STR_SZ];
	u64     t_ns;
};
enum lpfc_irq_chann_mode {
	NORMAL_MODE,
	NUMA_MODE,
	NHT_MODE,
};
enum lpfc_hba_bit_flags {
	FABRIC_COMANDS_BLOCKED,
	HBA_PCI_ERR,
	MBX_TMO_ERR,
};
struct lpfc_hba {
	struct lpfc_io_buf * (*lpfc_get_scsi_buf)
		(struct lpfc_hba *phba, struct lpfc_nodelist *ndlp,
		struct scsi_cmnd *cmnd);
	int (*lpfc_scsi_prep_dma_buf)
		(struct lpfc_hba *, struct lpfc_io_buf *);
	void (*lpfc_scsi_unprep_dma_buf)
		(struct lpfc_hba *, struct lpfc_io_buf *);
	void (*lpfc_release_scsi_buf)
		(struct lpfc_hba *, struct lpfc_io_buf *);
	void (*lpfc_rampdown_queue_depth)
		(struct lpfc_hba *);
	void (*lpfc_scsi_prep_cmnd)
		(struct lpfc_vport *, struct lpfc_io_buf *,
		 struct lpfc_nodelist *);
	int (*lpfc_scsi_prep_cmnd_buf)
		(struct lpfc_vport *vport,
		 struct lpfc_io_buf *lpfc_cmd,
		 uint8_t tmo);
	int (*lpfc_scsi_prep_task_mgmt_cmd)
		(struct lpfc_vport *vport,
		 struct lpfc_io_buf *lpfc_cmd,
		 u64 lun, u8 task_mgmt_cmd);
	int (*__lpfc_sli_issue_iocb)
		(struct lpfc_hba *, uint32_t,
		 struct lpfc_iocbq *, uint32_t);
	int (*__lpfc_sli_issue_fcp_io)
		(struct lpfc_hba *phba, uint32_t ring_number,
		 struct lpfc_iocbq *piocb, uint32_t flag);
	void (*__lpfc_sli_release_iocbq)(struct lpfc_hba *,
			 struct lpfc_iocbq *);
	int (*lpfc_hba_down_post)(struct lpfc_hba *phba);
	int (*lpfc_sli_issue_mbox)
		(struct lpfc_hba *, LPFC_MBOXQ_t *, uint32_t);
	void (*lpfc_sli_handle_slow_ring_event)
		(struct lpfc_hba *phba, struct lpfc_sli_ring *pring,
		 uint32_t mask);
	int (*lpfc_sli_hbq_to_firmware)
		(struct lpfc_hba *, uint32_t, struct hbq_dmabuf *);
	int (*lpfc_sli_brdrestart)
		(struct lpfc_hba *);
	int (*lpfc_sli_brdready)
		(struct lpfc_hba *, uint32_t);
	void (*lpfc_handle_eratt)
		(struct lpfc_hba *);
	void (*lpfc_stop_port)
		(struct lpfc_hba *);
	int (*lpfc_hba_init_link)
		(struct lpfc_hba *, uint32_t);
	int (*lpfc_hba_down_link)
		(struct lpfc_hba *, uint32_t);
	int (*lpfc_selective_reset)
		(struct lpfc_hba *);
	int (*lpfc_bg_scsi_prep_dma_buf)
		(struct lpfc_hba *, struct lpfc_io_buf *);
	void (*__lpfc_sli_prep_els_req_rsp)(struct lpfc_iocbq *cmdiocbq,
					    struct lpfc_vport *vport,
					    struct lpfc_dmabuf *bmp,
					    u16 cmd_size, u32 did, u32 elscmd,
					    u8 tmo, u8 expect_rsp);
	void (*__lpfc_sli_prep_gen_req)(struct lpfc_iocbq *cmdiocbq,
					struct lpfc_dmabuf *bmp, u16 rpi,
					u32 num_entry, u8 tmo);
	void (*__lpfc_sli_prep_xmit_seq64)(struct lpfc_iocbq *cmdiocbq,
					   struct lpfc_dmabuf *bmp, u16 rpi,
					   u16 ox_id, u32 num_entry, u8 rctl,
					   u8 last_seq, u8 cr_cx_cmd);
	void (*__lpfc_sli_prep_abort_xri)(struct lpfc_iocbq *cmdiocbq,
					  u16 ulp_context, u16 iotag,
					  u8 ulp_class, u16 cqid, bool ia,
					  bool wqec);
	struct lpfc_epd_pool epd_pool;
	struct lpfc_sli4_hba sli4_hba;
	struct workqueue_struct *wq;
	struct delayed_work     eq_delay_work;
#define LPFC_IDLE_STAT_DELAY 1000
	struct delayed_work	idle_stat_delay_work;
	struct lpfc_sli sli;
	uint8_t pci_dev_grp;	 
	uint32_t sli_rev;		 
	uint32_t sli3_options;		 
#define LPFC_SLI3_HBQ_ENABLED		0x01
#define LPFC_SLI3_NPIV_ENABLED		0x02
#define LPFC_SLI3_VPORT_TEARDOWN	0x04
#define LPFC_SLI3_CRP_ENABLED		0x08
#define LPFC_SLI3_BG_ENABLED		0x20
#define LPFC_SLI3_DSS_ENABLED		0x40
#define LPFC_SLI4_PERFH_ENABLED		0x80
#define LPFC_SLI4_PHWQ_ENABLED		0x100
	uint32_t iocb_cmd_size;
	uint32_t iocb_rsp_size;
	struct lpfc_trunk_link  trunk_link;
	enum hba_state link_state;
	uint32_t link_flag;	 
#define LS_LOOPBACK_MODE      0x1	 
#define LS_NPIV_FAB_SUPPORTED 0x2	 
#define LS_IGNORE_ERATT       0x4	 
#define LS_MDS_LINK_DOWN      0x8	 
#define LS_MDS_LOOPBACK       0x10	 
#define LS_CT_VEN_RPA         0x20	 
#define LS_EXTERNAL_LOOPBACK  0x40	 
	uint32_t hba_flag;	 
#define HBA_ERATT_HANDLED	0x1  
#define DEFER_ERATT		0x2  
#define HBA_FCOE_MODE		0x4  
#define HBA_SP_QUEUE_EVT	0x8  
#define HBA_POST_RECEIVE_BUFFER 0x10  
#define HBA_PERSISTENT_TOPO	0x20  
#define ELS_XRI_ABORT_EVENT	0x40  
#define ASYNC_EVENT		0x80
#define LINK_DISABLED		0x100  
#define FCF_TS_INPROG           0x200  
#define FCF_RR_INPROG           0x400  
#define HBA_FIP_SUPPORT		0x800  
#define HBA_DEVLOSS_TMO         0x2000  
#define HBA_RRQ_ACTIVE		0x4000  
#define HBA_IOQ_FLUSH		0x8000  
#define HBA_RECOVERABLE_UE	0x20000  
#define HBA_FORCED_LINK_SPEED	0x40000  
#define HBA_FLOGI_ISSUED	0x100000  
#define HBA_DEFER_FLOGI		0x800000  
#define HBA_SETUP		0x1000000  
#define HBA_NEEDS_CFG_PORT	0x2000000  
#define HBA_HBEAT_INP		0x4000000  
#define HBA_HBEAT_TMO		0x8000000  
#define HBA_FLOGI_OUTSTANDING	0x10000000  
#define HBA_RHBA_CMPL		0x20000000  
	struct completion *fw_dump_cmpl;  
	uint32_t fcp_ring_in_use;  
	struct lpfc_dmabuf slim2p;
	MAILBOX_t *mbox;
	uint32_t *mbox_ext;
	struct lpfc_mbox_ext_buf_ctx mbox_ext_buf_ctx;
	uint32_t ha_copy;
	struct _PCB *pcb;
	struct _IOCB *IOCBs;
	struct lpfc_dmabuf hbqslimp;
	uint16_t pci_cfg_value;
	uint8_t fc_linkspeed;	 
	uint32_t fc_eventTag;	 
	uint32_t link_events;
	uint32_t fc_pref_DID;	 
	uint8_t  fc_pref_ALPA;	 
	uint32_t fc_edtovResol;  
	uint32_t fc_edtov;	 
	uint32_t fc_arbtov;	 
	uint32_t fc_ratov;	 
	uint32_t fc_rttov;	 
	uint32_t fc_altov;	 
	uint32_t fc_crtov;	 
	struct serv_parm fc_fabparam;	 
	uint8_t alpa_map[128];	 
	uint32_t lmt;
	uint32_t fc_topology;	 
	uint32_t fc_topology_changed;	 
	struct lpfc_stats fc_stat;
	struct lpfc_nodelist fc_fcpnodev;  
	uint32_t nport_event_cnt;	 
	uint8_t  wwnn[8];
	uint8_t  wwpn[8];
	uint32_t RandomData[7];
	uint8_t  fcp_embed_io;
	uint8_t  nvmet_support;	 
#define LPFC_NVMET_MAX_PORTS	32
	uint8_t  mds_diags_support;
	uint8_t  bbcredit_support;
	uint8_t  enab_exp_wqcq_pages;
	u8	 nsler;  
	uint32_t cfg_ack0;
	uint32_t cfg_xri_rebalancing;
	uint32_t cfg_xpsgl;
	uint32_t cfg_enable_npiv;
	uint32_t cfg_enable_rrq;
	uint32_t cfg_topology;
	uint32_t cfg_link_speed;
#define LPFC_FCF_FOV 1		 
#define LPFC_FCF_PRIORITY 2	 
	uint32_t cfg_fcf_failover_policy;
	uint32_t cfg_fcp_io_sched;
	uint32_t cfg_ns_query;
	uint32_t cfg_fcp2_no_tgt_reset;
	uint32_t cfg_cr_delay;
	uint32_t cfg_cr_count;
	uint32_t cfg_multi_ring_support;
	uint32_t cfg_multi_ring_rctl;
	uint32_t cfg_multi_ring_type;
	uint32_t cfg_poll;
	uint32_t cfg_poll_tmo;
	uint32_t cfg_task_mgmt_tmo;
	uint32_t cfg_use_msi;
	uint32_t cfg_auto_imax;
	uint32_t cfg_fcp_imax;
	uint32_t cfg_force_rscn;
	uint32_t cfg_cq_poll_threshold;
	uint32_t cfg_cq_max_proc_limit;
	uint32_t cfg_fcp_cpu_map;
	uint32_t cfg_fcp_mq_threshold;
	uint32_t cfg_hdw_queue;
	uint32_t cfg_irq_chann;
	uint32_t cfg_suppress_rsp;
	uint32_t cfg_nvme_oas;
	uint32_t cfg_nvme_embed_cmd;
	uint32_t cfg_nvmet_mrq_post;
	uint32_t cfg_nvmet_mrq;
	uint32_t cfg_enable_nvmet;
	uint32_t cfg_nvme_enable_fb;
	uint32_t cfg_nvmet_fb_size;
	uint32_t cfg_total_seg_cnt;
	uint32_t cfg_sg_seg_cnt;
	uint32_t cfg_nvme_seg_cnt;
	uint32_t cfg_scsi_seg_cnt;
	uint32_t cfg_sg_dma_buf_size;
	uint32_t cfg_hba_queue_depth;
	uint32_t cfg_enable_hba_reset;
	uint32_t cfg_enable_hba_heartbeat;
	uint32_t cfg_fof;
	uint32_t cfg_EnableXLane;
	uint8_t cfg_oas_tgt_wwpn[8];
	uint8_t cfg_oas_vpt_wwpn[8];
	uint32_t cfg_oas_lun_state;
#define OAS_LUN_ENABLE	1
#define OAS_LUN_DISABLE	0
	uint32_t cfg_oas_lun_status;
#define OAS_LUN_STATUS_EXISTS	0x01
	uint32_t cfg_oas_flags;
#define OAS_FIND_ANY_VPORT	0x01
#define OAS_FIND_ANY_TARGET	0x02
#define OAS_LUN_VALID	0x04
	uint32_t cfg_oas_priority;
	uint32_t cfg_XLanePriority;
	uint32_t cfg_enable_bg;
	uint32_t cfg_prot_mask;
	uint32_t cfg_prot_guard;
	uint32_t cfg_hostmem_hgp;
	uint32_t cfg_log_verbose;
	uint32_t cfg_enable_fc4_type;
#define LPFC_ENABLE_FCP  1
#define LPFC_ENABLE_NVME 2
#define LPFC_ENABLE_BOTH 3
#if (IS_ENABLED(CONFIG_NVME_FC))
#define LPFC_MAX_ENBL_FC4_TYPE LPFC_ENABLE_BOTH
#define LPFC_DEF_ENBL_FC4_TYPE LPFC_ENABLE_BOTH
#else
#define LPFC_MAX_ENBL_FC4_TYPE LPFC_ENABLE_FCP
#define LPFC_DEF_ENBL_FC4_TYPE LPFC_ENABLE_FCP
#endif
	uint32_t cfg_sriov_nr_virtfn;
	uint32_t cfg_request_firmware_upgrade;
	uint32_t cfg_suppress_link_up;
	uint32_t cfg_rrq_xri_bitmap_sz;
	u32      cfg_fcp_wait_abts_rsp;
	uint32_t cfg_delay_discovery;
	uint32_t cfg_sli_mode;
#define LPFC_INITIALIZE_LINK              0	 
#define LPFC_DELAY_INIT_LINK              1	 
#define LPFC_DELAY_INIT_LINK_INDEFINITELY 2	 
	uint32_t cfg_fdmi_on;
#define LPFC_FDMI_NO_SUPPORT	0	 
#define LPFC_FDMI_SUPPORT	1	 
	uint32_t cfg_enable_SmartSAN;
	uint32_t cfg_enable_mds_diags;
	uint32_t cfg_ras_fwlog_level;
	uint32_t cfg_ras_fwlog_buffsize;
	uint32_t cfg_ras_fwlog_func;
	uint32_t cfg_enable_bbcr;	 
	uint32_t cfg_enable_dpp;	 
	uint32_t cfg_enable_pbde;
	uint32_t cfg_enable_mi;
	struct nvmet_fc_target_port *targetport;
	lpfc_vpd_t vpd;		 
	u32 cfg_max_vmid;	 
	u32 cfg_vmid_app_header;
#define LPFC_VMID_APP_HEADER_DISABLE	0
#define LPFC_VMID_APP_HEADER_ENABLE	1
	u32 cfg_vmid_priority_tagging;
	u32 cfg_vmid_inactivity_timeout;	 
	struct pci_dev *pcidev;
	struct list_head      work_list;
	uint32_t              work_ha;       
	uint32_t              work_ha_mask;  
	uint32_t              work_hs;       
	uint32_t              work_status[2];  
	wait_queue_head_t    work_waitq;
	struct task_struct   *worker_thread;
	unsigned long data_flags;
	uint32_t border_sge_num;
	uint32_t hbq_in_use;		 
	uint32_t hbq_count;	         
	struct hbq_s hbqs[LPFC_MAX_HBQS];  
	atomic_t fcp_qidx;          
	atomic_t nvme_qidx;         
	phys_addr_t pci_bar0_map;      
	phys_addr_t pci_bar1_map;      
	phys_addr_t pci_bar2_map;      
	void __iomem *slim_memmap_p;	 
	void __iomem *ctrl_regs_memmap_p; 
	void __iomem *pci_bar0_memmap_p;  
	void __iomem *pci_bar2_memmap_p;  
	void __iomem *pci_bar4_memmap_p;  
#define PCI_64BIT_BAR0	0
#define PCI_64BIT_BAR2	2
#define PCI_64BIT_BAR4	4
	void __iomem *MBslimaddr;	 
	void __iomem *HAregaddr;	 
	void __iomem *CAregaddr;	 
	void __iomem *HSregaddr;	 
	void __iomem *HCregaddr;	 
	struct lpfc_hgp __iomem *host_gp;  
	struct lpfc_pgp   *port_gp;
	uint32_t __iomem  *hbq_put;      
	uint32_t          *hbq_get;      
	int brd_no;			 
	char SerialNumber[32];		 
	char OptionROMVersion[32];	 
	char BIOSVersion[16];		 
	char ModelDesc[256];		 
	char ModelName[80];		 
	char ProgramType[256];		 
	char Port[20];			 
	uint8_t vpd_flag;                
#define VPD_MODEL_DESC      0x1          
#define VPD_MODEL_NAME      0x2          
#define VPD_PROGRAM_TYPE    0x4          
#define VPD_PORT            0x8          
#define VPD_MASK            0xf          
	struct timer_list fcp_poll_timer;
	struct timer_list eratt_poll;
	uint32_t eratt_poll_interval;
	uint64_t bg_guard_err_cnt;
	uint64_t bg_apptag_err_cnt;
	uint64_t bg_reftag_err_cnt;
	spinlock_t scsi_buf_list_get_lock;   
	spinlock_t scsi_buf_list_put_lock;   
	struct list_head lpfc_scsi_buf_list_get;
	struct list_head lpfc_scsi_buf_list_put;
	uint32_t total_scsi_bufs;
	struct list_head lpfc_iocb_list;
	uint32_t total_iocbq_bufs;
	struct list_head active_rrq_list;
	spinlock_t hbalock;
	struct work_struct  unblock_request_work;  
	struct dma_pool *lpfc_sg_dma_buf_pool;
	struct dma_pool *lpfc_mbuf_pool;
	struct dma_pool *lpfc_hrb_pool;	 
	struct dma_pool *lpfc_drb_pool;  
	struct dma_pool *lpfc_nvmet_drb_pool;  
	struct dma_pool *lpfc_hbq_pool;	 
	struct dma_pool *lpfc_cmd_rsp_buf_pool;
	struct lpfc_dma_pool lpfc_mbuf_safety_pool;
	mempool_t *mbox_mem_pool;
	mempool_t *nlp_mem_pool;
	mempool_t *rrq_pool;
	mempool_t *active_rrq_pool;
	struct fc_host_statistics link_stats;
	enum lpfc_irq_chann_mode irq_chann_mode;
	enum intr_type_t intr_type;
	uint32_t intr_mode;
#define LPFC_INTR_ERROR	0xFFFFFFFF
	struct list_head port_list;
	spinlock_t port_list_lock;	 
	struct lpfc_vport *pport;	 
	uint16_t max_vpi;		 
#define LPFC_MAX_VPI	0xFF		 
#define LPFC_MAX_VPORTS	0x100		 
	uint16_t max_vports;             
	uint16_t vpi_base;
	uint16_t vfi_base;
	unsigned long *vpi_bmask;	 
	uint16_t *vpi_ids;
	uint16_t vpi_count;
	struct list_head lpfc_vpi_blk_list;
	struct list_head fabric_iocb_list;
	atomic_t fabric_iocb_count;
	struct timer_list fabric_block_timer;
	unsigned long bit_flags;
	atomic_t num_rsrc_err;
	atomic_t num_cmd_success;
	unsigned long last_rsrc_error_time;
	unsigned long last_ramp_down_time;
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	struct dentry *hba_debugfs_root;
	atomic_t debugfs_vport_count;
	struct dentry *debug_multixri_pools;
	struct dentry *debug_hbqinfo;
	struct dentry *debug_dumpHostSlim;
	struct dentry *debug_dumpHBASlim;
	struct dentry *debug_InjErrLBA;   
	struct dentry *debug_InjErrNPortID;   
	struct dentry *debug_InjErrWWPN;   
	struct dentry *debug_writeGuard;  
	struct dentry *debug_writeApp;    
	struct dentry *debug_writeRef;    
	struct dentry *debug_readGuard;   
	struct dentry *debug_readApp;     
	struct dentry *debug_readRef;     
	struct dentry *debug_nvmeio_trc;
	struct lpfc_debugfs_nvmeio_trc *nvmeio_trc;
	struct dentry *debug_hdwqinfo;
#ifdef LPFC_HDWQ_LOCK_STAT
	struct dentry *debug_lockstat;
#endif
	struct dentry *debug_cgn_buffer;
	struct dentry *debug_rx_monitor;
	struct dentry *debug_ras_log;
	atomic_t nvmeio_trc_cnt;
	uint32_t nvmeio_trc_size;
	uint32_t nvmeio_trc_output_idx;
	uint32_t lpfc_injerr_wgrd_cnt;
	uint32_t lpfc_injerr_wapp_cnt;
	uint32_t lpfc_injerr_wref_cnt;
	uint32_t lpfc_injerr_rgrd_cnt;
	uint32_t lpfc_injerr_rapp_cnt;
	uint32_t lpfc_injerr_rref_cnt;
	uint32_t lpfc_injerr_nportid;
	struct lpfc_name lpfc_injerr_wwpn;
	sector_t lpfc_injerr_lba;
#define LPFC_INJERR_LBA_OFF	(sector_t)(-1)
	struct dentry *debug_slow_ring_trc;
	struct lpfc_debugfs_trc *slow_ring_trc;
	atomic_t slow_ring_trc_cnt;
	struct dentry *idiag_root;
	struct dentry *idiag_pci_cfg;
	struct dentry *idiag_bar_acc;
	struct dentry *idiag_que_info;
	struct dentry *idiag_que_acc;
	struct dentry *idiag_drb_acc;
	struct dentry *idiag_ctl_acc;
	struct dentry *idiag_mbx_acc;
	struct dentry *idiag_ext_acc;
	uint8_t lpfc_idiag_last_eq;
#endif
	uint16_t nvmeio_trc_on;
	struct list_head elsbuf;
	int elsbuf_cnt;
	int elsbuf_prev_cnt;
	uint8_t temp_sensor_support;
	unsigned long last_completion_time;
	unsigned long skipped_hb;
	struct timer_list hb_tmofunc;
	struct timer_list rrq_tmr;
	enum hba_temp_state over_temp_state;
#define QUE_BUFTAG_BIT  (1<<31)
	uint32_t buffer_tag_count;
#define LPFC_MAX_EVT_COUNT 512
	atomic_t fast_event_count;
	uint32_t fcoe_eventtag;
	uint32_t fcoe_eventtag_at_fcf_scan;
	uint32_t fcoe_cvl_eventtag;
	uint32_t fcoe_cvl_eventtag_attn;
	struct lpfc_fcf fcf;
	uint8_t fc_map[3];
	uint8_t valid_vlan;
	uint16_t vlan_id;
	struct list_head fcf_conn_rec_list;
	bool defer_flogi_acc_flag;
	uint16_t defer_flogi_acc_rx_id;
	uint16_t defer_flogi_acc_ox_id;
	spinlock_t ct_ev_lock;  
	struct list_head ct_ev_waiters;
	struct unsol_rcv_ct_ctx ct_ctx[LPFC_CT_CTX_MAX];
	uint32_t ctx_idx;
	struct timer_list inactive_vmid_poll;
	struct lpfc_ras_fwlog ras_fwlog;
	uint32_t iocb_cnt;
	uint32_t iocb_max;
	atomic_t sdev_cnt;
	spinlock_t devicelock;	 
	mempool_t *device_data_mem_pool;
	struct list_head luns;
#define LPFC_TRANSGRESSION_HIGH_TEMPERATURE	0x0080
#define LPFC_TRANSGRESSION_LOW_TEMPERATURE	0x0040
#define LPFC_TRANSGRESSION_HIGH_VOLTAGE		0x0020
#define LPFC_TRANSGRESSION_LOW_VOLTAGE		0x0010
#define LPFC_TRANSGRESSION_HIGH_TXBIAS		0x0008
#define LPFC_TRANSGRESSION_LOW_TXBIAS		0x0004
#define LPFC_TRANSGRESSION_HIGH_TXPOWER		0x0002
#define LPFC_TRANSGRESSION_LOW_TXPOWER		0x0001
#define LPFC_TRANSGRESSION_HIGH_RXPOWER		0x8000
#define LPFC_TRANSGRESSION_LOW_RXPOWER		0x4000
	uint16_t sfp_alarm;
	uint16_t sfp_warning;
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	uint16_t hdwqstat_on;
#define LPFC_CHECK_OFF		0
#define LPFC_CHECK_NVME_IO	1
#define LPFC_CHECK_NVMET_IO	2
#define LPFC_CHECK_SCSI_IO	4
	uint16_t ktime_on;
	uint64_t ktime_data_samples;
	uint64_t ktime_status_samples;
	uint64_t ktime_last_cmd;
	uint64_t ktime_seg1_total;
	uint64_t ktime_seg1_min;
	uint64_t ktime_seg1_max;
	uint64_t ktime_seg2_total;
	uint64_t ktime_seg2_min;
	uint64_t ktime_seg2_max;
	uint64_t ktime_seg3_total;
	uint64_t ktime_seg3_min;
	uint64_t ktime_seg3_max;
	uint64_t ktime_seg4_total;
	uint64_t ktime_seg4_min;
	uint64_t ktime_seg4_max;
	uint64_t ktime_seg5_total;
	uint64_t ktime_seg5_min;
	uint64_t ktime_seg5_max;
	uint64_t ktime_seg6_total;
	uint64_t ktime_seg6_min;
	uint64_t ktime_seg6_max;
	uint64_t ktime_seg7_total;
	uint64_t ktime_seg7_min;
	uint64_t ktime_seg7_max;
	uint64_t ktime_seg8_total;
	uint64_t ktime_seg8_min;
	uint64_t ktime_seg8_max;
	uint64_t ktime_seg9_total;
	uint64_t ktime_seg9_min;
	uint64_t ktime_seg9_max;
	uint64_t ktime_seg10_total;
	uint64_t ktime_seg10_min;
	uint64_t ktime_seg10_max;
#endif
	struct lpfc_cgn_stat __percpu *cmf_stat;
	uint32_t cmf_interval_rate;   
	uint32_t cmf_timer_cnt;
#define LPFC_CMF_INTERVAL 90
	uint64_t cmf_link_byte_count;
	uint64_t cmf_max_line_rate;
	uint64_t cmf_max_bytes_per_interval;
	uint64_t cmf_last_sync_bw;
#define  LPFC_CMF_BLK_SIZE 512
	struct hrtimer cmf_timer;
	struct hrtimer cmf_stats_timer;	 
	atomic_t cmf_bw_wait;
	atomic_t cmf_busy;
	atomic_t cmf_stop_io;       
	uint32_t cmf_active_mode;
	uint32_t cmf_info_per_interval;
#define LPFC_MAX_CMF_INFO 32
	struct timespec64 cmf_latency;   
	uint32_t cmf_last_ts;    
	uint32_t cmf_active_info;
	u8 cgn_reg_fpin;            
	u8 cgn_init_reg_fpin;       
#define LPFC_CGN_FPIN_NONE	0x0
#define LPFC_CGN_FPIN_WARN	0x1
#define LPFC_CGN_FPIN_ALARM	0x2
#define LPFC_CGN_FPIN_BOTH	(LPFC_CGN_FPIN_WARN | LPFC_CGN_FPIN_ALARM)
	u8 cgn_reg_signal;           
	u8 cgn_init_reg_signal;      
	u16 cgn_fpin_frequency;		 
#define LPFC_FPIN_INIT_FREQ	0xffff
	u32 cgn_sig_freq;
	u32 cgn_acqe_cnt;
	struct lpfc_rx_info_monitor *rx_monitor;
	atomic_t rx_max_read_cnt;        
	uint64_t rx_block_cnt;
	struct lpfc_cgn_param cgn_p;
	struct lpfc_cgn_acqe_stat cgn_acqe_stat;
	struct lpfc_dmabuf *cgn_i;       
	atomic_t cgn_fabric_warn_cnt;    
	atomic_t cgn_fabric_alarm_cnt;   
	atomic_t cgn_sync_warn_cnt;      
	atomic_t cgn_sync_alarm_cnt;     
	atomic_t cgn_driver_evt_cnt;     
	atomic_t cgn_latency_evt_cnt;
	atomic64_t cgn_latency_evt;      
	unsigned long cgn_evt_timestamp;
#define LPFC_CGN_TIMER_TO_MIN   60000  
	uint32_t cgn_evt_minute;
#define LPFC_SEC_MIN		60UL
#define LPFC_MIN_HOUR		60
#define LPFC_HOUR_DAY		24
#define LPFC_MIN_DAY		(LPFC_MIN_HOUR * LPFC_HOUR_DAY)
	struct hlist_node cpuhp;	 
	struct timer_list cpuhp_poll_timer;
	struct list_head poll_list;	 
#define LPFC_POLL_HB	1		 
	char os_host_name[MAXHOSTNAMELEN];
	u32 degrade_activate_threshold;
	u32 degrade_deactivate_threshold;
	u32 fec_degrade_interval;
	atomic_t dbg_log_idx;
	atomic_t dbg_log_cnt;
	atomic_t dbg_log_dmping;
	struct dbg_log_ent dbg_log[DBG_LOG_SZ];
};
#define LPFC_MAX_RXMONITOR_ENTRY	800
#define LPFC_MAX_RXMONITOR_DUMP		32
struct rx_info_entry {
	uint64_t cmf_bytes;	 
	uint64_t total_bytes;    
	uint64_t rcv_bytes;      
	uint64_t avg_io_size;
	uint64_t avg_io_latency; 
	uint64_t max_read_cnt;   
	uint64_t max_bytes_per_interval;
	uint32_t cmf_busy;
	uint32_t cmf_info;       
	uint32_t io_cnt;
	uint32_t timer_utilization;
	uint32_t timer_interval;
};
struct lpfc_rx_info_monitor {
	struct rx_info_entry *ring;  
	u32 head_idx, tail_idx;  
	spinlock_t lock;  
	u32 entries;  
};
static inline struct Scsi_Host *
lpfc_shost_from_vport(struct lpfc_vport *vport)
{
	return container_of((void *) vport, struct Scsi_Host, hostdata[0]);
}
static inline void
lpfc_set_loopback_flag(struct lpfc_hba *phba)
{
	if (phba->cfg_topology == FLAGS_LOCAL_LB)
		phba->link_flag |= LS_LOOPBACK_MODE;
	else
		phba->link_flag &= ~LS_LOOPBACK_MODE;
}
static inline int
lpfc_is_link_up(struct lpfc_hba *phba)
{
	return  phba->link_state == LPFC_LINK_UP ||
		phba->link_state == LPFC_CLEAR_LA ||
		phba->link_state == LPFC_HBA_READY;
}
static inline void
lpfc_worker_wake_up(struct lpfc_hba *phba)
{
	set_bit(LPFC_DATA_READY, &phba->data_flags);
	wake_up(&phba->work_waitq);
	return;
}
static inline int
lpfc_readl(void __iomem *addr, uint32_t *data)
{
	uint32_t temp;
	temp = readl(addr);
	if (temp == 0xffffffff)
		return -EIO;
	*data = temp;
	return 0;
}
static inline int
lpfc_sli_read_hs(struct lpfc_hba *phba)
{
	phba->sli.slistat.err_attn_event++;
	if (lpfc_readl(phba->HSregaddr, &phba->work_hs) ||
		lpfc_readl(phba->MBslimaddr + 0xa8, &phba->work_status[0]) ||
		lpfc_readl(phba->MBslimaddr + 0xac, &phba->work_status[1])) {
		return -EIO;
	}
	writel(HA_ERATT, phba->HAregaddr);
	readl(phba->HAregaddr);  
	phba->pport->stopped = 1;
	return 0;
}
static inline struct lpfc_sli_ring *
lpfc_phba_elsring(struct lpfc_hba *phba)
{
	if (phba->sli_rev != LPFC_SLI_REV4  &&
	    phba->sli_rev != LPFC_SLI_REV3  &&
	    phba->sli_rev != LPFC_SLI_REV2)
		return NULL;
	if (phba->sli_rev == LPFC_SLI_REV4) {
		if (phba->sli4_hba.els_wq)
			return phba->sli4_hba.els_wq->pring;
		else
			return NULL;
	}
	return &phba->sli.sli3_ring[LPFC_ELS_RING];
}
static inline unsigned int
lpfc_next_online_cpu(const struct cpumask *mask, unsigned int start)
{
	unsigned int cpu_it;
	for_each_cpu_wrap(cpu_it, mask, start) {
		if (cpu_online(cpu_it))
			break;
	}
	return cpu_it;
}
static inline unsigned int lpfc_next_present_cpu(int n)
{
	unsigned int cpu;
	cpu = cpumask_next(n, cpu_present_mask);
	if (cpu >= nr_cpu_ids)
		cpu = cpumask_first(cpu_present_mask);
	return cpu;
}
static inline void
lpfc_sli4_mod_hba_eq_delay(struct lpfc_hba *phba, struct lpfc_queue *eq,
			   u32 delay)
{
	struct lpfc_register reg_data;
	reg_data.word0 = 0;
	bf_set(lpfc_sliport_eqdelay_id, &reg_data, eq->queue_id);
	bf_set(lpfc_sliport_eqdelay_delay, &reg_data, delay);
	writel(reg_data.word0, phba->sli4_hba.u.if_type2.EQDregaddr);
	eq->q_mode = delay;
}
#define DECLARE_ENUM2STR_LOOKUP(routine, enum_name, enum_init)		\
static struct {								\
	enum enum_name		value;					\
	char			*name;					\
} fc_##enum_name##_e2str_names[] = enum_init;				\
static const char *routine(enum enum_name table_key)			\
{									\
	int i;								\
	char *name = "Unrecognized";					\
									\
	for (i = 0; i < ARRAY_SIZE(fc_##enum_name##_e2str_names); i++) {\
		if (fc_##enum_name##_e2str_names[i].value == table_key) {\
			name = fc_##enum_name##_e2str_names[i].name;	\
			break;						\
		}							\
	}								\
	return name;							\
}
static inline int lpfc_is_vmid_enabled(struct lpfc_hba *phba)
{
	return phba->cfg_vmid_app_header || phba->cfg_vmid_priority_tagging;
}
static inline
u8 get_job_ulpstatus(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return bf_get(lpfc_wcqe_c_status, &iocbq->wcqe_cmpl);
	else
		return iocbq->iocb.ulpStatus;
}
static inline
u32 get_job_word4(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return iocbq->wcqe_cmpl.parameter;
	else
		return iocbq->iocb.un.ulpWord[4];
}
static inline
u8 get_job_cmnd(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return bf_get(wqe_cmnd, &iocbq->wqe.generic.wqe_com);
	else
		return iocbq->iocb.ulpCommand;
}
static inline
u16 get_job_ulpcontext(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return bf_get(wqe_ctxt_tag, &iocbq->wqe.generic.wqe_com);
	else
		return iocbq->iocb.ulpContext;
}
static inline
u16 get_job_rcvoxid(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return bf_get(wqe_rcvoxid, &iocbq->wqe.generic.wqe_com);
	else
		return iocbq->iocb.unsli3.rcvsli3.ox_id;
}
static inline
u32 get_job_data_placed(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return iocbq->wcqe_cmpl.total_data_placed;
	else
		return iocbq->iocb.un.genreq64.bdl.bdeSize;
}
static inline
u32 get_job_abtsiotag(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return iocbq->wqe.abort_cmd.wqe_com.abort_tag;
	else
		return iocbq->iocb.un.acxri.abortIoTag;
}
static inline
u32 get_job_els_rsp64_did(struct lpfc_hba *phba, struct lpfc_iocbq *iocbq)
{
	if (phba->sli_rev == LPFC_SLI_REV4)
		return bf_get(wqe_els_did, &iocbq->wqe.els_req.wqe_dest);
	else
		return iocbq->iocb.un.elsreq64.remoteID;
}
