


#ifndef __SNIC_FWINT_H
#define __SNIC_FWINT_H

#define SNIC_CDB_LEN	32	
#define LUN_ADDR_LEN	8


enum snic_io_type {
	
	SNIC_REQ_REPORT_TGTS = 0x2,	
	SNIC_REQ_ICMND,			
	SNIC_REQ_ITMF,			
	SNIC_REQ_HBA_RESET,		
	SNIC_REQ_EXCH_VER,		
	SNIC_REQ_TGT_INFO,		
	SNIC_REQ_BOOT_LUNS,

	
	SNIC_RSP_REPORT_TGTS_CMPL = 0x12,
	SNIC_RSP_ICMND_CMPL,		
	SNIC_RSP_ITMF_CMPL,		
	SNIC_RSP_HBA_RESET_CMPL,	
	SNIC_RSP_EXCH_VER_CMPL,		
	SNIC_RSP_BOOT_LUNS_CMPL,

	
	SNIC_MSG_ACK = 0x80,		
	SNIC_MSG_ASYNC_EVNOTIFY,	
}; 



enum snic_io_status {
	SNIC_STAT_IO_SUCCESS = 0,	

	
	SNIC_STAT_INVALID_HDR,	
	SNIC_STAT_OUT_OF_RES,	
	SNIC_STAT_INVALID_PARM,	
	SNIC_STAT_REQ_NOT_SUP,	
	SNIC_STAT_IO_NOT_FOUND,	

	
	SNIC_STAT_ABORTED,		
	SNIC_STAT_TIMEOUT,		
	SNIC_STAT_SGL_INVALID,		
	SNIC_STAT_DATA_CNT_MISMATCH,	
	SNIC_STAT_FW_ERR,		
	SNIC_STAT_ITMF_REJECT,		
	SNIC_STAT_ITMF_FAIL,		
	SNIC_STAT_ITMF_INCORRECT_LUN,	
	SNIC_STAT_CMND_REJECT,		
	SNIC_STAT_DEV_OFFLINE,		
	SNIC_STAT_NO_BOOTLUN,
	SNIC_STAT_SCSI_ERR,		
	SNIC_STAT_NOT_READY,		
	SNIC_STAT_FATAL_ERROR,		
}; 


struct snic_io_hdr {
	__le32	hid;
	__le32	cmnd_id;	
	ulong	init_ctx;	
	u8	type;		
	u8	status;		
	u8	protocol;	
	u8	flags;
	__le16	sg_cnt;
	u16	resvd;
};


static inline void
snic_io_hdr_enc(struct snic_io_hdr *hdr, u8 typ, u8 status, u32 id, u32 hid,
		u16 sg_cnt, ulong ctx)
{
	hdr->type = typ;
	hdr->status = status;
	hdr->protocol = 0;
	hdr->hid = cpu_to_le32(hid);
	hdr->cmnd_id = cpu_to_le32(id);
	hdr->sg_cnt = cpu_to_le16(sg_cnt);
	hdr->init_ctx = ctx;
	hdr->flags = 0;
}


static inline void
snic_io_hdr_dec(struct snic_io_hdr *hdr, u8 *typ, u8 *stat, u32 *cmnd_id,
		u32 *hid, ulong *ctx)
{
	*typ = hdr->type;
	*stat = hdr->status;
	*hid = le32_to_cpu(hdr->hid);
	*cmnd_id = le32_to_cpu(hdr->cmnd_id);
	*ctx = hdr->init_ctx;
}


struct snic_exch_ver_req {
	__le32	drvr_ver;	
	__le32	os_type;	
};


#define SNIC_OS_LINUX	0x1
#define SNIC_OS_WIN	0x2
#define SNIC_OS_ESX	0x3


#define SNIC_HBA_CAP_DDL	0x02	
#define SNIC_HBA_CAP_AEN	0x04	
#define SNIC_HBA_CAP_TMO	0x08	


struct snic_exch_ver_rsp {
	__le32	version;
	__le32	hid;
	__le32	max_concur_ios;		
	__le32	max_sgs_per_cmd;	
	__le32	max_io_sz;		
	__le32	hba_cap;		
	__le32	max_tgts;		
	__le16	io_timeout;		
	u16	rsvd;
};



struct snic_report_tgts {
	__le16	sg_cnt;
	__le16	flags;		
	u8	_resvd[4];
	__le64	sg_addr;	
	__le64	sense_addr;
};

enum snic_type {
	SNIC_NONE = 0x0,
	SNIC_DAS,
	SNIC_SAN,
};



enum snic_tgt_type {
	SNIC_TGT_NONE = 0x0,
	SNIC_TGT_DAS,	
	SNIC_TGT_SAN,	
};


struct snic_tgt_id {
	__le32	tgt_id;		
	__le16	tgt_type;	
	__le16	vnic_id;	
};


struct snic_report_tgts_cmpl {
	__le32	tgt_cnt;	
	u32	_resvd;
};



#define SNIC_ICMND_WR		0x01	
#define SNIC_ICMND_RD		0x02	
#define SNIC_ICMND_ESGL		0x04	


