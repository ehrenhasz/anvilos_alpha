#ifndef _MEGARAID_SAS_FUSION_H_
#define _MEGARAID_SAS_FUSION_H_
#define MEGASAS_CHAIN_FRAME_SZ_MIN 1024
#define MFI_FUSION_ENABLE_INTERRUPT_MASK (0x00000009)
#define MEGASAS_MAX_CHAIN_SHIFT			5
#define MEGASAS_MAX_CHAIN_SIZE_UNITS_MASK	0x400000
#define MEGASAS_MAX_CHAIN_SIZE_MASK		0x3E0
#define MEGASAS_256K_IO				128
#define MEGASAS_1MB_IO				(MEGASAS_256K_IO * 4)
#define MEGA_MPI2_RAID_DEFAULT_IO_FRAME_SIZE 256
#define MEGASAS_MPI2_FUNCTION_PASSTHRU_IO_REQUEST   0xF0
#define MEGASAS_MPI2_FUNCTION_LD_IO_REQUEST         0xF1
#define MEGASAS_LOAD_BALANCE_FLAG		    0x1
#define MEGASAS_DCMD_MBOX_PEND_FLAG		    0x1
#define HOST_DIAG_WRITE_ENABLE			    0x80
#define HOST_DIAG_RESET_ADAPTER			    0x4
#define MEGASAS_FUSION_MAX_RESET_TRIES		    3
#define MAX_MSIX_QUEUES_FUSION			    128
#define RDPQ_MAX_INDEX_IN_ONE_CHUNK		    16
#define RDPQ_MAX_CHUNK_COUNT (MAX_MSIX_QUEUES_FUSION / RDPQ_MAX_INDEX_IN_ONE_CHUNK)
#define MPI2_TYPE_CUDA				    0x2
#define MPI25_SAS_DEVICE0_FLAGS_ENABLED_FAST_PATH   0x4000
#define	MR_RL_FLAGS_GRANT_DESTINATION_CPU0	    0x00
#define	MR_RL_FLAGS_GRANT_DESTINATION_CPU1	    0x10
#define	MR_RL_FLAGS_GRANT_DESTINATION_CUDA	    0x80
#define MR_RL_FLAGS_SEQ_NUM_ENABLE		    0x8
#define MR_RL_WRITE_THROUGH_MODE		    0x00
#define MR_RL_WRITE_BACK_MODE			    0x01
#define MR_PROT_INFO_TYPE_CONTROLLER                0x8
#define MEGASAS_SCSI_VARIABLE_LENGTH_CMD            0x7f
#define MEGASAS_SCSI_SERVICE_ACTION_READ32          0x9
#define MEGASAS_SCSI_SERVICE_ACTION_WRITE32         0xB
#define MEGASAS_SCSI_ADDL_CDB_LEN                   0x18
#define MEGASAS_RD_WR_PROTECT_CHECK_ALL		    0x20
#define MEGASAS_RD_WR_PROTECT_CHECK_NONE	    0x60
#define MPI2_SUP_REPLY_POST_HOST_INDEX_OFFSET   (0x0000030C)
#define MPI2_REPLY_POST_HOST_INDEX_OFFSET	(0x0000006C)
#define MR_RAID_CTX_RAID_FLAGS_IO_SUB_TYPE_SHIFT   0x4
#define MR_RAID_CTX_RAID_FLAGS_IO_SUB_TYPE_MASK    0x30
enum MR_RAID_FLAGS_IO_SUB_TYPE {
	MR_RAID_FLAGS_IO_SUB_TYPE_NONE = 0,
	MR_RAID_FLAGS_IO_SUB_TYPE_SYSTEM_PD = 1,
	MR_RAID_FLAGS_IO_SUB_TYPE_RMW_DATA     = 2,
	MR_RAID_FLAGS_IO_SUB_TYPE_RMW_P        = 3,
	MR_RAID_FLAGS_IO_SUB_TYPE_RMW_Q        = 4,
	MR_RAID_FLAGS_IO_SUB_TYPE_CACHE_BYPASS = 6,
	MR_RAID_FLAGS_IO_SUB_TYPE_LDIO_BW_LIMIT = 7,
	MR_RAID_FLAGS_IO_SUB_TYPE_R56_DIV_OFFLOAD = 8
};
#define MEGASAS_REQ_DESCRIPT_FLAGS_LD_IO           0x7
#define MEGASAS_REQ_DESCRIPT_FLAGS_MFA             0x1
#define MEGASAS_REQ_DESCRIPT_FLAGS_NO_LOCK	   0x2
#define MEGASAS_REQ_DESCRIPT_FLAGS_TYPE_SHIFT      1
#define MEGASAS_FP_CMD_LEN	16
#define MEGASAS_FUSION_IN_RESET 0
#define MEGASAS_FUSION_OCR_NOT_POSSIBLE 1
#define RAID_1_PEER_CMDS 2
#define JBOD_MAPS_COUNT	2
#define MEGASAS_REDUCE_QD_COUNT 64
#define IOC_INIT_FRAME_SIZE 4096
struct RAID_CONTEXT {
#if   defined(__BIG_ENDIAN_BITFIELD)
	u8 nseg:4;
	u8 type:4;
#else
	u8 type:4;
	u8 nseg:4;
#endif
	u8 resvd0;
	__le16 timeout_value;
	u8 reg_lock_flags;
	u8 resvd1;
	__le16 virtual_disk_tgt_id;
	__le64 reg_lock_row_lba;
	__le32 reg_lock_length;
	__le16 next_lmid;
	u8 ex_status;
	u8 status;
	u8 raid_flags;
	u8 num_sge;
	__le16 config_seq_num;
	u8 span_arm;
	u8 priority;
	u8 num_sge_ext;
	u8 resvd2;
};
struct RAID_CONTEXT_G35 {
	#define RAID_CONTEXT_NSEG_MASK	0x00F0
	#define RAID_CONTEXT_NSEG_SHIFT	4
	#define RAID_CONTEXT_TYPE_MASK	0x000F
	#define RAID_CONTEXT_TYPE_SHIFT	0
	u16		nseg_type;
	u16 timeout_value;  
	u16		routing_flags;	 
	u16 virtual_disk_tgt_id;    
	__le64 reg_lock_row_lba;       
	u32 reg_lock_length;       
	union {                      
		u16 rmw_op_index;    
		u16 peer_smid;       
		u16 r56_arm_map;     
	} flow_specific;
	u8 ex_status;        
	u8 status;           
	u8 raid_flags;		 
	u8 span_arm;             
	u16	config_seq_num;            
	union {
		#define NUM_SGE_MASK_LOWER	0xFF
		#define NUM_SGE_MASK_UPPER	0x0F
		#define NUM_SGE_SHIFT_UPPER	8
		#define STREAM_DETECT_SHIFT	7
		#define STREAM_DETECT_MASK	0x80
		struct {
#if   defined(__BIG_ENDIAN_BITFIELD)  
			u16 stream_detected:1;
			u16 reserved:3;
			u16 num_sge:12;
#else
			u16 num_sge:12;
			u16 reserved:3;
			u16 stream_detected:1;
#endif
		} bits;
		u8 bytes[2];
	} u;
	u8 resvd2[2];           
};
#define MR_RAID_CTX_ROUTINGFLAGS_SLD_SHIFT	1
#define MR_RAID_CTX_ROUTINGFLAGS_C2D_SHIFT	2
#define MR_RAID_CTX_ROUTINGFLAGS_FWD_SHIFT	3
#define MR_RAID_CTX_ROUTINGFLAGS_SQN_SHIFT	4
#define MR_RAID_CTX_ROUTINGFLAGS_SBS_SHIFT	5
#define MR_RAID_CTX_ROUTINGFLAGS_RW_SHIFT	6
#define MR_RAID_CTX_ROUTINGFLAGS_LOG_SHIFT	7
#define MR_RAID_CTX_ROUTINGFLAGS_CPUSEL_SHIFT	8
#define MR_RAID_CTX_ROUTINGFLAGS_CPUSEL_MASK	0x0F00
#define MR_RAID_CTX_ROUTINGFLAGS_SETDIVERT_SHIFT	12
#define MR_RAID_CTX_ROUTINGFLAGS_SETDIVERT_MASK	0xF000
static inline void set_num_sge(struct RAID_CONTEXT_G35 *rctx_g35,
			       u16 sge_count)
{
	rctx_g35->u.bytes[0] = (u8)(sge_count & NUM_SGE_MASK_LOWER);
	rctx_g35->u.bytes[1] |= (u8)((sge_count >> NUM_SGE_SHIFT_UPPER)
							& NUM_SGE_MASK_UPPER);
}
static inline u16 get_num_sge(struct RAID_CONTEXT_G35 *rctx_g35)
{
	u16 sge_count;
	sge_count = (u16)(((rctx_g35->u.bytes[1] & NUM_SGE_MASK_UPPER)
			<< NUM_SGE_SHIFT_UPPER) | (rctx_g35->u.bytes[0]));
	return sge_count;
}
#define SET_STREAM_DETECTED(rctx_g35) \
	(rctx_g35.u.bytes[1] |= STREAM_DETECT_MASK)
