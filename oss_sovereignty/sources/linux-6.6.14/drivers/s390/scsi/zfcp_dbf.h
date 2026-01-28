


#ifndef ZFCP_DBF_H
#define ZFCP_DBF_H

#include <scsi/fc/fc_fcp.h>
#include "zfcp_ext.h"
#include "zfcp_fsf.h"
#include "zfcp_def.h"

#define ZFCP_DBF_TAG_LEN       7

#define ZFCP_DBF_INVALID_WWPN	0x0000000000000000ull
#define ZFCP_DBF_INVALID_LUN	0xFFFFFFFFFFFFFFFFull

enum zfcp_dbf_pseudo_erp_act_type {
	ZFCP_PSEUDO_ERP_ACTION_RPORT_ADD = 0xff,
	ZFCP_PSEUDO_ERP_ACTION_RPORT_DEL = 0xfe,
};


struct zfcp_dbf_rec_trigger {
	u32 ready;
	u32 running;
	u8 want;
	u8 need;
} __packed;


struct zfcp_dbf_rec_running {
	u64 fsf_req_id;
	u32 rec_status;
	u16 rec_step;
	u8 rec_action;
	u8 rec_count;
} __packed;


enum zfcp_dbf_rec_id {
	ZFCP_DBF_REC_TRIG	= 1,
	ZFCP_DBF_REC_RUN	= 2,
};


struct zfcp_dbf_rec {
	u8 id;
	char tag[ZFCP_DBF_TAG_LEN];
	u64 lun;
	u64 wwpn;
	u32 d_id;
	u32 adapter_status;
	u32 port_status;
	u32 lun_status;
	union {
		struct zfcp_dbf_rec_trigger trig;
		struct zfcp_dbf_rec_running run;
	} u;
} __packed;


enum zfcp_dbf_san_id {
	ZFCP_DBF_SAN_REQ	= 1,
	ZFCP_DBF_SAN_RES	= 2,
	ZFCP_DBF_SAN_ELS	= 3,
};


struct zfcp_dbf_san {
	u8 id;
	char tag[ZFCP_DBF_TAG_LEN];
	u64 fsf_req_id;
	u32 d_id;
#define ZFCP_DBF_SAN_MAX_PAYLOAD (FC_CT_HDR_LEN + 32)
	char payload[ZFCP_DBF_SAN_MAX_PAYLOAD];
	u16 pl_len;
} __packed;


struct zfcp_dbf_hba_res {
	u64 req_issued;
	u32 prot_status;
	u8  prot_status_qual[FSF_PROT_STATUS_QUAL_SIZE];
	u32 fsf_status;
	u8  fsf_status_qual[FSF_STATUS_QUALIFIER_SIZE];
	u32 port_handle;
	u32 lun_handle;
} __packed;


struct zfcp_dbf_hba_uss {
	u32 status_type;
	u32 status_subtype;
	u32 d_id;
	u64 lun;
	u64 queue_designator;
} __packed;


struct zfcp_dbf_hba_fces {
	u64 req_issued;
	u32 fsf_status;
	u32 port_handle;
	u64 wwpn;
	u32 fc_security_old;
	u32 fc_security_new;
} __packed;


enum zfcp_dbf_hba_id {
	ZFCP_DBF_HBA_RES	= 1,
	ZFCP_DBF_HBA_USS	= 2,
	ZFCP_DBF_HBA_BIT	= 3,
	ZFCP_DBF_HBA_BASIC	= 4,
	ZFCP_DBF_HBA_FCES	= 5,
};


struct zfcp_dbf_hba {
	u8 id;
	char tag[ZFCP_DBF_TAG_LEN];
	u64 fsf_req_id;
	u32 fsf_req_status;
	u32 fsf_cmd;
	u32 fsf_seq_no;
	u16 pl_len;
	union {
		struct zfcp_dbf_hba_res res;
		struct zfcp_dbf_hba_uss uss;
		struct fsf_bit_error_payload be;
		struct zfcp_dbf_hba_fces fces;
	} u;
} __packed;


enum zfcp_dbf_scsi_id {
	ZFCP_DBF_SCSI_CMND	= 1,
};


struct zfcp_dbf_scsi {
	u8 id;
	char tag[ZFCP_DBF_TAG_LEN];
	u32 scsi_id;
	u32 scsi_lun;
	u32 scsi_result;
	u8 scsi_retries;
	u8 scsi_allowed;
	u8 fcp_rsp_info;
#define ZFCP_DBF_SCSI_OPCODE	16
	u8 scsi_opcode[ZFCP_DBF_SCSI_OPCODE];
	u64 fsf_req_id;
	u64 host_scribble;
	u16 pl_len;
	struct fcp_resp_with_ext fcp_rsp;
	u32 scsi_lun_64_hi;
} __packed;


struct zfcp_dbf_pay {
	u8 counter;
	char area[ZFCP_DBF_TAG_LEN];
	u64 fsf_req_id;
#define ZFCP_DBF_PAY_MAX_REC 0x100
	char data[ZFCP_DBF_PAY_MAX_REC];
} __packed;


