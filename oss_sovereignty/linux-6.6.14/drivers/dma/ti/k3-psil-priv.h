#ifndef K3_PSIL_PRIV_H_
#define K3_PSIL_PRIV_H_
#include <linux/dma/k3-psil.h>
struct psil_ep {
	u32 thread_id;
	struct psil_endpoint_config ep_config;
};
struct psil_ep_map {
	char *name;
	struct psil_ep	*src;
	int src_count;
	struct psil_ep	*dst;
	int dst_count;
};
struct psil_endpoint_config *psil_get_ep_config(u32 thread_id);
extern struct psil_ep_map am654_ep_map;
extern struct psil_ep_map j721e_ep_map;
extern struct psil_ep_map j7200_ep_map;
extern struct psil_ep_map am64_ep_map;
extern struct psil_ep_map j721s2_ep_map;
extern struct psil_ep_map am62_ep_map;
extern struct psil_ep_map am62a_ep_map;
extern struct psil_ep_map j784s4_ep_map;
#endif  
