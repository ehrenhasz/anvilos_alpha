#include <asm/byteorder.h>
struct lpfc_hba;
#define LPFC_FCP_CDB_LEN 16
#define list_remove_head(list, entry, type, member)		\
	do {							\
	entry = NULL;						\
	if (!list_empty(list)) {				\
		entry = list_entry((list)->next, type, member);	\
		list_del_init(&entry->member);			\
	}							\
	} while(0)
#define list_get_first(list, type, member)			\
	(list_empty(list)) ? NULL :				\
	list_entry((list)->next, type, member)
struct lpfc_rport_data {
	struct lpfc_nodelist *pnode;	 
};
struct lpfc_device_id {
	struct lpfc_name vport_wwpn;
	struct lpfc_name target_wwpn;
	uint64_t lun;
};
struct lpfc_device_data {
	struct list_head listentry;
	struct lpfc_rport_data *rport_data;
	struct lpfc_device_id device_id;
	uint8_t priority;
	bool oas_enabled;
	bool available;
};
struct fcp_rsp {
	uint32_t rspRsvd1;	 
	uint32_t rspRsvd2;	 
	uint8_t rspStatus0;	 
	uint8_t rspStatus1;	 
	uint8_t rspStatus2;	 
#define RSP_LEN_VALID  0x01	 
#define SNS_LEN_VALID  0x02	 
#define RESID_OVER     0x04	 
#define RESID_UNDER    0x08	 
	uint8_t rspStatus3;	 
	uint32_t rspResId;	 
	uint32_t rspSnsLen;	 
	uint32_t rspRspLen;	 
	uint8_t rspInfo0;	 
	uint8_t rspInfo1;	 
	uint8_t rspInfo2;	 
	uint8_t rspInfo3;	 
#define RSP_NO_FAILURE       0x00
#define RSP_DATA_BURST_ERR   0x01
#define RSP_CMD_FIELD_ERR    0x02
#define RSP_RO_MISMATCH_ERR  0x03
#define RSP_TM_NOT_SUPPORTED 0x04	 
#define RSP_TM_NOT_COMPLETED 0x05	 
#define RSP_TM_INVALID_LU    0x09	 
	uint32_t rspInfoRsvd;	 
	uint8_t rspSnsInfo[128];
#define SNS_ILLEGAL_REQ 0x05	 
#define SNSCOD_BADCMD 0x20	 
};
struct fcp_cmnd {
	struct scsi_lun  fcp_lun;
	uint8_t fcpCntl0;	 
	uint8_t fcpCntl1;	 
#define  SIMPLE_Q        0x00
#define  HEAD_OF_Q       0x01
#define  ORDERED_Q       0x02
#define  ACA_Q           0x04
#define  UNTAGGED        0x05
	uint8_t fcpCntl2;	 
#define  FCP_ABORT_TASK_SET  0x02	 
#define  FCP_CLEAR_TASK_SET  0x04	 
#define  FCP_BUS_RESET       0x08	 
#define  FCP_LUN_RESET       0x10	 
#define  FCP_TARGET_RESET    0x20	 
#define  FCP_CLEAR_ACA       0x40	 
#define  FCP_TERMINATE_TASK  0x80	 
	uint8_t fcpCntl3;
#define  WRITE_DATA      0x01	 
#define  READ_DATA       0x02	 
	uint8_t fcpCdb[LPFC_FCP_CDB_LEN];  
	uint32_t fcpDl;		 
};
#define LPFC_SCSI_DMA_EXT_SIZE	264
#define LPFC_BPL_SIZE		1024
#define MDAC_DIRECT_CMD		0x22
#define FIND_FIRST_OAS_LUN	0
#define NO_MORE_OAS_LUN		-1
#define NOT_OAS_ENABLED_LUN	NO_MORE_OAS_LUN
#ifndef FC_PORTSPEED_128GBIT
#define FC_PORTSPEED_128GBIT	0x2000
#endif
#ifndef FC_PORTSPEED_256GBIT
#define FC_PORTSPEED_256GBIT	0x4000
#endif
#define TXRDY_PAYLOAD_LEN	12
#define LPFC_MAX_SCSI_INFO_TMP_LEN	79
