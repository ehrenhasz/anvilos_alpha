 


#ifndef USNIC_ABI_H
#define USNIC_ABI_H

 
#define USNIC_UVERBS_ABI_VERSION	4

#define USNIC_QP_GRP_MAX_WQS		8
#define USNIC_QP_GRP_MAX_RQS		8
#define USNIC_QP_GRP_MAX_CQS		16

enum usnic_transport_type {
	USNIC_TRANSPORT_UNKNOWN		= 0,
	USNIC_TRANSPORT_ROCE_CUSTOM	= 1,
	USNIC_TRANSPORT_IPV4_UDP	= 2,
	USNIC_TRANSPORT_MAX		= 3,
};

struct usnic_transport_spec {
	enum usnic_transport_type	trans_type;
	union {
		struct {
			uint16_t	port_num;
		} usnic_roce;
		struct {
			uint32_t	sock_fd;
		} udp;
	};
};

struct usnic_ib_create_qp_cmd {
	struct usnic_transport_spec	spec;
};

 
struct usnic_ib_create_qp_resp {
	u32				vfid;
	u32				qp_grp_id;
	u64				bar_bus_addr;
	u32				bar_len;
 
	u32				wq_cnt;
	u32				rq_cnt;
	u32				cq_cnt;
	u32				wq_idx[USNIC_QP_GRP_MAX_WQS];
	u32				rq_idx[USNIC_QP_GRP_MAX_RQS];
	u32				cq_idx[USNIC_QP_GRP_MAX_CQS];
	u32				transport;
	u32				reserved[9];
};

#endif  