#define CLEAR_STREAM_DETECTED(rctx_g35) \
	(rctx_g35.u.bytes[1] &= ~(STREAM_DETECT_MASK))
static inline bool is_stream_detected(struct RAID_CONTEXT_G35 *rctx_g35)
{
	return ((rctx_g35->u.bytes[1] & STREAM_DETECT_MASK));
}
union RAID_CONTEXT_UNION {
	struct RAID_CONTEXT raid_context;
	struct RAID_CONTEXT_G35 raid_context_g35;
};
#define RAID_CTX_SPANARM_ARM_SHIFT	(0)
#define RAID_CTX_SPANARM_ARM_MASK	(0x1f)
#define RAID_CTX_SPANARM_SPAN_SHIFT	(5)
#define RAID_CTX_SPANARM_SPAN_MASK	(0xE0)
#define RAID_CTX_R56_Q_ARM_MASK		(0x1F)
#define RAID_CTX_R56_P_ARM_SHIFT	(5)
#define RAID_CTX_R56_P_ARM_MASK		(0x3E0)
#define RAID_CTX_R56_LOG_ARM_SHIFT	(10)
#define RAID_CTX_R56_LOG_ARM_MASK	(0x7C00)
#define BITS_PER_INDEX_STREAM		4
#define INVALID_STREAM_NUM              16
#define MR_STREAM_BITMAP		0x76543210
#define STREAM_MASK			((1 << BITS_PER_INDEX_STREAM) - 1)
#define ZERO_LAST_STREAM		0x0fffffff
#define MAX_STREAMS_TRACKED		8
enum REGION_TYPE {
	REGION_TYPE_UNUSED       = 0,
	REGION_TYPE_SHARED_READ  = 1,
	REGION_TYPE_SHARED_WRITE = 2,
	REGION_TYPE_EXCLUSIVE    = 3,
};
#define MPI2_FUNCTION_IOC_INIT              (0x02)  
#define MPI2_WHOINIT_HOST_DRIVER            (0x04)
#define MPI2_VERSION_MAJOR                  (0x02)
#define MPI2_VERSION_MINOR                  (0x00)
#define MPI2_VERSION_MAJOR_MASK             (0xFF00)
#define MPI2_VERSION_MAJOR_SHIFT            (8)
#define MPI2_VERSION_MINOR_MASK             (0x00FF)
#define MPI2_VERSION_MINOR_SHIFT            (0)
#define MPI2_VERSION ((MPI2_VERSION_MAJOR << MPI2_VERSION_MAJOR_SHIFT) | \
		      MPI2_VERSION_MINOR)
#define MPI2_HEADER_VERSION_UNIT            (0x10)
#define MPI2_HEADER_VERSION_DEV             (0x00)
#define MPI2_HEADER_VERSION_UNIT_MASK       (0xFF00)
#define MPI2_HEADER_VERSION_UNIT_SHIFT      (8)
#define MPI2_HEADER_VERSION_DEV_MASK        (0x00FF)
#define MPI2_HEADER_VERSION_DEV_SHIFT       (0)
#define MPI2_HEADER_VERSION ((MPI2_HEADER_VERSION_UNIT << 8) | \
			     MPI2_HEADER_VERSION_DEV)
