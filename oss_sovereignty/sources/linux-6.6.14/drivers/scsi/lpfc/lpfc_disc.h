

#define FC_MAX_HOLD_RSCN     32	      
#define FC_MAX_NS_RSP        64512    
#define FC_MAXLOOP           126      
#define LPFC_DISC_FLOGI_TMO  10	      





enum lpfc_work_type {
	LPFC_EVT_ONLINE,
	LPFC_EVT_OFFLINE_PREP,
	LPFC_EVT_OFFLINE,
	LPFC_EVT_WARM_START,
	LPFC_EVT_KILL,
	LPFC_EVT_ELS_RETRY,
	LPFC_EVT_DEV_LOSS,
	LPFC_EVT_FASTPATH_MGMT_EVT,
	LPFC_EVT_RESET_HBA,
	LPFC_EVT_RECOVER_PORT
};


struct lpfc_work_evt {
	struct list_head      evt_listp;
	void                 *evt_arg1;
	void                 *evt_arg2;
	enum lpfc_work_type   evt;
};

struct lpfc_scsi_check_condition_event;
struct lpfc_scsi_varqueuedepth_event;
struct lpfc_scsi_event_header;
struct lpfc_fabric_event_header;
struct lpfc_fcprdchkerr_event;


struct lpfc_fast_path_event {
	struct lpfc_work_evt work_evt;
	struct lpfc_vport     *vport;
	union {
		struct lpfc_scsi_check_condition_event check_cond_evt;
		struct lpfc_scsi_varqueuedepth_event queue_depth_evt;
		struct lpfc_scsi_event_header scsi_evt;
		struct lpfc_fabric_event_header fabric_evt;
		struct lpfc_fcprdchkerr_event read_check_error;
	} un;
};

#define LPFC_SLI4_MAX_XRI	1024	
#define XRI_BITMAP_ULONGS (LPFC_SLI4_MAX_XRI / BITS_PER_LONG)
struct lpfc_node_rrqs {
	unsigned long xri_bitmap[XRI_BITMAP_ULONGS];
};

enum lpfc_fc4_xpt_flags {
	NLP_XPT_REGD		= 0x1,
	SCSI_XPT_REGD		= 0x2,
	NVME_XPT_REGD		= 0x4,
	NVME_XPT_UNREG_WAIT	= 0x8,
	NLP_XPT_HAS_HH		= 0x10
};

enum lpfc_nlp_save_flags {
	
	NLP_IN_RECOV_POST_DEV_LOSS	= 0x1,
	
	NLP_WAIT_FOR_LOGO		= 0x2,
};

struct lpfc_nodelist {
	struct list_head nlp_listp;
	struct serv_parm fc_sparam;		
	struct lpfc_name nlp_portname;
	struct lpfc_name nlp_nodename;

	spinlock_t	lock;			

	uint32_t         nlp_flag;		
	uint32_t         nlp_DID;		
	uint32_t         nlp_last_elscmd;	
	uint16_t         nlp_type;
#define NLP_FC_NODE        0x1			
#define NLP_FABRIC         0x4			
#define NLP_FCP_TARGET     0x8			
#define NLP_FCP_INITIATOR  0x10			
#define NLP_NVME_TARGET    0x20			
#define NLP_NVME_INITIATOR 0x40			
#define NLP_NVME_DISCOVERY 0x80                 

	uint16_t	nlp_fc4_type;		
						
#define NLP_FC4_NONE	0x0
#define NLP_FC4_FCP	0x1			
#define NLP_FC4_NVME	0x2			

	uint16_t        nlp_rpi;
	uint16_t        nlp_state;		
	uint16_t        nlp_prev_state;		
	uint16_t        nlp_xri;		
	uint16_t        nlp_sid;		
#define NLP_NO_SID		0xffff
	uint16_t	nlp_maxframe;		
	uint8_t		nlp_class_sup;		
	uint8_t         nlp_retry;		
	uint8_t         nlp_fcp_info;	        
#define NLP_FCP_2_DEVICE   0x10			
	u8		nlp_nvme_info;	        
	uint8_t		vmid_support;		
#define NLP_NVME_NSLER     0x1			

