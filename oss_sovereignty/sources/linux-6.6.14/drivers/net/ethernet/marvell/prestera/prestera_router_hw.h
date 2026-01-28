


#ifndef _PRESTERA_ROUTER_HW_H_
#define _PRESTERA_ROUTER_HW_H_

struct prestera_vr {
	struct list_head router_node;
	refcount_t refcount;
	u32 tb_id;			
	u16 hw_vr_id;			
	u8 __pad[2];
};

struct prestera_rif_entry {
	struct prestera_rif_entry_key {
		struct prestera_iface iface;
	} key;
	struct prestera_vr *vr;
	unsigned char addr[ETH_ALEN];
	u16 hw_id; 
	struct list_head router_node; 
};

struct prestera_ip_addr {
	union {
		__be32 ipv4;
		struct in6_addr ipv6;
	} u;
	enum {
		PRESTERA_IPV4 = 0,
		PRESTERA_IPV6
	} v;
#define PRESTERA_IP_ADDR_PLEN(V) ((V) == PRESTERA_IPV4 ? 32 : \
				   128 )
};

struct prestera_nh_neigh_key {
	struct prestera_ip_addr addr;
	
	
	
	void *rif;
};


struct prestera_neigh_info {
	struct prestera_iface iface;
	unsigned char ha[ETH_ALEN];
	u8 connected; 
	u8 __pad[1];
};


struct prestera_nh_neigh {
	struct prestera_nh_neigh_key key;
	struct prestera_neigh_info info;
	struct rhash_head ht_node; 
	struct list_head nexthop_group_list;
};

#define PRESTERA_NHGR_SIZE_MAX 4

struct prestera_nexthop_group {
	struct prestera_nexthop_group_key {
		struct prestera_nh_neigh_key neigh[PRESTERA_NHGR_SIZE_MAX];
	} key;
	
	
	struct prestera_nh_neigh_head {
		struct prestera_nexthop_group *this;
		struct list_head head;
		
		struct prestera_nh_neigh *neigh;
	} nh_neigh_head[PRESTERA_NHGR_SIZE_MAX];
	struct rhash_head ht_node; 
	refcount_t refcount;
	u32 grp_id; 
};

struct prestera_fib_key {
	struct prestera_ip_addr addr;
	u32 prefix_len;
	u32 tb_id;
};

struct prestera_fib_info {
	struct prestera_vr *vr;
	struct list_head vr_node;
	enum prestera_fib_type {
		PRESTERA_FIB_TYPE_INVALID = 0,
		
		PRESTERA_FIB_TYPE_UC_NH,
		
		PRESTERA_FIB_TYPE_TRAP,
		PRESTERA_FIB_TYPE_DROP
	} type;
	
	struct prestera_nexthop_group *nh_grp;
};

struct prestera_fib_node {
	struct rhash_head ht_node; 
	struct prestera_fib_key key;
	struct prestera_fib_info info; 
};

struct prestera_rif_entry *
prestera_rif_entry_find(const struct prestera_switch *sw,
			const struct prestera_rif_entry_key *k);
void prestera_rif_entry_destroy(struct prestera_switch *sw,
				struct prestera_rif_entry *e);
struct prestera_rif_entry *
prestera_rif_entry_create(struct prestera_switch *sw,
			  struct prestera_rif_entry_key *k,
			  u32 tb_id, const unsigned char *addr);
struct prestera_nh_neigh *
prestera_nh_neigh_find(struct prestera_switch *sw,
		       struct prestera_nh_neigh_key *key);
struct prestera_nh_neigh *
prestera_nh_neigh_get(struct prestera_switch *sw,
		      struct prestera_nh_neigh_key *key);
void prestera_nh_neigh_put(struct prestera_switch *sw,
			   struct prestera_nh_neigh *neigh);
int prestera_nh_neigh_set(struct prestera_switch *sw,
			  struct prestera_nh_neigh *neigh);
bool prestera_nh_neigh_util_hw_state(struct prestera_switch *sw,
				     struct prestera_nh_neigh *nh_neigh);
struct prestera_fib_node *prestera_fib_node_find(struct prestera_switch *sw,
						 struct prestera_fib_key *key);
void prestera_fib_node_destroy(struct prestera_switch *sw,
			       struct prestera_fib_node *fib_node);
struct prestera_fib_node *
prestera_fib_node_create(struct prestera_switch *sw,
			 struct prestera_fib_key *key,
			 enum prestera_fib_type fib_type,
			 struct prestera_nexthop_group_key *nh_grp_key);
int prestera_router_hw_init(struct prestera_switch *sw);
void prestera_router_hw_fini(struct prestera_switch *sw);

#endif 
