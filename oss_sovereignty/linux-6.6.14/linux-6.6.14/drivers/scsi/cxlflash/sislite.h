#ifndef _SISLITE_H
#define _SISLITE_H
#include <linux/types.h>
typedef u16 ctx_hndl_t;
typedef u32 res_hndl_t;
#define SIZE_4K		4096
#define SIZE_64K	65536
struct sisl_ioarcb {
	u16 ctx_id;		 
	u16 req_flags;
#define SISL_REQ_FLAGS_RES_HNDL       0x8000U	 
#define SISL_REQ_FLAGS_PORT_LUN_ID    0x0000U
#define SISL_REQ_FLAGS_SUP_UNDERRUN   0x4000U	 
#define SISL_REQ_FLAGS_TIMEOUT_SECS   0x0000U	 
#define SISL_REQ_FLAGS_TIMEOUT_MSECS  0x0040U
#define SISL_REQ_FLAGS_TIMEOUT_USECS  0x0080U
#define SISL_REQ_FLAGS_TIMEOUT_CYCLES 0x00C0U
#define SISL_REQ_FLAGS_TMF_CMD        0x0004u	 
#define SISL_REQ_FLAGS_AFU_CMD        0x0002U	 
#define SISL_REQ_FLAGS_HOST_WRITE     0x0001U	 
#define SISL_REQ_FLAGS_HOST_READ      0x0000U
	union {
		u32 res_hndl;	 
		u32 port_sel;	 
	};
	u64 lun_id;
	u32 data_len;		 
	u32 ioadl_len;
	union {
		u64 data_ea;	 
		u64 ioadl_ea;
	};
	u8 msi;			 
#define SISL_MSI_CXL_PFAULT        0	 
#define SISL_MSI_SYNC_ERROR        1	 
#define SISL_MSI_RRQ_UPDATED       2	 
#define SISL_MSI_ASYNC_ERROR       3	 
	u8 rrq;			 
	u16 timeout;		 
	u32 rsvd1;
	u8 cdb[16];		 
#define SISL_AFU_CMD_SYNC		0xC0	 
#define SISL_AFU_CMD_LUN_PROVISION	0xD0	 
#define SISL_AFU_CMD_DEBUG		0xE0	 
#define SISL_AFU_LUN_PROVISION_CREATE	0x00	 
#define SISL_AFU_LUN_PROVISION_DELETE	0x01	 
	union {
		u64 reserved;			 
		struct sisl_ioasa *ioasa;	 
	};
} __packed;
struct sisl_rc {
	u8 flags;
#define SISL_RC_FLAGS_SENSE_VALID         0x80U
#define SISL_RC_FLAGS_FCP_RSP_CODE_VALID  0x40U
#define SISL_RC_FLAGS_OVERRUN             0x20U
#define SISL_RC_FLAGS_UNDERRUN            0x10U
	u8 afu_rc;
#define SISL_AFU_RC_RHT_INVALID           0x01U	 
#define SISL_AFU_RC_RHT_UNALIGNED         0x02U	 
#define SISL_AFU_RC_RHT_OUT_OF_BOUNDS     0x03u	 
#define SISL_AFU_RC_RHT_DMA_ERR           0x04u	 
#define SISL_AFU_RC_RHT_RW_PERM           0x05u	 
#define SISL_AFU_RC_LXT_UNALIGNED         0x12U	 
#define SISL_AFU_RC_LXT_OUT_OF_BOUNDS     0x13u	 
#define SISL_AFU_RC_LXT_DMA_ERR           0x14u	 
#define SISL_AFU_RC_LXT_RW_PERM           0x15u	 
#define SISL_AFU_RC_NOT_XLATE_HOST        0x1au	 
#define SISL_AFU_RC_NO_CHANNELS           0x20U	 
#define SISL_AFU_RC_CAP_VIOLATION         0x21U	 
#define SISL_AFU_RC_OUT_OF_DATA_BUFS      0x30U	 
#define SISL_AFU_RC_DATA_DMA_ERR          0x31U	 
	u8 scsi_rc;		 
#define SISL_SCSI_RC_CHECK                0x02U
#define SISL_SCSI_RC_BUSY                 0x08u
	u8 fc_rc;		 
#define SISL_FC_RC_ABORTPEND	0x52	 
#define SISL_FC_RC_WRABORTPEND	0x53	 
#define SISL_FC_RC_NOLOGI	0x54	 
#define SISL_FC_RC_NOEXP	0x55	 
#define SISL_FC_RC_INUSE	0x56	 
#define SISL_FC_RC_LINKDOWN	0x57	 
#define SISL_FC_RC_ABORTOK	0x58	 
#define SISL_FC_RC_ABORTFAIL	0x59	 
#define SISL_FC_RC_RESID	0x5A	 
#define SISL_FC_RC_RESIDERR	0x5B	 
#define SISL_FC_RC_TGTABORT	0x5C	 
};
#define SISL_SENSE_DATA_LEN     20	 
#define SISL_WWID_DATA_LEN	16	 
struct sisl_ioasa {
	union {
		struct sisl_rc rc;
		u32 ioasc;
#define SISL_IOASC_GOOD_COMPLETION        0x00000000U
	};
	union {
		u32 resid;
		u32 lunid_hi;
	};
	u8 port;
	u8 afu_extra;
#define SISL_AFU_DMA_ERR_PAGE_IN	0x0A	 
#define SISL_AFU_DMA_ERR_INVALID_EA	0x0B	 
#define SISL_AFU_NO_CLANNELS_AMASK(afu_extra) (((afu_extra) & 0x0C) >> 2)
#define SISL_AFU_NO_CLANNELS_RMASK(afu_extra) ((afu_extra) & 0x03)
	u8 scsi_extra;
	u8 fc_extra;
	union {
		u8 sense_data[SISL_SENSE_DATA_LEN];
		struct {
			u32 lunid_lo;
			u8 wwid[SISL_WWID_DATA_LEN];
		};
	};
	union {
		u64 host_use[4];
		u8 host_use_b[32];
	};
} __packed;
#define SISL_RESP_HANDLE_T_BIT        0x1ULL	 
#define SISL_ENDIAN_CTRL_BE           0x8000000000000080ULL
#define SISL_ENDIAN_CTRL_LE           0x0000000000000000ULL
#ifdef __BIG_ENDIAN
#define SISL_ENDIAN_CTRL              SISL_ENDIAN_CTRL_BE
#else
#define SISL_ENDIAN_CTRL              SISL_ENDIAN_CTRL_LE
#endif
struct sisl_host_map {
	__be64 endian_ctrl;	 
	__be64 intr_status;	 
#define SISL_ISTATUS_PERM_ERR_LISN_3_EA		0x0400ULL  
#define SISL_ISTATUS_PERM_ERR_LISN_2_EA		0x0200ULL  
#define SISL_ISTATUS_PERM_ERR_LISN_1_EA		0x0100ULL  
#define SISL_ISTATUS_PERM_ERR_LISN_3_PASID	0x0080ULL  
#define SISL_ISTATUS_PERM_ERR_LISN_2_PASID	0x0040ULL  
#define SISL_ISTATUS_PERM_ERR_LISN_1_PASID	0x0020ULL  
#define SISL_ISTATUS_PERM_ERR_CMDROOM		0x0010ULL  
#define SISL_ISTATUS_PERM_ERR_RCB_READ		0x0008ULL  
#define SISL_ISTATUS_PERM_ERR_SA_WRITE		0x0004ULL  
#define SISL_ISTATUS_PERM_ERR_RRQ_WRITE		0x0002ULL  
#define SISL_ISTATUS_TEMP_ERR_PAGEIN		0x0001ULL  
#define SISL_ISTATUS_UNMASK	(0x07FFULL)		 
#define SISL_ISTATUS_MASK	~(SISL_ISTATUS_UNMASK)	 
	__be64 intr_clear;
	__be64 intr_mask;
	__be64 ioarrin;		 
	__be64 rrq_start;	 
	__be64 rrq_end;		 
	__be64 cmd_room;
	__be64 ctx_ctrl;	 
#define SISL_CTX_CTRL_UNMAP_SECTOR	0x8000000000000000ULL  
#define SISL_CTX_CTRL_LISN_MASK		(0xFFULL)
	__be64 mbox_w;		 
	__be64 sq_start;	 
	__be64 sq_end;		 
	__be64 sq_head;		 
	__be64 sq_tail;		 
	__be64 sq_ctx_reset;	 
};
struct sisl_ctrl_map {
	__be64 rht_start;
	__be64 rht_cnt_id;
#define SISL_RHT_CNT_ID(cnt, ctx_id)  (((cnt) << 48) | ((ctx_id) << 32))
	__be64 ctx_cap;	 
#define SISL_CTX_CAP_PROXY_ISSUE       0x8000000000000000ULL  
#define SISL_CTX_CAP_REAL_MODE         0x4000000000000000ULL  
#define SISL_CTX_CAP_HOST_XLATE        0x2000000000000000ULL  
#define SISL_CTX_CAP_PROXY_TARGET      0x1000000000000000ULL  
#define SISL_CTX_CAP_AFU_CMD           0x0000000000000008ULL  
#define SISL_CTX_CAP_GSCSI_CMD         0x0000000000000004ULL  
#define SISL_CTX_CAP_WRITE_CMD         0x0000000000000002ULL  
#define SISL_CTX_CAP_READ_CMD          0x0000000000000001ULL  
	__be64 mbox_r;
	__be64 lisn_pasid[2];
#define SISL_LISN_PASID(_a, _b)	(((_a) << 32) | (_b))
	__be64 lisn_ea[3];
};
struct sisl_global_regs {
	__be64 aintr_status;
#define SISL_ASTATUS_FC2_OTHER	 0x80000000ULL  
#define SISL_ASTATUS_FC2_LOGO    0x40000000ULL  
#define SISL_ASTATUS_FC2_CRC_T   0x20000000ULL  
#define SISL_ASTATUS_FC2_LOGI_R  0x10000000ULL  
#define SISL_ASTATUS_FC2_LOGI_F  0x08000000ULL  
#define SISL_ASTATUS_FC2_LOGI_S  0x04000000ULL  
#define SISL_ASTATUS_FC2_LINK_DN 0x02000000ULL  
#define SISL_ASTATUS_FC2_LINK_UP 0x01000000ULL  
#define SISL_ASTATUS_FC3_OTHER   0x00800000ULL  
#define SISL_ASTATUS_FC3_LOGO    0x00400000ULL  
#define SISL_ASTATUS_FC3_CRC_T   0x00200000ULL  
#define SISL_ASTATUS_FC3_LOGI_R  0x00100000ULL  
#define SISL_ASTATUS_FC3_LOGI_F  0x00080000ULL  
#define SISL_ASTATUS_FC3_LOGI_S  0x00040000ULL  
#define SISL_ASTATUS_FC3_LINK_DN 0x00020000ULL  
#define SISL_ASTATUS_FC3_LINK_UP 0x00010000ULL  
#define SISL_ASTATUS_FC0_OTHER	 0x00008000ULL  
#define SISL_ASTATUS_FC0_LOGO    0x00004000ULL  
#define SISL_ASTATUS_FC0_CRC_T   0x00002000ULL  
#define SISL_ASTATUS_FC0_LOGI_R  0x00001000ULL  
#define SISL_ASTATUS_FC0_LOGI_F  0x00000800ULL  
#define SISL_ASTATUS_FC0_LOGI_S  0x00000400ULL  
#define SISL_ASTATUS_FC0_LINK_DN 0x00000200ULL  
#define SISL_ASTATUS_FC0_LINK_UP 0x00000100ULL  
#define SISL_ASTATUS_FC1_OTHER   0x00000080ULL  
#define SISL_ASTATUS_FC1_LOGO    0x00000040ULL  
#define SISL_ASTATUS_FC1_CRC_T   0x00000020ULL  
#define SISL_ASTATUS_FC1_LOGI_R  0x00000010ULL  
#define SISL_ASTATUS_FC1_LOGI_F  0x00000008ULL  
#define SISL_ASTATUS_FC1_LOGI_S  0x00000004ULL  
#define SISL_ASTATUS_FC1_LINK_DN 0x00000002ULL  
#define SISL_ASTATUS_FC1_LINK_UP 0x00000001ULL  
#define SISL_FC_INTERNAL_UNMASK	0x0000000300000000ULL	 
#define SISL_FC_INTERNAL_MASK	~(SISL_FC_INTERNAL_UNMASK)
#define SISL_FC_INTERNAL_SHIFT	32
#define SISL_FC_SHUTDOWN_NORMAL		0x0000000000000010ULL
#define SISL_FC_SHUTDOWN_ABRUPT		0x0000000000000020ULL
#define SISL_STATUS_SHUTDOWN_ACTIVE	0x0000000000000010ULL
#define SISL_STATUS_SHUTDOWN_COMPLETE	0x0000000000000020ULL
#define SISL_ASTATUS_UNMASK	0xFFFFFFFFULL		 
#define SISL_ASTATUS_MASK	~(SISL_ASTATUS_UNMASK)	 
	__be64 aintr_clear;
	__be64 aintr_mask;
	__be64 afu_ctrl;
	__be64 afu_hb;
	__be64 afu_scratch_pad;
	__be64 afu_port_sel;
#define SISL_AFUCONF_AR_IOARCB	0x4000ULL
#define SISL_AFUCONF_AR_LXT	0x2000ULL
#define SISL_AFUCONF_AR_RHT	0x1000ULL
#define SISL_AFUCONF_AR_DATA	0x0800ULL
#define SISL_AFUCONF_AR_RSRC	0x0400ULL
#define SISL_AFUCONF_AR_IOASA	0x0200ULL
#define SISL_AFUCONF_AR_RRQ	0x0100ULL
#define SISL_AFUCONF_AR_ALL	(SISL_AFUCONF_AR_IOARCB|SISL_AFUCONF_AR_LXT| \
				 SISL_AFUCONF_AR_RHT|SISL_AFUCONF_AR_DATA|   \
				 SISL_AFUCONF_AR_RSRC|SISL_AFUCONF_AR_IOASA| \
				 SISL_AFUCONF_AR_RRQ)
