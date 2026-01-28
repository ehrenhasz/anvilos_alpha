#ifndef _VNIC_RESOURCE_H_
#define _VNIC_RESOURCE_H_
#define VNIC_RES_MAGIC		0x766E6963L	 
#define VNIC_RES_VERSION	0x00000000L
enum vnic_res_type {
	RES_TYPE_EOL,			 
	RES_TYPE_WQ,			 
	RES_TYPE_RQ,			 
	RES_TYPE_CQ,			 
	RES_TYPE_RSVD1,
	RES_TYPE_NIC_CFG,		 
	RES_TYPE_RSVD2,
	RES_TYPE_RSVD3,
	RES_TYPE_RSVD4,
	RES_TYPE_RSVD5,
	RES_TYPE_INTR_CTRL,		 
	RES_TYPE_INTR_TABLE,		 
	RES_TYPE_INTR_PBA,		 
	RES_TYPE_INTR_PBA_LEGACY,	 
	RES_TYPE_RSVD6,
	RES_TYPE_RSVD7,
	RES_TYPE_DEVCMD,		 
	RES_TYPE_PASS_THRU_PAGE,	 
	RES_TYPE_SUBVNIC,                
	RES_TYPE_MQ_WQ,                  
	RES_TYPE_MQ_RQ,                  
	RES_TYPE_MQ_CQ,                  
	RES_TYPE_DEPRECATED1,            
	RES_TYPE_DEPRECATED2,            
	RES_TYPE_DEVCMD2,                
	RES_TYPE_MAX,			 
};
struct vnic_resource_header {
	u32 magic;
	u32 version;
};
struct vnic_resource {
	u8 type;
	u8 bar;
	u8 pad[2];
	u32 bar_offset;
	u32 count;
};
#endif  
