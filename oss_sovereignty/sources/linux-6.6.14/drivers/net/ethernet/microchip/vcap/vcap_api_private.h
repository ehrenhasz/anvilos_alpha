


#ifndef __VCAP_API_PRIVATE__
#define __VCAP_API_PRIVATE__

#include <linux/types.h>

#include "vcap_api.h"
#include "vcap_api_client.h"

#define to_intrule(rule) container_of((rule), struct vcap_rule_internal, data)

enum vcap_rule_state {
	VCAP_RS_PERMANENT, 
	VCAP_RS_ENABLED, 
	VCAP_RS_DISABLED, 
};


struct vcap_rule_internal {
	struct vcap_rule data; 
	struct list_head list; 
	struct vcap_admin *admin; 
	struct net_device *ndev;  
	struct vcap_control *vctrl; 
	u32 sort_key;  
	int keyset_sw;  
	int actionset_sw;  
	int keyset_sw_regs;  
	int actionset_sw_regs;  
	int size; 
	u32 addr; 
	u32 counter_id; 
	struct vcap_counter counter; 
	enum vcap_rule_state state;  
};


struct vcap_stream_iter {
	u32 offset; 
	u32 sw_width; 
	u32 regs_per_sw; 
	u32 reg_idx; 
	u32 reg_bitpos; 
	const struct vcap_typegroup *tg; 
};


int vcap_api_check(struct vcap_control *ctrl);

void vcap_erase_cache(struct vcap_rule_internal *ri);



void vcap_iter_init(struct vcap_stream_iter *itr, int sw_width,
		    const struct vcap_typegroup *tg, u32 offset);
void vcap_iter_next(struct vcap_stream_iter *itr);
void vcap_iter_set(struct vcap_stream_iter *itr, int sw_width,
		   const struct vcap_typegroup *tg, u32 offset);
void vcap_iter_update(struct vcap_stream_iter *itr);




int vcap_keyfield_count(struct vcap_control *vctrl,
			enum vcap_type vt, enum vcap_keyfield_set keyset);

const struct vcap_typegroup *
vcap_keyfield_typegroup(struct vcap_control *vctrl,
			enum vcap_type vt, enum vcap_keyfield_set keyset);

const struct vcap_field *vcap_keyfields(struct vcap_control *vctrl,
					enum vcap_type vt,
					enum vcap_keyfield_set keyset);




const struct vcap_set *
vcap_actionfieldset(struct vcap_control *vctrl,
		    enum vcap_type vt, enum vcap_actionfield_set actionset);

int vcap_actionfield_count(struct vcap_control *vctrl,
			   enum vcap_type vt,
			   enum vcap_actionfield_set actionset);

const struct vcap_typegroup *
vcap_actionfield_typegroup(struct vcap_control *vctrl, enum vcap_type vt,
			   enum vcap_actionfield_set actionset);

const struct vcap_field *
vcap_actionfields(struct vcap_control *vctrl,
		  enum vcap_type vt, enum vcap_actionfield_set actionset);

const char *vcap_actionset_name(struct vcap_control *vctrl,
				enum vcap_actionfield_set actionset);

const char *vcap_actionfield_name(struct vcap_control *vctrl,
				  enum vcap_action_field action);


int vcap_addr_keysets(struct vcap_control *vctrl, struct net_device *ndev,
		      struct vcap_admin *admin, int addr,
		      struct vcap_keyset_list *kslist);


int vcap_find_keystream_keysets(struct vcap_control *vctrl, enum vcap_type vt,
				u32 *keystream, u32 *mskstream, bool mask,
				int sw_max, struct vcap_keyset_list *kslist);


int vcap_rule_get_keysets(struct vcap_rule_internal *ri,
			  struct vcap_keyset_list *matches);

struct vcap_rule *vcap_decode_rule(struct vcap_rule_internal *elem);

#endif 
