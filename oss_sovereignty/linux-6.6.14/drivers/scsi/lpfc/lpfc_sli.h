 

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SCSI_LPFC_DEBUG_FS)
#define CONFIG_SCSI_LPFC_DEBUG_FS
#endif

 
struct lpfc_hba;
struct lpfc_vport;

 
typedef enum _lpfc_ctx_cmd {
	LPFC_CTX_LUN,
	LPFC_CTX_TGT,
	LPFC_CTX_HOST
} lpfc_ctx_cmd;

 
enum lpfc_mbox_ctx {
	MBOX_THD_UNLOCKED,
	MBOX_THD_LOCKED
};

union lpfc_vmid_tag {
	uint32_t app_id;
	uint8_t cs_ctl_vmid;
	struct lpfc_vmid_context *vmid_context;	 
};

struct lpfc_cq_event {
	struct list_head list;
	uint16_t hdwq;
	union {
		struct lpfc_mcqe		mcqe_cmpl;
		struct lpfc_acqe_link		acqe_link;
		struct lpfc_acqe_fip		acqe_fip;
		struct lpfc_acqe_dcbx		acqe_dcbx;
		struct lpfc_acqe_grp5		acqe_grp5;
		struct lpfc_acqe_fc_la		acqe_fc;
		struct lpfc_acqe_sli		acqe_sli;
		struct lpfc_rcqe		rcqe_cmpl;
		struct sli4_wcqe_xri_aborted	wcqe_axri;
		struct lpfc_wcqe_complete	wcqe_cmpl;
	} cqe;
};

 
struct lpfc_iocbq {
	 
	struct list_head list;
	struct list_head clist;
	struct list_head dlist;
	uint16_t iotag;          
	uint16_t sli4_lxritag;   
	uint16_t sli4_xritag;    
	uint16_t hba_wqidx;      
	struct lpfc_cq_event cq_event;
	uint64_t isr_timestamp;

	union lpfc_wqe128 wqe;	 
	IOCB_t iocb;		 
	struct lpfc_wcqe_complete wcqe_cmpl;	 

	u32 unsol_rcv_len;	 

	 
	u8 num_bdes;	 
	u8 abort_bls;	 
	u8 abort_rctl;	 
	u8 priority;	 
	u8 retry;	 
	u8 rsvd1;        
	u8 rsvd2;        
	u8 rsvd3;	 

	u32 cmd_flag;
#define LPFC_IO_LIBDFC		1	 
#define LPFC_IO_WAKE		2	 
#define LPFC_IO_WAKE_TMO	LPFC_IO_WAKE  
#define LPFC_IO_FCP		4	 
#define LPFC_DRIVER_ABORTED	8	 
#define LPFC_IO_FABRIC		0x10	 
#define LPFC_DELAY_MEM_FREE	0x20     
#define LPFC_EXCHANGE_BUSY	0x40     
#define LPFC_USE_FCPWQIDX	0x80     
#define DSS_SECURITY_OP		0x100	 
#define LPFC_IO_ON_TXCMPLQ	0x200	 
#define LPFC_IO_DIF_PASS	0x400	 
#define LPFC_IO_DIF_STRIP	0x800	 
#define LPFC_IO_DIF_INSERT	0x1000	 
#define LPFC_IO_CMD_OUTSTANDING	0x2000  

#define LPFC_FIP_ELS_ID_MASK	0xc000	 
#define LPFC_FIP_ELS_ID_SHIFT	14

#define LPFC_IO_OAS		0x10000  
#define LPFC_IO_FOF		0x20000  
#define LPFC_IO_LOOPBACK	0x40000  
#define LPFC_PRLI_NVME_REQ	0x80000  
#define LPFC_PRLI_FCP_REQ	0x100000  
#define LPFC_IO_NVME	        0x200000  
#define LPFC_IO_NVME_LS		0x400000  
#define LPFC_IO_NVMET		0x800000  
#define LPFC_IO_VMID            0x1000000  
#define LPFC_IO_CMF		0x4000000  

	uint32_t drvrTimeout;	 
	struct lpfc_vport *vport; 
	struct lpfc_dmabuf *cmd_dmabuf;
	struct lpfc_dmabuf *rsp_dmabuf;
	struct lpfc_dmabuf *bpl_dmabuf;
	uint32_t event_tag;	 
	union {
		wait_queue_head_t    *wait_queue;
		struct lpfcMboxq     *mbox;
		struct lpfc_node_rrq *rrq;
		struct nvmefc_ls_req *nvme_lsreq;
		struct lpfc_async_xchg_ctx *axchg;
		struct bsg_job_data *dd_data;
	} context_un;

	struct lpfc_io_buf *io_buf;
	struct lpfc_iocbq *rsp_iocb;
	struct lpfc_nodelist *ndlp;
	union lpfc_vmid_tag vmid_tag;
	void (*fabric_cmd_cmpl)(struct lpfc_hba *phba, struct lpfc_iocbq *cmd,
				struct lpfc_iocbq *rsp);
	void (*wait_cmd_cmpl)(struct lpfc_hba *phba, struct lpfc_iocbq *cmd,
			      struct lpfc_iocbq *rsp);
	void (*cmd_cmpl)(struct lpfc_hba *phba, struct lpfc_iocbq *cmd,
			 struct lpfc_iocbq *rsp);
};

