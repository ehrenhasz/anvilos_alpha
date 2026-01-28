#ifndef _MSCC_OCELOT_POLICE_H_
#define _MSCC_OCELOT_POLICE_H_
#include "ocelot.h"
#include <net/flow_offload.h>
enum mscc_qos_rate_mode {
	MSCC_QOS_RATE_MODE_DISABLED,  
	MSCC_QOS_RATE_MODE_LINE,  
	MSCC_QOS_RATE_MODE_DATA,  
	MSCC_QOS_RATE_MODE_FRAME,  
	__MSCC_QOS_RATE_MODE_END,
	NUM_MSCC_QOS_RATE_MODE = __MSCC_QOS_RATE_MODE_END,
	MSCC_QOS_RATE_MODE_MAX = __MSCC_QOS_RATE_MODE_END - 1,
};
struct qos_policer_conf {
	enum mscc_qos_rate_mode mode;
	bool dlb;  
	bool cf;   
	u32  cir;  
	u32  cbs;  
	u32  pir;  
	u32  pbs;  
	u8   ipg;  
};
int qos_policer_conf_set(struct ocelot *ocelot, u32 pol_ix,
			 struct qos_policer_conf *conf);
int ocelot_policer_validate(const struct flow_action *action,
			    const struct flow_action_entry *a,
			    struct netlink_ext_ack *extack);
#endif  
