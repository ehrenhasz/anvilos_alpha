#ifndef _SPARC64_MDESC_H
#define _SPARC64_MDESC_H
#include <linux/types.h>
#include <linux/cpumask.h>
#include <asm/prom.h>
struct mdesc_handle;
struct mdesc_handle *mdesc_grab(void);
void mdesc_release(struct mdesc_handle *);
#define MDESC_NODE_NULL		(~(u64)0)
#define MDESC_MAX_STR_LEN	256
u64 mdesc_node_by_name(struct mdesc_handle *handle,
		       u64 from_node, const char *name);
#define mdesc_for_each_node_by_name(__hdl, __node, __name) \
	for (__node = mdesc_node_by_name(__hdl, MDESC_NODE_NULL, __name); \
	     (__node) != MDESC_NODE_NULL; \
	     __node = mdesc_node_by_name(__hdl, __node, __name))
const void *mdesc_get_property(struct mdesc_handle *handle,
			       u64 node, const char *name, int *lenp);
const char *mdesc_node_name(struct mdesc_handle *hp, u64 node);
#define MDESC_ARC_TYPE_FWD	"fwd"
#define MDESC_ARC_TYPE_BACK	"back"
u64 mdesc_next_arc(struct mdesc_handle *handle, u64 from,
		   const char *arc_type);
#define mdesc_for_each_arc(__arc, __hdl, __node, __type) \
	for (__arc = mdesc_next_arc(__hdl, __node, __type); \
	     (__arc) != MDESC_NODE_NULL; \
	     __arc = mdesc_next_arc(__hdl, __arc, __type))
u64 mdesc_arc_target(struct mdesc_handle *hp, u64 arc);
void mdesc_update(void);
struct mdesc_notifier_client {
	void (*add)(struct mdesc_handle *handle, u64 node,
		    const char *node_name);
	void (*remove)(struct mdesc_handle *handle, u64 node,
		       const char *node_name);
	const char			*node_name;
	struct mdesc_notifier_client	*next;
};
void mdesc_register_notifier(struct mdesc_notifier_client *client);
union md_node_info {
	struct vdev_port {
		u64 id;				 
		u64 parent_cfg_hdl;		 
		const char *name;		 
	} vdev_port;
	struct ds_port {
		u64 id;				 
	} ds_port;
};
u64 mdesc_get_node(struct mdesc_handle *hp, const char *node_name,
		   union md_node_info *node_info);
int mdesc_get_node_info(struct mdesc_handle *hp, u64 node,
			const char *node_name, union md_node_info *node_info);
void mdesc_fill_in_cpu_data(cpumask_t *mask);
void mdesc_populate_present_mask(cpumask_t *mask);
void mdesc_get_page_sizes(cpumask_t *mask, unsigned long *pgsz_mask);
void sun4v_mdesc_init(void);
#endif