#ifdef __BIG_ENDIAN
#define SISL_AFUCONF_ENDIAN            0x0000ULL
#else
#define SISL_AFUCONF_ENDIAN            0x0020ULL
#endif
#define SISL_AFUCONF_MBOX_CLR_READ     0x0010ULL
	__be64 afu_config;
	__be64 rsvd[0xf8];
	__le64 afu_version;
	__be64 interface_version;
#define SISL_INTVER_CAP_SHIFT			16
#define SISL_INTVER_MAJ_SHIFT			8
#define SISL_INTVER_CAP_MASK			0xFFFFFFFF00000000ULL
#define SISL_INTVER_MAJ_MASK			0x00000000FFFF0000ULL
#define SISL_INTVER_MIN_MASK			0x000000000000FFFFULL
#define SISL_INTVER_CAP_IOARRIN_CMD_MODE	0x800000000000ULL
#define SISL_INTVER_CAP_SQ_CMD_MODE		0x400000000000ULL
#define SISL_INTVER_CAP_RESERVED_CMD_MODE_A	0x200000000000ULL
#define SISL_INTVER_CAP_RESERVED_CMD_MODE_B	0x100000000000ULL
#define SISL_INTVER_CAP_LUN_PROVISION		0x080000000000ULL
#define SISL_INTVER_CAP_AFU_DEBUG		0x040000000000ULL
#define SISL_INTVER_CAP_OCXL_LISN		0x020000000000ULL
};
#define CXLFLASH_NUM_FC_PORTS_PER_BANK	2	 
#define CXLFLASH_MAX_FC_BANKS		2	 
#define CXLFLASH_MAX_FC_PORTS	(CXLFLASH_NUM_FC_PORTS_PER_BANK *	\
				 CXLFLASH_MAX_FC_BANKS)