#define SLI_IOCB_RET_IOCB      1	 

#define IOCB_SUCCESS        0
#define IOCB_BUSY           1
#define IOCB_ERROR          2
#define IOCB_TIMEDOUT       3
#define IOCB_ABORTED        4
#define IOCB_ABORTING	    5
#define IOCB_NORESOURCE	    6

#define SLI_WQE_RET_WQE    1     

#define WQE_SUCCESS        0
#define WQE_BUSY           1
#define WQE_ERROR          2
#define WQE_TIMEDOUT       3
#define WQE_ABORTED        4
#define WQE_ABORTING	   5
#define WQE_NORESOURCE	   6

#define LPFC_MBX_WAKE		1
#define LPFC_MBX_IMED_UNREG	2

typedef struct lpfcMboxq {
	 
	struct list_head list;	 
	union {
		MAILBOX_t mb;		 
		struct lpfc_mqe mqe;
	} u;
	struct lpfc_vport *vport;  
	void *ctx_ndlp;		   
	void *ctx_buf;		   
	void *context3;

	void (*mbox_cmpl) (struct lpfc_hba *, struct lpfcMboxq *);
	uint8_t mbox_flag;
	uint16_t in_ext_byte_len;
	uint16_t out_ext_byte_len;
	uint8_t  mbox_offset_word;
	struct lpfc_mcqe mcqe;
	struct lpfc_mbx_nembed_sge_virt *sge_array;
} LPFC_MBOXQ_t;

#define MBX_POLL        1	 
#define MBX_NOWAIT      2	 

#define LPFC_MAX_RING_MASK  5	 
#define LPFC_SLI3_MAX_RING  4	 

struct lpfc_sli_ring;

struct lpfc_sli_ring_mask {
	uint8_t profile;	 
	uint8_t rctl;	 
	uint8_t type;	 
	uint8_t rsvd;
	 
	void (*lpfc_sli_rcv_unsol_event) (struct lpfc_hba *,
					 struct lpfc_sli_ring *,
					 struct lpfc_iocbq *);
};


 
struct lpfc_sli_ring_stat {
	uint64_t iocb_event;	  
	uint64_t iocb_cmd;	  
	uint64_t iocb_rsp;	  
	uint64_t iocb_cmd_delay;  
	uint64_t iocb_cmd_full;	  
	uint64_t iocb_cmd_empty;  
	uint64_t iocb_rsp_full;	  
};

struct lpfc_sli3_ring {
	uint32_t local_getidx;   
	uint32_t next_cmdidx;    
	uint32_t rspidx;	 
	uint32_t cmdidx;	 
	uint16_t numCiocb;	 
	uint16_t numRiocb;	 
	uint16_t sizeCiocb;	 
	uint16_t sizeRiocb;	 
	uint32_t *cmdringaddr;	 
	uint32_t *rspringaddr;	 
};

struct lpfc_sli4_ring {
	struct lpfc_queue *wqp;	 
};


 
struct lpfc_sli_ring {
	uint16_t flag;		 
#define LPFC_DEFERRED_RING_EVENT 0x001	 
#define LPFC_CALL_RING_AVAILABLE 0x002	 
#define LPFC_STOP_IOCB_EVENT     0x020	 
	uint16_t abtsiotag;	 

	uint8_t rsvd;
	uint8_t ringno;		 

	spinlock_t ring_lock;	 

	uint32_t fast_iotag;	 
	uint32_t iotag_ctr;	 
	uint32_t iotag_max;	 
	struct list_head txq;
	uint16_t txq_cnt;	 
	uint16_t txq_max;	 
	struct list_head txcmplq;
	uint16_t txcmplq_cnt;	 
	uint16_t txcmplq_max;	 
	uint32_t missbufcnt;	 
	struct list_head postbufq;
	uint16_t postbufq_cnt;	 
	uint16_t postbufq_max;	 
	struct list_head iocb_continueq;
	uint16_t iocb_continueq_cnt;	 
	uint16_t iocb_continueq_max;	 
	struct list_head iocb_continue_saveq;

	struct lpfc_sli_ring_mask prt[LPFC_MAX_RING_MASK];
	uint32_t num_mask;	 
	void (*lpfc_sli_rcv_async_status) (struct lpfc_hba *,
		struct lpfc_sli_ring *, struct lpfc_iocbq *);

	struct lpfc_sli_ring_stat stats;	 

	 
	void (*lpfc_sli_cmd_available) (struct lpfc_hba *,
					struct lpfc_sli_ring *);
	union {
		struct lpfc_sli3_ring sli3;
		struct lpfc_sli4_ring sli4;
	} sli;
};

 
struct lpfc_hbq_init {
	uint32_t rn;		 
	uint32_t entry_count;	 
	uint32_t headerLen;	 
	uint32_t logEntry;	 
	uint32_t profile;	 
	uint32_t ring_mask;	 
	uint32_t hbq_index;	 

