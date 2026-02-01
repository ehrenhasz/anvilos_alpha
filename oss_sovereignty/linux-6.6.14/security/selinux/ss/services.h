 
 
#ifndef _SS_SERVICES_H_
#define _SS_SERVICES_H_

#include "policydb.h"

 
struct selinux_mapping {
	u16 value;  
	u16 num_perms;  
	u32 perms[sizeof(u32) * 8];  
};

 
struct selinux_map {
	struct selinux_mapping *mapping;  
	u16 size;  
};

struct selinux_policy {
	struct sidtab *sidtab;
	struct policydb policydb;
	struct selinux_map map;
	u32 latest_granting;
} __randomize_layout;

struct convert_context_args {
	struct policydb *oldp;
	struct policydb *newp;
};

void services_compute_xperms_drivers(struct extended_perms *xperms,
				     struct avtab_node *node);
void services_compute_xperms_decision(struct extended_perms_decision *xpermd,
				      struct avtab_node *node);

int services_convert_context(struct convert_context_args *args,
			     struct context *oldc, struct context *newc,
			     gfp_t gfp_flags);

#endif	 
