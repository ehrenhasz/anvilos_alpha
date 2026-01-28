#ifndef __OTX_CPT_COMMON_H
#define __OTX_CPT_COMMON_H
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#define OTX_CPT_MAX_MBOX_DATA_STR_SIZE 64
enum otx_cptpf_type {
	OTX_CPT_AE = 2,
	OTX_CPT_SE = 3,
	BAD_OTX_CPTPF_TYPE,
};
enum otx_cptvf_type {
	OTX_CPT_AE_TYPES = 1,
	OTX_CPT_SE_TYPES = 2,
	BAD_OTX_CPTVF_TYPE,
};
enum otx_cpt_mbox_opcode {
	OTX_CPT_MSG_VF_UP = 1,
	OTX_CPT_MSG_VF_DOWN,
	OTX_CPT_MSG_READY,
	OTX_CPT_MSG_QLEN,
	OTX_CPT_MSG_QBIND_GRP,
	OTX_CPT_MSG_VQ_PRIORITY,
	OTX_CPT_MSG_PF_TYPE,
	OTX_CPT_MSG_ACK,
	OTX_CPT_MSG_NACK
};
struct otx_cpt_mbox {
	u64 msg;  
	u64 data; 
};
#endif  