	uint32_t seqlenoff;
	uint32_t maxlen;
	uint32_t seqlenbcnt;
	uint32_t cmdcodeoff;
	uint32_t cmdmatch[8];
	uint32_t mask_count;	 
	struct hbq_mask hbqMasks[6];

	 
	uint32_t buffer_count;	 
	uint32_t init_count;	 
	uint32_t add_count;	 
} ;

 
struct lpfc_sli_stat {
	uint64_t mbox_stat_err;   
	uint64_t mbox_cmd;        
	uint64_t sli_intr;        
	uint64_t sli_prev_intr;   
	uint64_t sli_ips;         
	uint32_t err_attn_event;  
	uint32_t link_event;      
	uint32_t mbox_event;      
	uint32_t mbox_busy;	  
};

 
struct lpfc_lnk_stat {
	uint32_t link_failure_count;
	uint32_t loss_of_sync_count;
	uint32_t loss_of_signal_count;
	uint32_t prim_seq_protocol_err_count;
	uint32_t invalid_tx_word_count;
	uint32_t invalid_crc_count;
	uint32_t error_frames;
	uint32_t link_events;
};

 
struct lpfc_sli {
	uint32_t num_rings;
	uint32_t sli_flag;

	 
#define LPFC_SLI_MBOX_ACTIVE      0x100	 
#define LPFC_SLI_ACTIVE           0x200	 
#define LPFC_PROCESS_LA           0x400	 
#define LPFC_BLOCK_MGMT_IO        0x800	 
#define LPFC_SLI_ASYNC_MBX_BLK    0x2000  
#define LPFC_SLI_SUPPRESS_RSP     0x4000  
#define LPFC_SLI_USE_EQDR         0x8000  
#define LPFC_QUEUE_FREE_INIT	  0x10000  
#define LPFC_QUEUE_FREE_WAIT	  0x20000  

	struct lpfc_sli_ring *sli3_ring;

	struct lpfc_sli_stat slistat;	 
	struct list_head mboxq;
	uint16_t mboxq_cnt;	 
	uint16_t mboxq_max;	 
	LPFC_MBOXQ_t *mbox_active;	 
	struct list_head mboxq_cmpl;

	struct timer_list mbox_tmo;	 

#define LPFC_IOCBQ_LOOKUP_INCREMENT  1024
	struct lpfc_iocbq ** iocbq_lookup;  
	size_t iocbq_lookup_len;            
	uint16_t  last_iotag;               
	time64_t  stats_start;		    
	struct lpfc_lnk_stat lnk_stat_offsets;
};

 
#define LPFC_MBOX_TMO				30
 
#define LPFC_MBOX_SLI4_CONFIG_TMO		60
 
#define LPFC_MBOX_SLI4_CONFIG_EXTENDED_TMO	300
 
#define LPFC_MBOX_TMO_FLASH_CMD			300

struct lpfc_io_buf {
	 
	struct list_head list;
	void *data;

	dma_addr_t dma_handle;
	dma_addr_t dma_phys_sgl;

	struct sli4_sge *dma_sgl;  

	 
	struct list_head dma_sgl_xtra_list;

	 
	struct list_head dma_cmd_rsp_list;

	struct lpfc_iocbq cur_iocbq;
	struct lpfc_sli4_hdw_queue *hdwq;
	uint16_t hdwq_no;
	uint16_t cpu;

	struct lpfc_nodelist *ndlp;
	uint32_t timeout;
	uint16_t flags;
#define LPFC_SBUF_XBUSY		0x1	 
#define LPFC_SBUF_BUMP_QDEPTH	0x2	 
					 
#define LPFC_SBUF_NORMAL_DIF	0x4	 
#define LPFC_SBUF_PASS_DIF	0x8	 
#define LPFC_SBUF_NOT_POSTED    0x10     
	uint16_t status;	 
	uint32_t result;	 

	uint32_t   seg_cnt;	 
	unsigned long start_time;
	spinlock_t buf_lock;	 
	bool expedite;		 

	union {
		 
		struct {
			struct scsi_cmnd *pCmd;
			struct lpfc_rport_data *rdata;
			uint32_t prot_seg_cnt;   

			 
			struct fcp_cmnd *fcp_cmnd;
			struct fcp_rsp *fcp_rsp;

			wait_queue_head_t *waitq;

#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
			 
			void *prot_data_segment;
			uint32_t prot_data;
			uint32_t prot_data_type;
#define	LPFC_INJERR_REFTAG	1
#define	LPFC_INJERR_APPTAG	2
#define	LPFC_INJERR_GUARD	3
#endif
		};

		 
		struct {
			struct nvmefc_fcp_req *nvmeCmd;
			uint16_t qidx;
		};
	};
#ifdef CONFIG_SCSI_LPFC_DEBUG_FS
	uint64_t ts_cmd_start;
	uint64_t ts_last_cmd;
	uint64_t ts_cmd_wqput;
	uint64_t ts_isr_cmpl;
	uint64_t ts_data_io;
#endif
	uint64_t rx_cmd_start;
};