#define SNIC_ICMND_TSK_SHIFT		2	
#define SNIC_ICMND_TSK_MASK(x)		((x>>SNIC_ICMND_TSK_SHIFT) & ~(0xffff))
#define SNIC_ICMND_TSK_SIMPLE		0	
#define SNIC_ICMND_TSK_HEAD_OF_QUEUE	1	
#define SNIC_ICMND_TSK_ORDERED		2	

#define SNIC_ICMND_PRI_SHIFT		5	


struct snic_icmnd {
	__le16	sg_cnt;		
	__le16	flags;		
	__le32	sense_len;	
	__le64	tgt_id;		
	__le64	lun_id;		
	u8	cdb_len;
	u8	_resvd;
	__le16	time_out;	
	__le32	data_len;	
	u8	cdb[SNIC_CDB_LEN];
	__le64	sg_addr;	
	__le64	sense_addr;	
};




#define SNIC_ICMND_CMPL_UNDR_RUN	0x01	
#define SNIC_ICMND_CMPL_OVER_RUN	0x02	


struct snic_icmnd_cmpl {
	u8	scsi_status;	
	u8	flags;
	__le16	sense_len;	
	__le32	resid;		
};


struct snic_itmf {
	u8	tm_type;	
	u8	resvd;
	__le16	flags;		
	__le32	req_id;		
	__le64	tgt_id;		
	__le64	lun_id;		
	__le16	timeout;	
};


enum snic_itmf_tm_type {
	SNIC_ITMF_ABTS_TASK = 0x01,	
	SNIC_ITMF_ABTS_TASK_SET,	
	SNIC_ITMF_CLR_TASK,		
	SNIC_ITMF_CLR_TASKSET,		
	SNIC_ITMF_LUN_RESET,		
	SNIC_ITMF_ABTS_TASK_TERM,	
};


struct snic_itmf_cmpl {
	__le32	nterminated;	
	u8	flags;		
	u8	_resvd[3];
};


#define SNIC_NUM_TERM_VALID	0x01	


struct snic_hba_reset {
	__le16	flags;		
	u8	_resvd[6];
};


struct snic_hba_reset_cmpl {
	u8	flags;		
	u8	_resvd[7];
};


struct snic_notify_msg {
	__le32	wqe_num;	
	u8	flags;		
	u8	_resvd[4];
};


#define SNIC_EVDATA_LEN		24	

struct snic_async_evnotify {
	u8	FLS_EVENT_DESC;
	u8	vnic;			
	u8	_resvd[2];
	__le32	ev_id;			
	u8	ev_data[SNIC_EVDATA_LEN]; 
	u8	_resvd2[4];
};


enum snic_ev_type {
	SNIC_EV_TGT_OFFLINE = 0x01, 
	SNIC_EV_TGT_ONLINE,	
	SNIC_EV_LUN_OFFLINE,	
	SNIC_EV_LUN_ONLINE,	
	SNIC_EV_CONF_CHG,	
	SNIC_EV_TGT_ADDED,	
	SNIC_EV_TGT_DELTD,	
	SNIC_EV_LUN_ADDED,	
	SNIC_EV_LUN_DELTD,	

	SNIC_EV_DISC_CMPL = 0x10, 
};


#define SNIC_HOST_REQ_LEN	128	

#define SNIC_HOST_REQ_PAYLOAD	((int)(SNIC_HOST_REQ_LEN -		\
					sizeof(struct snic_io_hdr) -	\
					(2 * sizeof(u64)) - sizeof(ulong)))


struct snic_host_req {
	u64	ctrl_data[2];	
	struct snic_io_hdr hdr;
	union {
		
		u8	buf[SNIC_HOST_REQ_PAYLOAD];

		
		struct snic_exch_ver_req	exch_ver;

		
		struct snic_report_tgts		rpt_tgts;

		
		struct snic_icmnd		icmnd;

		
		struct snic_itmf		itmf;

		
		struct snic_hba_reset		reset;
	} u;

	ulong req_pa;
}; 


#define SNIC_FW_REQ_LEN		64 
struct snic_fw_req {
	struct snic_io_hdr hdr;
	union {
		
		u8	buf[SNIC_FW_REQ_LEN - sizeof(struct snic_io_hdr)];

		
		struct snic_exch_ver_rsp	exch_ver_cmpl;

		
		struct snic_report_tgts_cmpl	rpt_tgts_cmpl;

		
		struct snic_icmnd_cmpl		icmnd_cmpl;

		
		struct snic_itmf_cmpl		itmf_cmpl;

		
		struct snic_hba_reset_cmpl	reset_cmpl;

		
		struct snic_notify_msg		ack;

		
		struct snic_async_evnotify	async_ev;

	} u;
}; 


#define VERIFY_REQ_SZ(x)
#define VERIFY_CMPL_SZ(x)


static inline void
snic_color_enc(struct snic_fw_req *req, u8 color)
{
	u8 *c = ((u8 *) req) + sizeof(struct snic_fw_req) - 1;

	if (color)
		*c |= 0x80;
	else
		*c &= ~0x80;
}

static inline void
snic_color_dec(struct snic_fw_req *req, u8 *color)
{
	u8 *c = ((u8 *) req) + sizeof(struct snic_fw_req) - 1;

	*color = *c >> 7;

	
	rmb();
}
#endif 