	struct timer_list   nlp_delayfunc;	
	struct lpfc_hba *phba;
	struct fc_rport *rport;		
	struct lpfc_nvme_rport *nrport;	
	struct lpfc_vport *vport;
	struct lpfc_work_evt els_retry_evt;
	struct lpfc_work_evt dev_loss_evt;
	struct lpfc_work_evt recovery_evt;
	struct kref     kref;
	atomic_t cmd_pending;
	uint32_t cmd_qdepth;
	unsigned long last_change_time;
	unsigned long *active_rrqs_xri_bitmap;
	uint32_t fc4_prli_sent;

	
	enum lpfc_nlp_save_flags save_flags;

	enum lpfc_fc4_xpt_flags fc4_xpt_flags;

	uint32_t nvme_fb_size; 
#define NVME_FB_BIT_SHIFT 9    
	uint32_t nlp_defer_did;
	wait_queue_head_t *logo_waitq;
};

struct lpfc_node_rrq {
	struct list_head list;
	uint16_t xritag;
	uint16_t send_rrq;
	uint16_t rxid;
	uint32_t         nlp_DID;		
	struct lpfc_vport *vport;
	unsigned long rrq_stop_time;
};

#define lpfc_ndlp_check_qdepth(phba, ndlp) \
	(ndlp->cmd_qdepth < phba->sli4_hba.max_cfg_param.max_xri)


#define NLP_IGNR_REG_CMPL  0x00000001 
#define NLP_REG_LOGIN_SEND 0x00000002   
#define NLP_RELEASE_RPI    0x00000004   
#define NLP_SUPPRESS_RSP   0x00000010	
#define NLP_PLOGI_SND      0x00000020	
#define NLP_PRLI_SND       0x00000040	
#define NLP_ADISC_SND      0x00000080	
#define NLP_LOGO_SND       0x00000100	
#define NLP_RNID_SND       0x00000400	
#define NLP_ELS_SND_MASK   0x000007e0	
#define NLP_NVMET_RECOV    0x00001000   
#define NLP_UNREG_INP      0x00008000	
#define NLP_DROPPED        0x00010000	
#define NLP_DELAY_TMO      0x00020000	
#define NLP_NPR_2B_DISC    0x00040000	
#define NLP_RCV_PLOGI      0x00080000	
#define NLP_LOGO_ACC       0x00100000	
#define NLP_TGT_NO_SCSIID  0x00200000	
#define NLP_ISSUE_LOGO     0x00400000	
#define NLP_IN_DEV_LOSS    0x00800000	
#define NLP_ACC_REGLOGIN   0x01000000	
#define NLP_NPR_ADISC      0x02000000	
#define NLP_RM_DFLT_RPI    0x04000000	
#define NLP_NODEV_REMOVE   0x08000000	
#define NLP_TARGET_REMOVE  0x10000000   
#define NLP_SC_REQ         0x20000000	
#define NLP_FIRSTBURST     0x40000000	
#define NLP_RPI_REGISTERED 0x80000000	




#define NLP_STE_UNUSED_NODE       0x0	
#define NLP_STE_PLOGI_ISSUE       0x1	
#define NLP_STE_ADISC_ISSUE       0x2	
#define NLP_STE_REG_LOGIN_ISSUE   0x3	
#define NLP_STE_PRLI_ISSUE        0x4	
#define NLP_STE_LOGO_ISSUE	  0x5	
#define NLP_STE_UNMAPPED_NODE     0x6	
#define NLP_STE_MAPPED_NODE       0x7	
#define NLP_STE_NPR_NODE          0x8	
#define NLP_STE_MAX_STATE         0x9
#define NLP_STE_FREED_NODE        0xff	






#define NLP_EVT_RCV_PLOGI         0x0	
#define NLP_EVT_RCV_PRLI          0x1	
#define NLP_EVT_RCV_LOGO          0x2	
#define NLP_EVT_RCV_ADISC         0x3	
#define NLP_EVT_RCV_PDISC         0x4	
#define NLP_EVT_RCV_PRLO          0x5	
#define NLP_EVT_CMPL_PLOGI        0x6	
#define NLP_EVT_CMPL_PRLI         0x7	
#define NLP_EVT_CMPL_LOGO         0x8	
#define NLP_EVT_CMPL_ADISC        0x9	
#define NLP_EVT_CMPL_REG_LOGIN    0xa	
#define NLP_EVT_DEVICE_RM         0xb	
#define NLP_EVT_DEVICE_RECOVERY   0xc	
#define NLP_EVT_MAX_EVENT         0xd
#define NLP_EVT_NOTHING_PENDING   0xff
