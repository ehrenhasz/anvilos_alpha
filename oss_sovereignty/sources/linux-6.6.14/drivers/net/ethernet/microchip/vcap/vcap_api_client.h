


#ifndef __VCAP_API_CLIENT__
#define __VCAP_API_CLIENT__

#include <linux/types.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <net/flow_offload.h>

#include "vcap_api.h"


struct vcap_client_keyfield_ctrl {
	struct list_head list;  
	enum vcap_key_field key;
	enum vcap_field_type type;
};

struct vcap_u1_key {
	u8 value;
	u8 mask;
};

struct vcap_u32_key {
	u32 value;
	u32 mask;
};

struct vcap_u48_key {
	u8 value[6];
	u8 mask[6];
};

struct vcap_u56_key {
	u8 value[7];
	u8 mask[7];
};

struct vcap_u64_key {
	u8 value[8];
	u8 mask[8];
};

struct vcap_u72_key {
	u8 value[9];
	u8 mask[9];
};

struct vcap_u112_key {
	u8 value[14];
	u8 mask[14];
};

struct vcap_u128_key {
	u8 value[16];
	u8 mask[16];
};


struct vcap_client_keyfield_data {
	union {
		struct vcap_u1_key u1;
		struct vcap_u32_key u32;
		struct vcap_u48_key u48;
		struct vcap_u56_key u56;
		struct vcap_u64_key u64;
		struct vcap_u72_key u72;
		struct vcap_u112_key u112;
		struct vcap_u128_key u128;
	};
};


struct vcap_client_keyfield {
	struct vcap_client_keyfield_ctrl ctrl;
	struct vcap_client_keyfield_data data;
};


struct vcap_client_actionfield_ctrl {
	struct list_head list;  
	enum vcap_action_field action;
	enum vcap_field_type type;
};

struct vcap_u1_action {
	u8 value;
};

struct vcap_u32_action {
	u32 value;
};

struct vcap_u48_action {
	u8 value[6];
};

struct vcap_u56_action {
	u8 value[7];
};

struct vcap_u64_action {
	u8 value[8];
};

struct vcap_u72_action {
	u8 value[9];
};

struct vcap_u112_action {
	u8 value[14];
};

struct vcap_u128_action {
	u8 value[16];
};

struct vcap_client_actionfield_data {
	union {
		struct vcap_u1_action u1;
		struct vcap_u32_action u32;
		struct vcap_u48_action u48;
		struct vcap_u56_action u56;
		struct vcap_u64_action u64;
		struct vcap_u72_action u72;
		struct vcap_u112_action u112;
		struct vcap_u128_action u128;
	};
};

struct vcap_client_actionfield {
	struct vcap_client_actionfield_ctrl ctrl;
	struct vcap_client_actionfield_data data;
};

enum vcap_bit {
	VCAP_BIT_ANY,
	VCAP_BIT_0,
	VCAP_BIT_1
};

struct vcap_counter {
	u32 value;
	bool sticky;
};


int vcap_enable_lookups(struct vcap_control *vctrl, struct net_device *ndev,
			int from_cid, int to_cid, unsigned long cookie,
			bool enable);



struct vcap_rule *vcap_alloc_rule(struct vcap_control *vctrl,
				  struct net_device *ndev,
				  int vcap_chain_id,
				  enum vcap_user user,
				  u16 priority,
				  u32 id);

void vcap_free_rule(struct vcap_rule *rule);

int vcap_val_rule(struct vcap_rule *rule, u16 l3_proto);

int vcap_add_rule(struct vcap_rule *rule);

int vcap_del_rule(struct vcap_control *vctrl, struct net_device *ndev, u32 id);

struct vcap_rule *vcap_copy_rule(struct vcap_rule *rule);

struct vcap_rule *vcap_get_rule(struct vcap_control *vctrl, u32 id);

int vcap_mod_rule(struct vcap_rule *rule);


int vcap_set_rule_set_keyset(struct vcap_rule *rule,
			     enum vcap_keyfield_set keyset);

int vcap_set_rule_set_actionset(struct vcap_rule *rule,
				enum vcap_actionfield_set actionset);