#define MPI2_IEEE_SGE_FLAGS_IOCPLBNTA_ADDR      (0x03)
#define MPI2_SCSIIO_EEDPFLAGS_INC_PRI_REFTAG        (0x8000)
#define MPI2_SCSIIO_EEDPFLAGS_CHECK_REFTAG          (0x0400)
#define MPI2_SCSIIO_EEDPFLAGS_CHECK_REMOVE_OP       (0x0003)
#define MPI2_SCSIIO_EEDPFLAGS_CHECK_APPTAG          (0x0200)
#define MPI2_SCSIIO_EEDPFLAGS_CHECK_GUARD           (0x0100)
#define MPI2_SCSIIO_EEDPFLAGS_INSERT_OP             (0x0004)
#define MPI25_SCSIIO_EEDPFLAGS_DO_NOT_DISABLE_MODE  (0x0040)
#define MPI2_FUNCTION_SCSI_IO_REQUEST               (0x00)  
#define MPI2_FUNCTION_SCSI_TASK_MGMT                (0x01)
#define MPI2_REQ_DESCRIPT_FLAGS_HIGH_PRIORITY       (0x03)
#define MPI2_REQ_DESCRIPT_FLAGS_FP_IO               (0x06)
#define MPI2_REQ_DESCRIPT_FLAGS_SCSI_IO                 (0x00)
#define MPI2_SGE_FLAGS_64_BIT_ADDRESSING        (0x02)
#define MPI2_SCSIIO_CONTROL_WRITE               (0x01000000)
#define MPI2_SCSIIO_CONTROL_READ                (0x02000000)
#define MPI2_REQ_DESCRIPT_FLAGS_TYPE_MASK       (0x0E)
#define MPI2_RPY_DESCRIPT_FLAGS_UNUSED          (0x0F)
#define MPI2_RPY_DESCRIPT_FLAGS_SCSI_IO_SUCCESS (0x00)
#define MPI2_RPY_DESCRIPT_FLAGS_TYPE_MASK       (0x0F)
#define MPI2_WRSEQ_FLUSH_KEY_VALUE              (0x0)
#define MPI2_WRITE_SEQUENCE_OFFSET              (0x00000004)
#define MPI2_WRSEQ_1ST_KEY_VALUE                (0xF)
#define MPI2_WRSEQ_2ND_KEY_VALUE                (0x4)
#define MPI2_WRSEQ_3RD_KEY_VALUE                (0xB)
#define MPI2_WRSEQ_4TH_KEY_VALUE                (0x2)
#define MPI2_WRSEQ_5TH_KEY_VALUE                (0x7)
#define MPI2_WRSEQ_6TH_KEY_VALUE                (0xD)
struct MPI25_IEEE_SGE_CHAIN64 {
	__le64			Address;
	__le32			Length;
	__le16			Reserved1;
	u8                      NextChainOffset;
	u8                      Flags;
};
struct MPI2_SGE_SIMPLE_UNION {
	__le32                     FlagsLength;
	union {
		__le32                 Address32;
		__le64                 Address64;
	} u;
};
struct MPI2_SCSI_IO_CDB_EEDP32 {
	u8                      CDB[20];                     
	__be32			PrimaryReferenceTag;         
	__be16			PrimaryApplicationTag;       
	__be16			PrimaryApplicationTagMask;   
	__le32			TransferLength;              
};
struct MPI2_SGE_CHAIN_UNION {
	__le16			Length;
	u8                      NextChainOffset;
	u8                      Flags;
	union {
		__le32		Address32;
		__le64		Address64;
	} u;
};
struct MPI2_IEEE_SGE_SIMPLE32 {
	__le32			Address;
	__le32			FlagsLength;
};
struct MPI2_IEEE_SGE_CHAIN32 {
	__le32			Address;
	__le32			FlagsLength;
};
struct MPI2_IEEE_SGE_SIMPLE64 {
	__le64			Address;
	__le32			Length;
	__le16			Reserved1;
	u8                      Reserved2;
	u8                      Flags;
};
struct MPI2_IEEE_SGE_CHAIN64 {
	__le64			Address;
	__le32			Length;
	__le16			Reserved1;
	u8                      Reserved2;
	u8                      Flags;
};
union MPI2_IEEE_SGE_SIMPLE_UNION {
	struct MPI2_IEEE_SGE_SIMPLE32  Simple32;
	struct MPI2_IEEE_SGE_SIMPLE64  Simple64;
};
union MPI2_IEEE_SGE_CHAIN_UNION {
	struct MPI2_IEEE_SGE_CHAIN32   Chain32;
	struct MPI2_IEEE_SGE_CHAIN64   Chain64;
};
union MPI2_SGE_IO_UNION {
	struct MPI2_SGE_SIMPLE_UNION       MpiSimple;
	struct MPI2_SGE_CHAIN_UNION        MpiChain;
	union MPI2_IEEE_SGE_SIMPLE_UNION  IeeeSimple;
	union MPI2_IEEE_SGE_CHAIN_UNION   IeeeChain;
};
union MPI2_SCSI_IO_CDB_UNION {
	u8                      CDB32[32];
	struct MPI2_SCSI_IO_CDB_EEDP32 EEDP32;
	struct MPI2_SGE_SIMPLE_UNION SGE;
};
struct MPI2_SCSI_TASK_MANAGE_REQUEST {
	u16 DevHandle;		 
	u8 ChainOffset;		 
	u8 Function;		 
	u8 Reserved1;		 
	u8 TaskType;		 
	u8 Reserved2;		 
	u8 MsgFlags;		 
	u8 VP_ID;		 
	u8 VF_ID;		 
	u16 Reserved3;		 
	u8 LUN[8];		 
	u32 Reserved4[7];	 
	u16 TaskMID;		 
	u16 Reserved5;		 
};
struct MPI2_SCSI_TASK_MANAGE_REPLY {
	u16 DevHandle;		 
	u8 MsgLength;		 
	u8 Function;		 
	u8 ResponseCode;	 
	u8 TaskType;		 
	u8 Reserved1;		 
	u8 MsgFlags;		 
	u8 VP_ID;		 
	u8 VF_ID;		 
	u16 Reserved2;		 
	u16 Reserved3;		 
	u16 IOCStatus;		 
	u32 IOCLogInfo;		 
	u32 TerminationCount;	 
	u32 ResponseInfo;	 
};
struct MR_TM_REQUEST {
	char request[128];
};
struct MR_TM_REPLY {
	char reply[128];
};
struct MR_TASK_MANAGE_REQUEST {
	struct MR_TM_REQUEST         TmRequest;
	union {
		struct {
#if   defined(__BIG_ENDIAN_BITFIELD)
			u32 reserved1:30;
			u32 isTMForPD:1;
			u32 isTMForLD:1;
#else
			u32 isTMForLD:1;
			u32 isTMForPD:1;
			u32 reserved1:30;
#endif
			u32 reserved2;
		} tmReqFlags;
		struct MR_TM_REPLY   TMReply;
	};
};
#define MPI2_SCSITASKMGMT_TASKTYPE_ABORT_TASK           (0x01)
#define MPI2_SCSITASKMGMT_TASKTYPE_ABRT_TASK_SET        (0x02)
#define MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET         (0x03)
#define MPI2_SCSITASKMGMT_TASKTYPE_LOGICAL_UNIT_RESET   (0x05)
#define MPI2_SCSITASKMGMT_TASKTYPE_CLEAR_TASK_SET       (0x06)
#define MPI2_SCSITASKMGMT_TASKTYPE_QUERY_TASK           (0x07)
#define MPI2_SCSITASKMGMT_TASKTYPE_CLR_ACA              (0x08)
#define MPI2_SCSITASKMGMT_TASKTYPE_QRY_TASK_SET         (0x09)
#define MPI2_SCSITASKMGMT_TASKTYPE_QRY_ASYNC_EVENT      (0x0A)
#define MPI2_SCSITASKMGMT_RSP_TM_COMPLETE               (0x00)
#define MPI2_SCSITASKMGMT_RSP_INVALID_FRAME             (0x02)
#define MPI2_SCSITASKMGMT_RSP_TM_NOT_SUPPORTED          (0x04)
#define MPI2_SCSITASKMGMT_RSP_TM_FAILED                 (0x05)
#define MPI2_SCSITASKMGMT_RSP_TM_SUCCEEDED              (0x08)
#define MPI2_SCSITASKMGMT_RSP_TM_INVALID_LUN            (0x09)
#define MPI2_SCSITASKMGMT_RSP_TM_OVERLAPPED_TAG         (0x0A)
#define MPI2_SCSITASKMGMT_RSP_IO_QUEUED_ON_IOC          (0x80)
struct MPI2_RAID_SCSI_IO_REQUEST {
	__le16			DevHandle;                       
	u8                      ChainOffset;                     
	u8                      Function;                        
	__le16			Reserved1;                       
	u8                      Reserved2;                       
	u8                      MsgFlags;                        
	u8                      VP_ID;                           
	u8                      VF_ID;                           
	__le16			Reserved3;                       
	__le32			SenseBufferLowAddress;           
	__le16			SGLFlags;                        
	u8                      SenseBufferLength;               
	u8                      Reserved4;                       
	u8                      SGLOffset0;                      
	u8                      SGLOffset1;                      
	u8                      SGLOffset2;                      
	u8                      SGLOffset3;                      
	__le32			SkipCount;                       
	__le32			DataLength;                      
	__le32			BidirectionalDataLength;         
	__le16			IoFlags;                         
	__le16			EEDPFlags;                       
	__le32			EEDPBlockSize;                   
	__le32			SecondaryReferenceTag;           
	__le16			SecondaryApplicationTag;         
	__le16			ApplicationTagTranslationMask;   
	u8                      LUN[8];                          
	__le32			Control;                         
	union MPI2_SCSI_IO_CDB_UNION  CDB;			 
	union RAID_CONTEXT_UNION RaidContext;   
	union {
		union MPI2_SGE_IO_UNION       SGL;		 
		DECLARE_FLEX_ARRAY(union MPI2_SGE_IO_UNION, SGLs);
	};
};
struct MEGASAS_RAID_MFA_IO_REQUEST_DESCRIPTOR {
	u32     RequestFlags:8;
	u32     MessageAddress1:24;
	u32     MessageAddress2;
};
struct MPI2_DEFAULT_REQUEST_DESCRIPTOR {
	u8              RequestFlags;                
	u8              MSIxIndex;                   
	__le16		SMID;                        
	__le16		LMID;                        
	__le16		DescriptorTypeDependent;     
};
struct MPI2_HIGH_PRIORITY_REQUEST_DESCRIPTOR {
	u8              RequestFlags;                
	u8              MSIxIndex;                   
	__le16		SMID;                        
	__le16		LMID;                        
	__le16		Reserved1;                   
};
struct MPI2_SCSI_IO_REQUEST_DESCRIPTOR {
	u8              RequestFlags;                
	u8              MSIxIndex;                   
	__le16		SMID;                        
	__le16		LMID;                        
	__le16		DevHandle;                   
};
struct MPI2_SCSI_TARGET_REQUEST_DESCRIPTOR {
	u8              RequestFlags;                
	u8              MSIxIndex;                   
	__le16		SMID;                        
	__le16		LMID;                        
	__le16		IoIndex;                     
};
struct MPI2_RAID_ACCEL_REQUEST_DESCRIPTOR {
	u8              RequestFlags;                
	u8              MSIxIndex;                   
	__le16		SMID;                        
	__le16		LMID;                        
	__le16		Reserved;                    
};
union MEGASAS_REQUEST_DESCRIPTOR_UNION {
	struct MPI2_DEFAULT_REQUEST_DESCRIPTOR             Default;
	struct MPI2_HIGH_PRIORITY_REQUEST_DESCRIPTOR       HighPriority;
	struct MPI2_SCSI_IO_REQUEST_DESCRIPTOR             SCSIIO;
	struct MPI2_SCSI_TARGET_REQUEST_DESCRIPTOR         SCSITarget;
	struct MPI2_RAID_ACCEL_REQUEST_DESCRIPTOR          RAIDAccelerator;
	struct MEGASAS_RAID_MFA_IO_REQUEST_DESCRIPTOR      MFAIo;
	union {
		struct {
			__le32 low;
			__le32 high;
		} u;
		__le64 Words;
	};
};
struct MPI2_DEFAULT_REPLY_DESCRIPTOR {
	u8              ReplyFlags;                  
	u8              MSIxIndex;                   
	__le16		DescriptorTypeDependent1;    
	__le32		DescriptorTypeDependent2;    
};
struct MPI2_ADDRESS_REPLY_DESCRIPTOR {
	u8              ReplyFlags;                  
	u8              MSIxIndex;                   
	__le16		SMID;                        
	__le32		ReplyFrameAddress;           
};
struct MPI2_SCSI_IO_SUCCESS_REPLY_DESCRIPTOR {
	u8              ReplyFlags;                  
	u8              MSIxIndex;                   
	__le16		SMID;                        
	__le16		TaskTag;                     
	__le16		Reserved1;                   
};
struct MPI2_TARGETASSIST_SUCCESS_REPLY_DESCRIPTOR {
	u8              ReplyFlags;                  
	u8              MSIxIndex;                   
	__le16		SMID;                        
	u8              SequenceNumber;              
	u8              Reserved1;                   
	__le16		IoIndex;                     
};
struct MPI2_TARGET_COMMAND_BUFFER_REPLY_DESCRIPTOR {
	u8              ReplyFlags;                  
	u8              MSIxIndex;                   
	u8              VP_ID;                       
	u8              Flags;                       
	__le16		InitiatorDevHandle;          
	__le16		IoIndex;                     
};
struct MPI2_RAID_ACCELERATOR_SUCCESS_REPLY_DESCRIPTOR {
	u8              ReplyFlags;                  
	u8              MSIxIndex;                   
	__le16		SMID;                        
	__le32		Reserved;                    
};
union MPI2_REPLY_DESCRIPTORS_UNION {
	struct MPI2_DEFAULT_REPLY_DESCRIPTOR                   Default;
	struct MPI2_ADDRESS_REPLY_DESCRIPTOR                   AddressReply;
	struct MPI2_SCSI_IO_SUCCESS_REPLY_DESCRIPTOR           SCSIIOSuccess;
	struct MPI2_TARGETASSIST_SUCCESS_REPLY_DESCRIPTOR TargetAssistSuccess;
	struct MPI2_TARGET_COMMAND_BUFFER_REPLY_DESCRIPTOR TargetCommandBuffer;
	struct MPI2_RAID_ACCELERATOR_SUCCESS_REPLY_DESCRIPTOR
	RAIDAcceleratorSuccess;
	__le64                                             Words;
};
struct MPI2_IOC_INIT_REQUEST {
	u8                      WhoInit;                         
	u8                      Reserved1;                       
	u8                      ChainOffset;                     
	u8                      Function;                        
	__le16			Reserved2;                       
	u8                      Reserved3;                       
	u8                      MsgFlags;                        
	u8                      VP_ID;                           
	u8                      VF_ID;                           
	__le16			Reserved4;                       
	__le16			MsgVersion;                      
	__le16			HeaderVersion;                   
	u32                     Reserved5;                       
	__le16			Reserved6;                       
	u8                      HostPageSize;                    
	u8                      HostMSIxVectors;                 
	__le16			Reserved8;                       
	__le16			SystemRequestFrameSize;          
	__le16			ReplyDescriptorPostQueueDepth;   
	__le16			ReplyFreeQueueDepth;             
	__le32			SenseBufferAddressHigh;          
	__le32			SystemReplyAddressHigh;          
	__le64			SystemRequestFrameBaseAddress;   
	__le64			ReplyDescriptorPostQueueAddress; 
	__le64			ReplyFreeQueueAddress;           
	__le64			TimeStamp;                       
};
#define MR_PD_INVALID 0xFFFF
#define MR_DEVHANDLE_INVALID 0xFFFF
#define MAX_SPAN_DEPTH 8
#define MAX_QUAD_DEPTH	MAX_SPAN_DEPTH
#define MAX_RAIDMAP_SPAN_DEPTH (MAX_SPAN_DEPTH)
#define MAX_ROW_SIZE 32
#define MAX_RAIDMAP_ROW_SIZE (MAX_ROW_SIZE)
#define MAX_LOGICAL_DRIVES 64
#define MAX_LOGICAL_DRIVES_EXT 256
#define MAX_LOGICAL_DRIVES_DYN 512
#define MAX_RAIDMAP_LOGICAL_DRIVES (MAX_LOGICAL_DRIVES)
#define MAX_RAIDMAP_VIEWS (MAX_LOGICAL_DRIVES)
#define MAX_ARRAYS 128
#define MAX_RAIDMAP_ARRAYS (MAX_ARRAYS)
#define MAX_ARRAYS_EXT	256
#define MAX_API_ARRAYS_EXT (MAX_ARRAYS_EXT)
#define MAX_API_ARRAYS_DYN 512
#define MAX_PHYSICAL_DEVICES 256
#define MAX_RAIDMAP_PHYSICAL_DEVICES (MAX_PHYSICAL_DEVICES)
#define MAX_RAIDMAP_PHYSICAL_DEVICES_DYN 512
#define MR_DCMD_LD_MAP_GET_INFO             0x0300e101
#define MR_DCMD_SYSTEM_PD_MAP_GET_INFO      0x0200e102
#define MR_DCMD_DRV_GET_TARGET_PROP         0x0200e103
#define MR_DCMD_CTRL_SHARED_HOST_MEM_ALLOC  0x010e8485    
#define MR_DCMD_LD_VF_MAP_GET_ALL_LDS_111   0x03200200
#define MR_DCMD_LD_VF_MAP_GET_ALL_LDS       0x03150200
#define MR_DCMD_CTRL_SNAPDUMP_GET_PROPERTIES	0x01200100
#define MR_DCMD_CTRL_DEVICE_LIST_GET		0x01190600
struct MR_DEV_HANDLE_INFO {
	__le16	curDevHdl;
	u8      validHandles;
	u8      interfaceType;
	__le16	devHandle[2];
};
struct MR_ARRAY_INFO {
	__le16	pd[MAX_RAIDMAP_ROW_SIZE];
};
struct MR_QUAD_ELEMENT {
	__le64     logStart;
	__le64     logEnd;
	__le64     offsetInSpan;
	__le32     diff;
	__le32     reserved1;
};
struct MR_SPAN_INFO {
	__le32             noElements;
	__le32             reserved1;
	struct MR_QUAD_ELEMENT quad[MAX_RAIDMAP_SPAN_DEPTH];
};
struct MR_LD_SPAN {
	__le64	 startBlk;
	__le64	 numBlks;
	__le16	 arrayRef;
	u8       spanRowSize;
	u8       spanRowDataSize;
	u8       reserved[4];
};
struct MR_SPAN_BLOCK_INFO {
	__le64          num_rows;
	struct MR_LD_SPAN   span;
	struct MR_SPAN_INFO block_span_info;
};
#define MR_RAID_CTX_CPUSEL_0		0
#define MR_RAID_CTX_CPUSEL_1		1
#define MR_RAID_CTX_CPUSEL_2		2
#define MR_RAID_CTX_CPUSEL_3		3
#define MR_RAID_CTX_CPUSEL_FCFS		0xF
struct MR_CPU_AFFINITY_MASK {
	union {
		struct {
#ifndef __BIG_ENDIAN_BITFIELD
		u8 hw_path:1;
		u8 cpu0:1;
		u8 cpu1:1;
		u8 cpu2:1;
		u8 cpu3:1;
		u8 reserved:3;
#else
		u8 reserved:3;
		u8 cpu3:1;
		u8 cpu2:1;
		u8 cpu1:1;
		u8 cpu0:1;
		u8 hw_path:1;
#endif
		};
		u8 core_mask;
	};
};
struct MR_IO_AFFINITY {
	union {
		struct {
			struct MR_CPU_AFFINITY_MASK pdRead;
			struct MR_CPU_AFFINITY_MASK pdWrite;
			struct MR_CPU_AFFINITY_MASK ldRead;
			struct MR_CPU_AFFINITY_MASK ldWrite;
			};
		u32 word;
		};
	u8 maxCores;     
	u8 reserved[3];
};
struct MR_LD_RAID {
	struct {
#if   defined(__BIG_ENDIAN_BITFIELD)
		u32 reserved4:2;
		u32 fp_cache_bypass_capable:1;
		u32 fp_rmw_capable:1;
		u32 disable_coalescing:1;
		u32     fpBypassRegionLock:1;
		u32     tmCapable:1;
		u32	fpNonRWCapable:1;
		u32     fpReadAcrossStripe:1;
		u32     fpWriteAcrossStripe:1;
		u32     fpReadCapable:1;
		u32     fpWriteCapable:1;
		u32     encryptionType:8;
		u32     pdPiMode:4;
		u32     ldPiMode:4;
		u32 reserved5:2;
		u32 ra_capable:1;
		u32     fpCapable:1;
#else
		u32     fpCapable:1;
		u32 ra_capable:1;
		u32 reserved5:2;
		u32     ldPiMode:4;
		u32     pdPiMode:4;
		u32     encryptionType:8;
		u32     fpWriteCapable:1;
		u32     fpReadCapable:1;
		u32     fpWriteAcrossStripe:1;
		u32     fpReadAcrossStripe:1;
		u32	fpNonRWCapable:1;
		u32     tmCapable:1;
		u32     fpBypassRegionLock:1;
		u32 disable_coalescing:1;
		u32 fp_rmw_capable:1;
		u32 fp_cache_bypass_capable:1;
		u32 reserved4:2;
#endif
	} capability;
	__le32     reserved6;
	__le64     size;
	u8      spanDepth;
	u8      level;
	u8      stripeShift;
	u8      rowSize;
	u8      rowDataSize;
	u8      writeMode;
	u8      PRL;
	u8      SRL;
	__le16     targetId;
	u8      ldState;
	u8      regTypeReqOnWrite;
	u8      modFactor;
	u8	regTypeReqOnRead;
	__le16     seqNum;
struct {
#ifndef __BIG_ENDIAN_BITFIELD
	u32 ldSyncRequired:1;
	u32 regTypeReqOnReadIsValid:1;
	u32 isEPD:1;
	u32 enableSLDOnAllRWIOs:1;
	u32 reserved:28;
#else
	u32 reserved:28;
	u32 enableSLDOnAllRWIOs:1;
	u32 isEPD:1;
	u32 regTypeReqOnReadIsValid:1;
	u32 ldSyncRequired:1;
#endif
	} flags;
	u8	LUN[8];  
	u8	fpIoTimeoutForLd; 
	u8 ld_accept_priority_type;
	u8 reserved2[2];	         
	u32 logical_block_length;
	struct {
#ifndef __BIG_ENDIAN_BITFIELD
	u32 ld_pi_exp:4;
	u32 ld_logical_block_exp:4;
	u32 reserved1:24;            
#else
	u32 reserved1:24;            
	u32 ld_logical_block_exp:4;
	u32 ld_pi_exp:4;
#endif
	};                                
	struct MR_IO_AFFINITY cpuAffinity;
	u8 reserved3[0x80 - 0x40];     
};
struct MR_LD_SPAN_MAP {
	struct MR_LD_RAID          ldRaid;
	u8                  dataArmMap[MAX_RAIDMAP_ROW_SIZE];
	struct MR_SPAN_BLOCK_INFO  spanBlock[MAX_RAIDMAP_SPAN_DEPTH];
};
struct MR_FW_RAID_MAP {
	__le32                 totalSize;
	union {
		struct {
			__le32         maxLd;
			__le32         maxSpanDepth;
			__le32         maxRowSize;
			__le32         maxPdCount;
			__le32         maxArrays;
		} validationInfo;
		__le32             version[5];
	};
	__le32                 ldCount;
	__le32                 Reserved1;
	u8                  ldTgtIdToLd[MAX_RAIDMAP_LOGICAL_DRIVES+
					MAX_RAIDMAP_VIEWS];
	u8                  fpPdIoTimeoutSec;
	u8                  reserved2[7];
	struct MR_ARRAY_INFO       arMapInfo[MAX_RAIDMAP_ARRAYS];
	struct MR_DEV_HANDLE_INFO  devHndlInfo[MAX_RAIDMAP_PHYSICAL_DEVICES];
	struct MR_LD_SPAN_MAP      ldSpanMap[];
};
struct IO_REQUEST_INFO {
	u64 ldStartBlock;
	u32 numBlocks;
	u16 ldTgtId;
	u8 isRead;
	__le16 devHandle;
	u8 pd_interface;
	u64 pdBlock;
	u8 fpOkForIo;
	u8 IoforUnevenSpan;
	u8 start_span;
	u8 do_fp_rlbypass;
	u64 start_row;
	u8  span_arm;	 
	u8  pd_after_lb;
	u16 r1_alt_dev_handle;  
	bool ra_capable;
	u8 data_arms;
};
struct MR_LD_TARGET_SYNC {
	u8  targetId;
	u8  reserved;
	__le16 seqNum;
};
enum MR_RAID_MAP_DESC_TYPE {
	RAID_MAP_DESC_TYPE_DEVHDL_INFO    = 0x0,
	RAID_MAP_DESC_TYPE_TGTID_INFO     = 0x1,
	RAID_MAP_DESC_TYPE_ARRAY_INFO     = 0x2,
	RAID_MAP_DESC_TYPE_SPAN_INFO      = 0x3,
	RAID_MAP_DESC_TYPE_COUNT,
};
struct MR_RAID_MAP_DESC_TABLE {
	u32 raid_map_desc_type;
	u32 raid_map_desc_offset;
	u32 raid_map_desc_buffer_size;
	u32 raid_map_desc_elements;
};
struct MR_FW_RAID_MAP_DYNAMIC {
	u32 raid_map_size;    
	u32 desc_table_offset; 
	u32 desc_table_size;   
	u32 desc_table_num_elements;
	u64	reserved1;
	u32	reserved2[3];	 
	u8 fp_pd_io_timeout_sec;
	u8 reserved3[3];
	u32 rmw_fp_seq_num;
	u16 ld_count;	 
	u16 ar_count;    
	u16 span_count;  
	u16 reserved4[3];
	union {
		struct {
			struct MR_DEV_HANDLE_INFO  *dev_hndl_info;
			u16 *ld_tgt_id_to_ld;
			struct MR_ARRAY_INFO *ar_map_info;
			struct MR_LD_SPAN_MAP *ld_span_map;
			};
		u64 ptr_structure_size[RAID_MAP_DESC_TYPE_COUNT];
		};
	struct MR_RAID_MAP_DESC_TABLE
			raid_map_desc_table[RAID_MAP_DESC_TYPE_COUNT];
	u32 raid_map_desc_data[];
};  
#define IEEE_SGE_FLAGS_ADDR_MASK            (0x03)
#define IEEE_SGE_FLAGS_SYSTEM_ADDR          (0x00)
#define IEEE_SGE_FLAGS_IOCDDR_ADDR          (0x01)
#define IEEE_SGE_FLAGS_IOCPLB_ADDR          (0x02)
#define IEEE_SGE_FLAGS_IOCPLBNTA_ADDR       (0x03)
#define IEEE_SGE_FLAGS_CHAIN_ELEMENT        (0x80)
#define IEEE_SGE_FLAGS_END_OF_LIST          (0x40)
#define MPI2_SGE_FLAGS_SHIFT                (0x02)
#define IEEE_SGE_FLAGS_FORMAT_MASK          (0xC0)
#define IEEE_SGE_FLAGS_FORMAT_IEEE          (0x00)
#define IEEE_SGE_FLAGS_FORMAT_NVME          (0x02)
#define MPI26_IEEE_SGE_FLAGS_NSF_MASK           (0x1C)
#define MPI26_IEEE_SGE_FLAGS_NSF_MPI_IEEE       (0x00)
#define MPI26_IEEE_SGE_FLAGS_NSF_NVME_PRP       (0x08)
#define MPI26_IEEE_SGE_FLAGS_NSF_NVME_SGL       (0x10)
#define MEGASAS_DEFAULT_SNAP_DUMP_WAIT_TIME 15
#define MEGASAS_MAX_SNAP_DUMP_WAIT_TIME 60
struct megasas_register_set;
struct megasas_instance;
union desc_word {
	u64 word;
	struct {
		u32 low;
		u32 high;
	} u;
};
struct megasas_cmd_fusion {
	struct MPI2_RAID_SCSI_IO_REQUEST	*io_request;
	dma_addr_t			io_request_phys_addr;
	union MPI2_SGE_IO_UNION	*sg_frame;
	dma_addr_t		sg_frame_phys_addr;
	u8 *sense;
	dma_addr_t sense_phys_addr;
	struct list_head list;
	struct scsi_cmnd *scmd;
	struct megasas_instance *instance;
	u8 retry_for_fw_reset;
	union MEGASAS_REQUEST_DESCRIPTOR_UNION  *request_desc;
	u32 sync_cmd_idx;
	u32 index;
	u8 pd_r1_lb;
	struct completion done;
	u8 pd_interface;
	u16 r1_alt_dev_handle;  
	bool cmd_completed;   
};
struct LD_LOAD_BALANCE_INFO {
	u8	loadBalanceFlag;
	u8	reserved1;
	atomic_t     scsi_pending_cmds[MAX_PHYSICAL_DEVICES];
	u64     last_accessed_block[MAX_PHYSICAL_DEVICES];
};
typedef struct _LD_SPAN_SET {
	u64  log_start_lba;
	u64  log_end_lba;
	u64  span_row_start;
	u64  span_row_end;
	u64  data_strip_start;
	u64  data_strip_end;
	u64  data_row_start;
	u64  data_row_end;
	u8   strip_offset[MAX_SPAN_DEPTH];
	u32    span_row_data_width;
	u32    diff;
	u32    reserved[2];
} LD_SPAN_SET, *PLD_SPAN_SET;
typedef struct LOG_BLOCK_SPAN_INFO {
	LD_SPAN_SET  span_set[MAX_SPAN_DEPTH];
} LD_SPAN_INFO, *PLD_SPAN_INFO;
struct MR_FW_RAID_MAP_ALL {
	struct MR_FW_RAID_MAP raidMap;
	struct MR_LD_SPAN_MAP ldSpanMap[MAX_LOGICAL_DRIVES];
} __attribute__ ((packed));
struct MR_DRV_RAID_MAP {
	__le32                 totalSize;
	union {
	struct {
		__le32         maxLd;
		__le32         maxSpanDepth;
		__le32         maxRowSize;
		__le32         maxPdCount;
		__le32         maxArrays;
	} validationInfo;
	__le32             version[5];
	};
	u8                  fpPdIoTimeoutSec;
	u8                  reserved2[7];
	__le16                 ldCount;
	__le16                 arCount;
	__le16                 spanCount;
	__le16                 reserve3;
	struct MR_DEV_HANDLE_INFO
		devHndlInfo[MAX_RAIDMAP_PHYSICAL_DEVICES_DYN];
	u16 ldTgtIdToLd[MAX_LOGICAL_DRIVES_DYN];
	struct MR_ARRAY_INFO arMapInfo[MAX_API_ARRAYS_DYN];
	struct MR_LD_SPAN_MAP      ldSpanMap[];
};
struct MR_DRV_RAID_MAP_ALL {
	struct MR_DRV_RAID_MAP raidMap;
	struct MR_LD_SPAN_MAP ldSpanMap[MAX_LOGICAL_DRIVES_DYN];
} __packed;
struct MR_FW_RAID_MAP_EXT {
	u32                 reserved;
	union {
	struct {
		u32         maxLd;
		u32         maxSpanDepth;
		u32         maxRowSize;
		u32         maxPdCount;
		u32         maxArrays;
	} validationInfo;
	u32             version[5];
	};
	u8                  fpPdIoTimeoutSec;
	u8                  reserved2[7];
	__le16                 ldCount;
	__le16                 arCount;
	__le16                 spanCount;
	__le16                 reserve3;
	struct MR_DEV_HANDLE_INFO  devHndlInfo[MAX_RAIDMAP_PHYSICAL_DEVICES];
	u8                  ldTgtIdToLd[MAX_LOGICAL_DRIVES_EXT];
	struct MR_ARRAY_INFO       arMapInfo[MAX_API_ARRAYS_EXT];
	struct MR_LD_SPAN_MAP      ldSpanMap[MAX_LOGICAL_DRIVES_EXT];
};
struct MR_PD_CFG_SEQ {
	u16 seqNum;
	u16 devHandle;
	struct {
#if   defined(__BIG_ENDIAN_BITFIELD)
		u8     reserved:7;
		u8     tmCapable:1;
#else
		u8     tmCapable:1;
		u8     reserved:7;
#endif
	} capability;
	u8  reserved;
	u16 pd_target_id;
} __packed;
struct MR_PD_CFG_SEQ_NUM_SYNC {
	__le32 size;
	__le32 count;
	struct MR_PD_CFG_SEQ seq[];
} __packed;
struct STREAM_DETECT {
	u64 next_seq_lba;  
	struct megasas_cmd_fusion *first_cmd_fusion;  
	struct megasas_cmd_fusion *last_cmd_fusion;  
	u32 count_cmds_in_stream;  
	u16 num_sges_in_group;  
	u8 is_read;  
	u8 group_depth;  
	bool group_flush;
	u8 reserved[7];  
};
struct LD_STREAM_DETECT {
	bool write_back;  
	bool fp_write_enabled;
	bool members_ssds;
	bool fp_cache_bypass_capable;
	u32 mru_bit_map;  
	struct STREAM_DETECT stream_track[MAX_STREAMS_TRACKED];
};
struct MPI2_IOC_INIT_RDPQ_ARRAY_ENTRY {
	u64 RDPQBaseAddress;
	u32 Reserved1;
	u32 Reserved2;
};
struct rdpq_alloc_detail {
	struct dma_pool *dma_pool_ptr;
	dma_addr_t	pool_entry_phys;
	union MPI2_REPLY_DESCRIPTORS_UNION *pool_entry_virt;
};
struct fusion_context {
	struct megasas_cmd_fusion **cmd_list;
	dma_addr_t req_frames_desc_phys;
	u8 *req_frames_desc;
	struct dma_pool *io_request_frames_pool;
	dma_addr_t io_request_frames_phys;
	u8 *io_request_frames;
	struct dma_pool *sg_dma_pool;
	struct dma_pool *sense_dma_pool;
	u8 *sense;
	dma_addr_t sense_phys_addr;
	atomic_t   busy_mq_poll[MAX_MSIX_QUEUES_FUSION];
	dma_addr_t reply_frames_desc_phys[MAX_MSIX_QUEUES_FUSION];
	union MPI2_REPLY_DESCRIPTORS_UNION *reply_frames_desc[MAX_MSIX_QUEUES_FUSION];
	struct rdpq_alloc_detail rdpq_tracker[RDPQ_MAX_CHUNK_COUNT];
	struct dma_pool *reply_frames_desc_pool;
	struct dma_pool *reply_frames_desc_pool_align;
	u16 last_reply_idx[MAX_MSIX_QUEUES_FUSION];
	u32 reply_q_depth;
	u32 request_alloc_sz;
	u32 reply_alloc_sz;
	u32 io_frames_alloc_sz;
	struct MPI2_IOC_INIT_RDPQ_ARRAY_ENTRY *rdpq_virt;
	dma_addr_t rdpq_phys;
	u16	max_sge_in_main_msg;
	u16	max_sge_in_chain;
	u8	chain_offset_io_request;
	u8	chain_offset_mfi_pthru;
	struct MR_FW_RAID_MAP_DYNAMIC *ld_map[2];
	dma_addr_t ld_map_phys[2];
	struct MR_DRV_RAID_MAP_ALL *ld_drv_map[2];
	u32 max_map_sz;
	u32 current_map_sz;
	u32 old_map_sz;
	u32 new_map_sz;
	u32 drv_map_sz;
	u32 drv_map_pages;
	struct MR_PD_CFG_SEQ_NUM_SYNC	*pd_seq_sync[JBOD_MAPS_COUNT];
	dma_addr_t pd_seq_phys[JBOD_MAPS_COUNT];
	u8 fast_path_io;
	struct LD_LOAD_BALANCE_INFO *load_balance_info;
	u32 load_balance_info_pages;
	LD_SPAN_INFO *log_to_span;
	u32 log_to_span_pages;
	struct LD_STREAM_DETECT **stream_detect_by_ld;
	dma_addr_t ioc_init_request_phys;
	struct MPI2_IOC_INIT_REQUEST *ioc_init_request;
	struct megasas_cmd *ioc_init_cmd;
	bool pcie_bw_limitation;
	bool r56_div_offload;
};
union desc_value {
	__le64 word;
	struct {
		__le32 low;
		__le32 high;
	} u;
};
enum CMD_RET_VALUES {
	REFIRE_CMD = 1,
	COMPLETE_CMD = 2,
	RETURN_CMD = 3,
};
struct  MR_SNAPDUMP_PROPERTIES {
	u8       offload_num;
	u8       max_num_supported;
	u8       cur_num_supported;
	u8       trigger_min_num_sec_before_ocr;
	u8       reserved[12];
};
struct megasas_debugfs_buffer {
	void *buf;
	u32 len;
};
void megasas_free_cmds_fusion(struct megasas_instance *instance);
int megasas_ioc_init_fusion(struct megasas_instance *instance);
u8 megasas_get_map_info(struct megasas_instance *instance);
int megasas_sync_map_info(struct megasas_instance *instance);
void megasas_release_fusion(struct megasas_instance *instance);
void megasas_reset_reply_desc(struct megasas_instance *instance);
int megasas_check_mpio_paths(struct megasas_instance *instance,
			      struct scsi_cmnd *scmd);
void megasas_fusion_ocr_wq(struct work_struct *work);
#endif  
