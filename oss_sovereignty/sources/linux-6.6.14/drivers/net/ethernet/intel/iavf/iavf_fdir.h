


#ifndef _IAVF_FDIR_H_
#define _IAVF_FDIR_H_

struct iavf_adapter;


enum iavf_fdir_fltr_state_t {
	IAVF_FDIR_FLTR_ADD_REQUEST,	
	IAVF_FDIR_FLTR_ADD_PENDING,	
	IAVF_FDIR_FLTR_DEL_REQUEST,	
	IAVF_FDIR_FLTR_DEL_PENDING,	
	IAVF_FDIR_FLTR_DIS_REQUEST,	
	IAVF_FDIR_FLTR_DIS_PENDING,	
	IAVF_FDIR_FLTR_INACTIVE,	
	IAVF_FDIR_FLTR_ACTIVE,		
};

enum iavf_fdir_flow_type {
	
	IAVF_FDIR_FLOW_NONE = 0,
	IAVF_FDIR_FLOW_IPV4_TCP,
	IAVF_FDIR_FLOW_IPV4_UDP,
	IAVF_FDIR_FLOW_IPV4_SCTP,
	IAVF_FDIR_FLOW_IPV4_AH,
	IAVF_FDIR_FLOW_IPV4_ESP,
	IAVF_FDIR_FLOW_IPV4_OTHER,
	IAVF_FDIR_FLOW_IPV6_TCP,
	IAVF_FDIR_FLOW_IPV6_UDP,
	IAVF_FDIR_FLOW_IPV6_SCTP,
	IAVF_FDIR_FLOW_IPV6_AH,
	IAVF_FDIR_FLOW_IPV6_ESP,
	IAVF_FDIR_FLOW_IPV6_OTHER,
	IAVF_FDIR_FLOW_NON_IP_L2,
	
	IAVF_FDIR_FLOW_PTYPE_MAX,
};


#define IAVF_FLEX_WORD_NUM	2

struct iavf_flex_word {
	u16 offset;
	u16 word;
};

struct iavf_ipv4_addrs {
	__be32 src_ip;
	__be32 dst_ip;
};

struct iavf_ipv6_addrs {
	struct in6_addr src_ip;
	struct in6_addr dst_ip;
};

struct iavf_fdir_eth {
	__be16 etype;
};

struct iavf_fdir_ip {
	union {
		struct iavf_ipv4_addrs v4_addrs;
		struct iavf_ipv6_addrs v6_addrs;
	};
	__be16 src_port;
	__be16 dst_port;
	__be32 l4_header;	
	__be32 spi;		
	union {
		u8 tos;
		u8 tclass;
	};
	u8 proto;
};

struct iavf_fdir_extra {
	u32 usr_def[IAVF_FLEX_WORD_NUM];
};


struct iavf_fdir_fltr {
	enum iavf_fdir_fltr_state_t state;
	struct list_head list;

	enum iavf_fdir_flow_type flow_type;

	struct iavf_fdir_eth eth_data;
	struct iavf_fdir_eth eth_mask;

	struct iavf_fdir_ip ip_data;
	struct iavf_fdir_ip ip_mask;

	struct iavf_fdir_extra ext_data;
	struct iavf_fdir_extra ext_mask;

	enum virtchnl_action action;

	
	u8 ip_ver; 
	u8 flex_cnt;
	struct iavf_flex_word flex_words[IAVF_FLEX_WORD_NUM];

	u32 flow_id;

	u32 loc;	
	u32 q_index;

	struct virtchnl_fdir_add vc_add_msg;
};

int iavf_validate_fdir_fltr_masks(struct iavf_adapter *adapter,
				  struct iavf_fdir_fltr *fltr);
int iavf_fill_fdir_add_msg(struct iavf_adapter *adapter, struct iavf_fdir_fltr *fltr);
void iavf_print_fdir_fltr(struct iavf_adapter *adapter, struct iavf_fdir_fltr *fltr);
bool iavf_fdir_is_dup_fltr(struct iavf_adapter *adapter, struct iavf_fdir_fltr *fltr);
void iavf_fdir_list_add_fltr(struct iavf_adapter *adapter, struct iavf_fdir_fltr *fltr);
struct iavf_fdir_fltr *iavf_find_fdir_fltr_by_loc(struct iavf_adapter *adapter, u32 loc);
#endif 