struct zfcp_dbf {
	debug_info_t			*pay;
	debug_info_t			*rec;
	debug_info_t			*hba;
	debug_info_t			*san;
	debug_info_t			*scsi;
	spinlock_t			pay_lock;
	spinlock_t			rec_lock;
	spinlock_t			hba_lock;
	spinlock_t			san_lock;
	spinlock_t			scsi_lock;
	struct zfcp_dbf_pay		pay_buf;
	struct zfcp_dbf_rec		rec_buf;
	struct zfcp_dbf_hba		hba_buf;
	struct zfcp_dbf_san		san_buf;
	struct zfcp_dbf_scsi		scsi_buf;
};


static inline
bool zfcp_dbf_hba_fsf_resp_suppress(struct zfcp_fsf_req *req)
{
	struct fsf_qtcb *qtcb = req->qtcb;
	u32 fsf_stat = qtcb->header.fsf_status;
	struct fcp_resp *fcp_rsp;
	u8 rsp_flags, fr_status;

	if (qtcb->prefix.qtcb_type != FSF_IO_COMMAND)
		return false; 
	fcp_rsp = &qtcb->bottom.io.fcp_rsp.iu.resp;
	rsp_flags = fcp_rsp->fr_flags;
	fr_status = fcp_rsp->fr_status;
	return (fsf_stat == FSF_FCP_RSP_AVAILABLE) &&
		(rsp_flags == FCP_RESID_UNDER) &&
		(fr_status == SAM_STAT_GOOD);
}

static inline
void zfcp_dbf_hba_fsf_resp(char *tag, int level, struct zfcp_fsf_req *req)
{
	if (debug_level_enabled(req->adapter->dbf->hba, level))
		zfcp_dbf_hba_fsf_res(tag, level, req);
}


static inline
void zfcp_dbf_hba_fsf_response(struct zfcp_fsf_req *req)
{
	struct fsf_qtcb *qtcb = req->qtcb;

	if (unlikely(req->status & (ZFCP_STATUS_FSFREQ_DISMISSED |
				    ZFCP_STATUS_FSFREQ_ERROR))) {
		zfcp_dbf_hba_fsf_resp("fs_rerr", 3, req);

	} else if ((qtcb->prefix.prot_status != FSF_PROT_GOOD) &&
	    (qtcb->prefix.prot_status != FSF_PROT_FSF_STATUS_PRESENTED)) {
		zfcp_dbf_hba_fsf_resp("fs_perr", 1, req);

	} else if (qtcb->header.fsf_status != FSF_GOOD) {
		zfcp_dbf_hba_fsf_resp("fs_ferr",
				      zfcp_dbf_hba_fsf_resp_suppress(req)
				      ? 5 : 1, req);

	} else if ((qtcb->header.fsf_command == FSF_QTCB_OPEN_PORT_WITH_DID) ||
		   (qtcb->header.fsf_command == FSF_QTCB_OPEN_LUN)) {
		zfcp_dbf_hba_fsf_resp("fs_open", 4, req);

	} else if (qtcb->header.log_length) {
		zfcp_dbf_hba_fsf_resp("fs_qtcb", 5, req);

	} else {
		zfcp_dbf_hba_fsf_resp("fs_norm", 6, req);
	}
}

static inline
void _zfcp_dbf_scsi(char *tag, int level, struct scsi_cmnd *scmd,
		   struct zfcp_fsf_req *req)
{
	struct zfcp_adapter *adapter = (struct zfcp_adapter *)
					scmd->device->host->hostdata[0];

	if (debug_level_enabled(adapter->dbf->scsi, level))
		zfcp_dbf_scsi_common(tag, level, scmd->device, scmd, req);
}


static inline
void zfcp_dbf_scsi_result(struct scsi_cmnd *scmd, struct zfcp_fsf_req *req)
{
	if (scmd->result != 0)
		_zfcp_dbf_scsi("rsl_err", 3, scmd, req);
	else if (scmd->retries > 0)
		_zfcp_dbf_scsi("rsl_ret", 4, scmd, req);
	else
		_zfcp_dbf_scsi("rsl_nor", 6, scmd, req);
}


static inline
void zfcp_dbf_scsi_fail_send(struct scsi_cmnd *scmd)
{
	_zfcp_dbf_scsi("rsl_fai", 4, scmd, NULL);
}


static inline
void zfcp_dbf_scsi_abort(char *tag, struct scsi_cmnd *scmd,
			 struct zfcp_fsf_req *fsf_req)
{
	_zfcp_dbf_scsi(tag, 1, scmd, fsf_req);
}


static inline
void zfcp_dbf_scsi_devreset(char *tag, struct scsi_device *sdev, u8 flag,
			    struct zfcp_fsf_req *fsf_req)
{
	struct zfcp_adapter *adapter = (struct zfcp_adapter *)
					sdev->host->hostdata[0];
	char tmp_tag[ZFCP_DBF_TAG_LEN];
	static int const level = 1;

	if (unlikely(!debug_level_enabled(adapter->dbf->scsi, level)))
		return;

	if (flag == FCP_TMF_TGT_RESET)
		memcpy(tmp_tag, "tr_", 3);
	else
		memcpy(tmp_tag, "lr_", 3);

	memcpy(&tmp_tag[3], tag, 4);
	zfcp_dbf_scsi_common(tmp_tag, level, sdev, NULL, fsf_req);
}


static inline void zfcp_dbf_scsi_nullcmnd(struct scsi_cmnd *scmnd,
					  struct zfcp_fsf_req *fsf_req)
{
	_zfcp_dbf_scsi("scfc__1", 3, scmnd, fsf_req);
}

#endif 