void vcap_rule_set_counter_id(struct vcap_rule *rule, u32 counter_id);


int vcap_rule_add_key_bit(struct vcap_rule *rule, enum vcap_key_field key,
			  enum vcap_bit val);
int vcap_rule_add_key_u32(struct vcap_rule *rule, enum vcap_key_field key,
			  u32 value, u32 mask);
int vcap_rule_add_key_u48(struct vcap_rule *rule, enum vcap_key_field key,
			  struct vcap_u48_key *fieldval);
int vcap_rule_add_key_u72(struct vcap_rule *rule, enum vcap_key_field key,
			  struct vcap_u72_key *fieldval);
int vcap_rule_add_key_u128(struct vcap_rule *rule, enum vcap_key_field key,
			   struct vcap_u128_key *fieldval);
int vcap_rule_add_action_bit(struct vcap_rule *rule,
			     enum vcap_action_field action, enum vcap_bit val);
int vcap_rule_add_action_u32(struct vcap_rule *rule,
			     enum vcap_action_field action, u32 value);


int vcap_admin_rule_count(struct vcap_admin *admin, int cid);


int vcap_get_rule_count_by_cookie(struct vcap_control *vctrl,
				  struct vcap_counter *ctr, u64 cookie);
int vcap_rule_set_counter(struct vcap_rule *rule, struct vcap_counter *ctr);
int vcap_rule_get_counter(struct vcap_rule *rule, struct vcap_counter *ctr);



int vcap_chain_id_to_lookup(struct vcap_admin *admin, int cur_cid);

struct vcap_admin *vcap_find_admin(struct vcap_control *vctrl, int cid);

const struct vcap_field *vcap_lookup_keyfield(struct vcap_rule *rule,
					      enum vcap_key_field key);

int vcap_lookup_rule_by_cookie(struct vcap_control *vctrl, u64 cookie);

int vcap_chain_offset(struct vcap_control *vctrl, int from_cid, int to_cid);

bool vcap_is_next_lookup(struct vcap_control *vctrl, int cur_cid, int next_cid);

bool vcap_is_last_chain(struct vcap_control *vctrl, int cid, bool ingress);

bool vcap_rule_find_keysets(struct vcap_rule *rule,
			    struct vcap_keyset_list *matches);

const struct vcap_set *vcap_keyfieldset(struct vcap_control *vctrl,
					enum vcap_type vt,
					enum vcap_keyfield_set keyset);

void vcap_netbytes_copy(u8 *dst, u8 *src, int count);


void vcap_set_tc_exterr(struct flow_cls_offload *fco, struct vcap_rule *vrule);


int vcap_del_rules(struct vcap_control *vctrl, struct vcap_admin *admin);


bool vcap_keyset_list_add(struct vcap_keyset_list *keysetlist,
			  enum vcap_keyfield_set keyset);

int vcap_filter_rule_keys(struct vcap_rule *rule,
			  enum vcap_key_field keylist[], int length,
			  bool drop_unsupported);


const char *vcap_keyset_name(struct vcap_control *vctrl,
			     enum vcap_keyfield_set keyset);

const char *vcap_keyfield_name(struct vcap_control *vctrl,
			       enum vcap_key_field key);


int vcap_rule_mod_key_u32(struct vcap_rule *rule, enum vcap_key_field key,
			  u32 value, u32 mask);

int vcap_rule_mod_action_u32(struct vcap_rule *rule,
			     enum vcap_action_field action,
			     u32 value);


int vcap_rule_get_key_u32(struct vcap_rule *rule, enum vcap_key_field key,
			  u32 *value, u32 *mask);


int vcap_rule_rem_key(struct vcap_rule *rule, enum vcap_key_field key);


enum vcap_keyfield_set
vcap_select_min_rule_keyset(struct vcap_control *vctrl, enum vcap_type vtype,
			    struct vcap_keyset_list *kslist);

struct vcap_client_actionfield *
vcap_find_actionfield(struct vcap_rule *rule, enum vcap_action_field act);
#endif 