#define CXLFLASH_MAX_CONTEXT	512	 
#define CXLFLASH_NUM_VLUNS	512	 
#define CXLFLASH_NUM_REGS	512	 
struct fc_port_bank {
	__be64 fc_port_regs[CXLFLASH_NUM_FC_PORTS_PER_BANK][CXLFLASH_NUM_REGS];
	__be64 fc_port_luns[CXLFLASH_NUM_FC_PORTS_PER_BANK][CXLFLASH_NUM_VLUNS];
};
struct sisl_global_map {
	union {
		struct sisl_global_regs regs;
		char page0[SIZE_4K];	 
	};
	char page1[SIZE_4K];	 
	struct fc_port_bank bank[CXLFLASH_MAX_FC_BANKS];  
};
struct cxlflash_afu_map {
	union {
		struct sisl_host_map host;
		char harea[SIZE_64K];	 
	} hosts[CXLFLASH_MAX_CONTEXT];
	union {
		struct sisl_ctrl_map ctrl;
		char carea[cache_line_size()];	 
	} ctrls[CXLFLASH_MAX_CONTEXT];
	union {
		struct sisl_global_map global;
		char garea[SIZE_64K];	 
	};
};
struct sisl_lxt_entry {
	u64 rlba_base;	 
};
struct sisl_rht_entry {
	struct sisl_lxt_entry *lxt_start;
	u32 lxt_cnt;
	u16 rsvd;
	u8 fp;			 
	u8 nmask;
} __packed __aligned(16);
struct sisl_rht_entry_f1 {
	u64 lun_id;
	union {
		struct {
			u8 valid;
			u8 rsvd[5];
			u8 fp;
			u8 port_sel;
		};
		u64 dw;
	};
} __packed __aligned(16);
#define SISL_RHT_FP(fmt, perm) (((fmt) << 4) | (perm))
#define SISL_RHT_FP_CLONE(src_fp, cln_flags) ((src_fp) & (0xFC | (cln_flags)))
#define RHT_PERM_READ  0x01U
#define RHT_PERM_WRITE 0x02U
#define RHT_PERM_RW    (RHT_PERM_READ | RHT_PERM_WRITE)
#define SISL_RHT_PERM(fp) ((fp) & RHT_PERM_RW)
#define PORT0  0x01U
#define PORT1  0x02U
#define PORT2  0x04U
#define PORT3  0x08U
#define PORT_MASK(_n)	((1 << (_n)) - 1)
#define AFU_LW_SYNC 0x0U
#define AFU_HW_SYNC 0x1U
#define AFU_GSYNC   0x2U
#define TMF_LUN_RESET  0x1U
#define TMF_CLEAR_ACA  0x2U
#endif  
