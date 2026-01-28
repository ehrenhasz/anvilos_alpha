


#ifndef __NFP_FLOWER_CONNTRACK_H__
#define __NFP_FLOWER_CONNTRACK_H__ 1

#include <net/netfilter/nf_flow_table.h>
#include "main.h"

#define NFP_FL_CT_NO_TUN	0xff

#define COMPARE_UNMASKED_FIELDS(__match1, __match2, __out)	\
	do {							\
		typeof(__match1) _match1 = (__match1);		\
		typeof(__match2) _match2 = (__match2);		\
		bool *_out = (__out);		\
		int i, size = sizeof(*(_match1).key);		\
		char *k1, *m1, *k2, *m2;			\
		*_out = false;					\
		k1 = (char *)_match1.key;			\
		m1 = (char *)_match1.mask;			\
		k2 = (char *)_match2.key;			\
		m2 = (char *)_match2.mask;			\
		for (i = 0; i < size; i++)			\
			if ((k1[i] & m1[i] & m2[i]) ^		\
			    (k2[i] & m1[i] & m2[i])) {		\
				*_out = true;			\
				break;				\
			}					\
	} while (0)						\

extern const struct rhashtable_params nfp_zone_table_params;
extern const struct rhashtable_params nfp_ct_map_params;
extern const struct rhashtable_params nfp_tc_ct_merge_params;
extern const struct rhashtable_params nfp_nft_ct_merge_params;


struct nfp_fl_ct_zone_entry {
	u16 zone;
	struct rhash_head hash_node;

	struct nfp_flower_priv *priv;
	struct nf_flowtable *nft;

	struct list_head pre_ct_list;
	unsigned int pre_ct_count;

	struct list_head post_ct_list;
	unsigned int post_ct_count;

	struct rhashtable tc_merge_tb;
	unsigned int tc_merge_count;

	struct list_head nft_flows_list;
	unsigned int nft_flows_count;

	struct rhashtable nft_merge_tb;
	unsigned int nft_merge_count;
};

enum ct_entry_type {
	CT_TYPE_PRE_CT,
	CT_TYPE_NFT,
	CT_TYPE_POST_CT,
	_CT_TYPE_MAX,
};

#define NFP_MAX_RECIRC_CT_ZONES 4
#define NFP_MAX_ENTRY_RULES  (NFP_MAX_RECIRC_CT_ZONES * 2 + 1)

enum nfp_nfp_layer_name {
	FLOW_PAY_META_TCI =    0,
	FLOW_PAY_INPORT,
	FLOW_PAY_EXT_META,
	FLOW_PAY_MAC_MPLS,
	FLOW_PAY_L4,
	FLOW_PAY_IPV4,
	FLOW_PAY_IPV6,
	FLOW_PAY_CT,
	FLOW_PAY_GRE,
	FLOW_PAY_QINQ,
	FLOW_PAY_UDP_TUN,
	FLOW_PAY_GENEVE_OPT,

	_FLOW_PAY_LAYERS_MAX
};


#define NFP_FL_ACTION_DO_NAT		BIT(0)
#define NFP_FL_ACTION_DO_MANGLE		BIT(1)


struct nfp_fl_ct_flow_entry {
	unsigned long cookie;
	struct list_head list_node;
	u32 chain_index;
	u32 goto_chain_index;
	struct net_device *netdev;
	struct nfp_fl_ct_zone_entry *zt;
	struct list_head children;
	struct flow_rule *rule;
	struct flow_stats stats;
	struct nfp_fl_nft_tc_merge *prev_m_entries[NFP_MAX_RECIRC_CT_ZONES - 1];
	u8 num_prev_m_entries;
	u8 tun_offset;		
	u8 flags;
	u8 type;
};


struct nfp_fl_ct_tc_merge {
	unsigned long cookie[2];
	struct rhash_head hash_node;
	struct list_head pre_ct_list;
	struct list_head post_ct_list;
	struct nfp_fl_ct_zone_entry *zt;
	struct nfp_fl_ct_flow_entry *pre_ct_parent;
	struct nfp_fl_ct_flow_entry *post_ct_parent;
	struct list_head children;
};


struct nfp_fl_nft_tc_merge {
	struct net_device *netdev;
	unsigned long cookie[3];
	struct rhash_head hash_node;
	struct nfp_fl_ct_zone_entry *zt;
	struct list_head nft_flow_list;
	struct list_head tc_merge_list;
	struct nfp_fl_ct_tc_merge *tc_m_parent;
	struct nfp_fl_ct_flow_entry *nft_parent;
	unsigned long tc_flower_cookie;
	struct nfp_fl_payload *flow_pay;
	struct nfp_fl_ct_flow_entry *next_pre_ct_entry;
};


struct nfp_fl_ct_map_entry {
	unsigned long cookie;
	struct rhash_head hash_node;
	struct nfp_fl_ct_flow_entry *ct_entry;
};

bool is_pre_ct_flow(struct flow_cls_offload *flow);
bool is_post_ct_flow(struct flow_cls_offload *flow);


int nfp_fl_ct_handle_pre_ct(struct nfp_flower_priv *priv,
			    struct net_device *netdev,
			    struct flow_cls_offload *flow,
			    struct netlink_ext_ack *extack,
			    struct nfp_fl_nft_tc_merge *m_entry);

int nfp_fl_ct_handle_post_ct(struct nfp_flower_priv *priv,
			     struct net_device *netdev,
			     struct flow_cls_offload *flow,
			     struct netlink_ext_ack *extack);


int nfp_fl_create_new_pre_ct(struct nfp_fl_nft_tc_merge *m_entry);


void nfp_fl_ct_clean_flow_entry(struct nfp_fl_ct_flow_entry *entry);


int nfp_fl_ct_del_flow(struct nfp_fl_ct_map_entry *ct_map_ent);


int nfp_fl_ct_handle_nft_flow(enum tc_setup_type type, void *type_data,
			      void *cb_priv);


int nfp_fl_ct_stats(struct flow_cls_offload *flow,
		    struct nfp_fl_ct_map_entry *ct_map_ent);
#endif
