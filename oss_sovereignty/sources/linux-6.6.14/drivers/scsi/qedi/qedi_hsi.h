

#ifndef __QEDI_HSI__
#define __QEDI_HSI__

#include <linux/qed/common_hsi.h>


#include <linux/qed/storage_common.h>


#include <linux/qed/tcp_common.h>


#include <linux/qed/iscsi_common.h>


struct iscsi_cmdqe {
	__le16 conn_id;
	u8 invalid_command;
	u8 cmd_hdr_type;
	__le32 reserved1[2];
	__le32 cmd_payload[13];
};


enum iscsi_cmd_hdr_type {
	ISCSI_CMD_HDR_TYPE_BHS_ONLY ,
	ISCSI_CMD_HDR_TYPE_BHS_W_AHS ,
	ISCSI_CMD_HDR_TYPE_AHS ,
	MAX_ISCSI_CMD_HDR_TYPE
};

#endif 
