


#ifndef __VCAP_API__
#define __VCAP_API__

#include <linux/types.h>
#include <linux/list.h>
#include <linux/netdevice.h>


#include "vcap_ag_api.h"

#define VCAP_CID_LOOKUP_SIZE          100000 
#define VCAP_CID_INGRESS_L0          1000000 
#define VCAP_CID_INGRESS_L1          1100000 
#define VCAP_CID_INGRESS_L2          1200000 
#define VCAP_CID_INGRESS_L3          1300000 
#define VCAP_CID_INGRESS_L4          1400000 
#define VCAP_CID_INGRESS_L5          1500000 

#define VCAP_CID_PREROUTING_IPV6     3000000 
#define VCAP_CID_PREROUTING          6000000 

#define VCAP_CID_INGRESS_STAGE2_L0   8000000 
#define VCAP_CID_INGRESS_STAGE2_L1   8100000 
#define VCAP_CID_INGRESS_STAGE2_L2   8200000 
#define VCAP_CID_INGRESS_STAGE2_L3   8300000 

#define VCAP_CID_EGRESS_L0           10000000 
#define VCAP_CID_EGRESS_L1           10100000 

#define VCAP_CID_EGRESS_STAGE2_L0    20000000 
#define VCAP_CID_EGRESS_STAGE2_L1    20100000 


enum vcap_user {
	VCAP_USER_PTP,
	VCAP_USER_MRP,
	VCAP_USER_CFM,
	VCAP_USER_VLAN,
	VCAP_USER_QOS,
	VCAP_USER_VCAP_UTIL,
	VCAP_USER_TC,
	VCAP_USER_TC_EXTRA,

	

	
	__VCAP_USER_AFTER_LAST,
	VCAP_USER_MAX = __VCAP_USER_AFTER_LAST - 1,
};


struct vcap_statistics {
	char *name;
	int count;
	const char * const *keyfield_set_names;
	const char * const *actionfield_set_names;
	const char * const *keyfield_names;
	const char * const *actionfield_names;
};


struct vcap_field {
	u16 type;
	u16 width;
	u16 offset;
};


struct vcap_set {
	u8 type_id;
	u8 sw_per_item;
	u8 sw_cnt;
};


struct vcap_typegroup {
	u16 offset;
	u16 width;
	u16 value;
};


struct vcap_info {
	char *name; 
	u16 rows; 
	u16 sw_count; 
	u16 sw_width; 
	u16 sticky_width; 
	u16 act_width;  
	u16 default_cnt; 
	u16 require_cnt_dis; 
	u16 version; 
	const struct vcap_set *keyfield_set; 
	int keyfield_set_size; 
	const struct vcap_set *actionfield_set; 
	int actionfield_set_size; 
	
	const struct vcap_field **keyfield_set_map;
	
	int *keyfield_set_map_size;
	
	const struct vcap_field **actionfield_set_map;
	
	int *actionfield_set_map_size;
	
	const struct vcap_typegroup **keyfield_set_typegroups;
	
	const struct vcap_typegroup **actionfield_set_typegroups;
};

enum vcap_field_type {
	VCAP_FIELD_BIT,
	VCAP_FIELD_U32,
	VCAP_FIELD_U48,
	VCAP_FIELD_U56,
	VCAP_FIELD_U64,
	VCAP_FIELD_U72,
	VCAP_FIELD_U112,
	VCAP_FIELD_U128,
};


struct vcap_cache_data {
	u32 *keystream;
	u32 *maskstream;
	u32 *actionstream;
	u32 counter;
	bool sticky;
};


enum vcap_selection {
	VCAP_SEL_ENTRY = 0x01,
	VCAP_SEL_ACTION = 0x02,
	VCAP_SEL_COUNTER = 0x04,
	VCAP_SEL_ALL = 0xff,
};


enum vcap_command {
	VCAP_CMD_WRITE = 0,
	VCAP_CMD_READ = 1,
	VCAP_CMD_MOVE_DOWN = 2,
	VCAP_CMD_MOVE_UP = 3,
	VCAP_CMD_INITIALIZE = 4,
};

enum vcap_rule_error {
	VCAP_ERR_NONE = 0,  
	VCAP_ERR_NO_ADMIN,  
	VCAP_ERR_NO_NETDEV,  
	VCAP_ERR_NO_KEYSET_MATCH, 
	VCAP_ERR_NO_ACTIONSET_MATCH, 
	VCAP_ERR_NO_PORT_KEYSET_MATCH, 
};


struct vcap_admin {
	struct list_head list; 
	struct list_head rules; 
	struct list_head enabled; 
	struct mutex lock; 
	enum vcap_type vtype;  
	int vinst; 
	int first_cid; 
	int last_cid; 
	int tgt_inst; 
	int lookups; 
	int lookups_per_instance; 
	int last_valid_addr; 
	int first_valid_addr; 
	int last_used_addr;  
	bool w32be; 
	bool ingress; 
	struct vcap_cache_data cache; 
};


struct vcap_rule {
	int vcap_chain_id; 
	enum vcap_user user; 
	u16 priority;
	u32 id;  
	u64 cookie;  
	struct list_head keyfields;  
	struct list_head actionfields;  
	enum vcap_keyfield_set keyset; 
	enum vcap_actionfield_set actionset; 
	enum vcap_rule_error exterr; 
	u64 client; 
};


struct vcap_keyset_list {
	int max; 
	int cnt; 
	enum vcap_keyfield_set *keysets; 
};


struct vcap_actionset_list {
	int max; 
	int cnt; 
	enum vcap_actionfield_set *actionsets; 
};


struct vcap_output_print {
	__printf(2, 3)
	void (*prf)(void *out, const char *fmt, ...);
	void *dst;
};


struct vcap_operations {
	
	enum vcap_keyfield_set (*validate_keyset)
		(struct net_device *ndev,
		 struct vcap_admin *admin,
		 struct vcap_rule *rule,
		 struct vcap_keyset_list *kslist,
		 u16 l3_proto);
	
	void (*add_default_fields)
		(struct net_device *ndev,
		 struct vcap_admin *admin,
		 struct vcap_rule *rule);
	
	void (*cache_erase)
		(struct vcap_admin *admin);
	void (*cache_write)
		(struct net_device *ndev,
		 struct vcap_admin *admin,
		 enum vcap_selection sel,
		 u32 idx, u32 count);
	void (*cache_read)
		(struct net_device *ndev,
		 struct vcap_admin *admin,
		 enum vcap_selection sel,
		 u32 idx,
		 u32 count);
	
	void (*init)
		(struct net_device *ndev,
		 struct vcap_admin *admin,
		 u32 addr,
		 u32 count);
	void (*update)
		(struct net_device *ndev,
		 struct vcap_admin *admin,
		 enum vcap_command cmd,
		 enum vcap_selection sel,
		 u32 addr);
	void (*move)
		(struct net_device *ndev,
		 struct vcap_admin *admin,
		 u32 addr,
		 int offset,
		 int count);
	
	int (*port_info)
		(struct net_device *ndev,
		 struct vcap_admin *admin,
		 struct vcap_output_print *out);
};


struct vcap_control {
	struct vcap_operations *ops;  
	const struct vcap_info *vcaps; 
	const struct vcap_statistics *stats; 
	struct list_head list; 
};

#endif 
