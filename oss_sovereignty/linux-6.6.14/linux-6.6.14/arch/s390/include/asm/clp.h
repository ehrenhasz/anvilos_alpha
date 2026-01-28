#ifndef _ASM_S390_CLP_H
#define _ASM_S390_CLP_H
#define CLP_BLK_SIZE			PAGE_SIZE
#define CLP_SLPC		0x0001
#define CLP_LPS_BASE	0
#define CLP_LPS_PCI	2
struct clp_req_hdr {
	u16 len;
	u16 cmd;
	u32 fmt		: 4;
	u32 reserved1	: 28;
	u64 reserved2;
} __packed;
struct clp_rsp_hdr {
	u16 len;
	u16 rsp;
	u32 fmt		: 4;
	u32 reserved1	: 28;
	u64 reserved2;
} __packed;
#define CLP_RC_OK			0x0010	 
#define CLP_RC_CMD			0x0020	 
#define CLP_RC_PERM			0x0030	 
#define CLP_RC_FMT			0x0040	 
#define CLP_RC_LEN			0x0050	 
#define CLP_RC_8K			0x0060	 
#define CLP_RC_RESNOT0			0x0070	 
#define CLP_RC_NODATA			0x0080	 
#define CLP_RC_FC_UNKNOWN		0x0100	 
struct clp_req_slpc {
	struct clp_req_hdr hdr;
} __packed;
struct clp_rsp_slpc {
	struct clp_rsp_hdr hdr;
	u32 reserved2[4];
	u32 lpif[8];
	u32 reserved3[8];
	u32 lpic[8];
} __packed;
struct clp_req_rsp_slpc {
	struct clp_req_slpc request;
	struct clp_rsp_slpc response;
} __packed;
#endif
