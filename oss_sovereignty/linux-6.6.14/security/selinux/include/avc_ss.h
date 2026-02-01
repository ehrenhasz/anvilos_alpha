 
 
#ifndef _SELINUX_AVC_SS_H_
#define _SELINUX_AVC_SS_H_

#include <linux/types.h>

int avc_ss_reset(u32 seqno);

 
struct security_class_mapping {
	const char *name;
	const char *perms[sizeof(u32) * 8 + 1];
};

extern const struct security_class_mapping secclass_map[];

#endif  

