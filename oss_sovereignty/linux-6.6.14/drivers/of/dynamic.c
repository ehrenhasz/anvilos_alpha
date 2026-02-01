
 

#define pr_fmt(fmt)	"OF: " fmt

#include <linux/of.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/proc_fs.h>

#include "of_private.h"

static struct device_node *kobj_to_device_node(struct kobject *kobj)
{
	return container_of(kobj, struct device_node, kobj);
}

 
struct device_node *of_node_get(struct device_node *node)
{
	if (node)
		kobject_get(&node->kobj);
	return node;
}
EXPORT_SYMBOL(of_node_get);

 
void of_node_put(struct device_node *node)
{
	if (node)
		kobject_put(&node->kobj);
}
EXPORT_SYMBOL(of_node_put);

static BLOCKING_NOTIFIER_HEAD(of_reconfig_chain);

int of_reconfig_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&of_reconfig_chain, nb);
}
EXPORT_SYMBOL_GPL(of_reconfig_notifier_register);

int of_reconfig_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&of_reconfig_chain, nb);
}
EXPORT_SYMBOL_GPL(of_reconfig_notifier_unregister);

static const char *action_names[] = {
	[0] = "INVALID",
	[OF_RECONFIG_ATTACH_NODE] = "ATTACH_NODE",
	[OF_RECONFIG_DETACH_NODE] = "DETACH_NODE",
	[OF_RECONFIG_ADD_PROPERTY] = "ADD_PROPERTY",
	[OF_RECONFIG_REMOVE_PROPERTY] = "REMOVE_PROPERTY",
	[OF_RECONFIG_UPDATE_PROPERTY] = "UPDATE_PROPERTY",
};

#define _do_print(func, prefix, action, node, prop, ...) ({	\
	func("changeset: " prefix "%-15s %pOF%s%s\n",		\
	     ##__VA_ARGS__, action_names[action], node,		\
	     prop ? ":" : "", prop ? prop->name : "");		\
})
#define of_changeset_action_err(...) _do_print(pr_err, __VA_ARGS__)
#define of_changeset_action_debug(...) _do_print(pr_debug, __VA_ARGS__)

int of_reconfig_notify(unsigned long action, struct of_reconfig_data *p)
{
	int rc;
	struct of_reconfig_data *pr = p;

	of_changeset_action_debug("notify: ", action, pr->dn, pr->prop);

	rc = blocking_notifier_call_chain(&of_reconfig_chain, action, p);
	return notifier_to_errno(rc);
}

 
int of_reconfig_get_state_change(unsigned long action, struct of_reconfig_data *pr)
{
	struct property *prop, *old_prop = NULL;
	int is_status, status_state, old_status_state, prev_state, new_state;

	 
	switch (action) {
	case OF_RECONFIG_ATTACH_NODE:
	case OF_RECONFIG_DETACH_NODE:
		prop = of_find_property(pr->dn, "status", NULL);
		break;
	case OF_RECONFIG_ADD_PROPERTY:
	case OF_RECONFIG_REMOVE_PROPERTY:
		prop = pr->prop;
		break;
	case OF_RECONFIG_UPDATE_PROPERTY:
		prop = pr->prop;
		old_prop = pr->old_prop;
		break;
	default:
		return OF_RECONFIG_NO_CHANGE;
	}

	is_status = 0;
	status_state = -1;
	old_status_state = -1;
	prev_state = -1;
	new_state = -1;

	if (prop && !strcmp(prop->name, "status")) {
		is_status = 1;
		status_state = !strcmp(prop->value, "okay") ||
			       !strcmp(prop->value, "ok");
		if (old_prop)
			old_status_state = !strcmp(old_prop->value, "okay") ||
					   !strcmp(old_prop->value, "ok");
	}

	switch (action) {
	case OF_RECONFIG_ATTACH_NODE:
		prev_state = 0;
		 
		new_state = status_state != 0;
		break;
	case OF_RECONFIG_DETACH_NODE:
		 
		prev_state = status_state != 0;
		new_state = 0;
		break;
	case OF_RECONFIG_ADD_PROPERTY:
		if (is_status) {
			 
			prev_state = 1;
			new_state = status_state;
		}
		break;
	case OF_RECONFIG_REMOVE_PROPERTY:
		if (is_status) {
			prev_state = status_state;
			 
			new_state = 1;
		}
		break;
	case OF_RECONFIG_UPDATE_PROPERTY:
		if (is_status) {
			prev_state = old_status_state != 0;
			new_state = status_state != 0;
		}
		break;
	}

	if (prev_state == new_state)
		return OF_RECONFIG_NO_CHANGE;

	return new_state ? OF_RECONFIG_CHANGE_ADD : OF_RECONFIG_CHANGE_REMOVE;
}
EXPORT_SYMBOL_GPL(of_reconfig_get_state_change);

int of_property_notify(int action, struct device_node *np,
		       struct property *prop, struct property *oldprop)
{
	struct of_reconfig_data pr;

	 
	if (!of_node_is_attached(np))
		return 0;

	pr.dn = np;
	pr.prop = prop;
	pr.old_prop = oldprop;
	return of_reconfig_notify(action, &pr);
}

static void __of_attach_node(struct device_node *np)
{
	const __be32 *phandle;
	int sz;
	unsigned long flags;

	raw_spin_lock_irqsave(&devtree_lock, flags);

	if (!of_node_check_flag(np, OF_OVERLAY)) {
		np->name = __of_get_property(np, "name", NULL);
		if (!np->name)
			np->name = "<NULL>";

		phandle = __of_get_property(np, "phandle", &sz);
		if (!phandle)
			phandle = __of_get_property(np, "linux,phandle", &sz);
		if (IS_ENABLED(CONFIG_PPC_PSERIES) && !phandle)
			phandle = __of_get_property(np, "ibm,phandle", &sz);
		if (phandle && (sz >= 4))
			np->phandle = be32_to_cpup(phandle);
		else
			np->phandle = 0;
	}

	np->child = NULL;
	np->sibling = np->parent->child;
	np->parent->child = np;
	of_node_clear_flag(np, OF_DETACHED);
	np->fwnode.flags |= FWNODE_FLAG_NOT_DEVICE;

	raw_spin_unlock_irqrestore(&devtree_lock, flags);

	__of_attach_node_sysfs(np);
}

 
int of_attach_node(struct device_node *np)
{
	struct of_reconfig_data rd;

	memset(&rd, 0, sizeof(rd));
	rd.dn = np;

	mutex_lock(&of_mutex);
	__of_attach_node(np);
	mutex_unlock(&of_mutex);

	of_reconfig_notify(OF_RECONFIG_ATTACH_NODE, &rd);

	return 0;
}

void __of_detach_node(struct device_node *np)
{
	struct device_node *parent;
	unsigned long flags;

	raw_spin_lock_irqsave(&devtree_lock, flags);

	parent = np->parent;
	if (WARN_ON(of_node_check_flag(np, OF_DETACHED) || !parent)) {
		raw_spin_unlock_irqrestore(&devtree_lock, flags);
		return;
	}

	if (parent->child == np)
		parent->child = np->sibling;
	else {
		struct device_node *prevsib;
		for (prevsib = np->parent->child;
		     prevsib->sibling != np;
		     prevsib = prevsib->sibling)
			;
		prevsib->sibling = np->sibling;
	}

	of_node_set_flag(np, OF_DETACHED);

	 
	__of_phandle_cache_inv_entry(np->phandle);

	raw_spin_unlock_irqrestore(&devtree_lock, flags);

	__of_detach_node_sysfs(np);
}

 
int of_detach_node(struct device_node *np)
{
	struct of_reconfig_data rd;

	memset(&rd, 0, sizeof(rd));
	rd.dn = np;

	mutex_lock(&of_mutex);
	__of_detach_node(np);
	mutex_unlock(&of_mutex);

	of_reconfig_notify(OF_RECONFIG_DETACH_NODE, &rd);

	return 0;
}
EXPORT_SYMBOL_GPL(of_detach_node);

static void property_list_free(struct property *prop_list)
{
	struct property *prop, *next;

	for (prop = prop_list; prop != NULL; prop = next) {
		next = prop->next;
		kfree(prop->name);
		kfree(prop->value);
		kfree(prop);
	}
}

 
void of_node_release(struct kobject *kobj)
{
	struct device_node *node = kobj_to_device_node(kobj);

	 

	 
	if (!of_node_check_flag(node, OF_DETACHED)) {

		pr_err("ERROR: %s() detected bad of_node_put() on %pOF/%s\n",
			__func__, node->parent, node->full_name);

		 
		if (!IS_ENABLED(CONFIG_OF_UNITTEST) ||
		    strcmp(node->parent->full_name, "testcase-data")) {
			dump_stack();
			pr_err("ERROR: next of_node_put() on this node will result in a kobject warning 'refcount_t: underflow; use-after-free.'\n");
		}

		return;
	}
	if (!of_node_check_flag(node, OF_DYNAMIC))
		return;

	if (of_node_check_flag(node, OF_OVERLAY)) {

		if (!of_node_check_flag(node, OF_OVERLAY_FREE_CSET)) {
			 
			pr_err("ERROR: memory leak before free overlay changeset,  %pOF\n",
			       node);
			return;
		}

		 
		if (node->properties)
			pr_err("ERROR: %s(), unexpected properties in %pOF\n",
			       __func__, node);
	}

	if (node->child)
		pr_err("ERROR: %s() unexpected children for %pOF/%s\n",
			__func__, node->parent, node->full_name);

	property_list_free(node->properties);
	property_list_free(node->deadprops);
	fwnode_links_purge(of_fwnode_handle(node));

	kfree(node->full_name);
	kfree(node->data);
	kfree(node);
}

 
struct property *__of_prop_dup(const struct property *prop, gfp_t allocflags)
{
	struct property *new;

	new = kzalloc(sizeof(*new), allocflags);
	if (!new)
		return NULL;

	 
	new->name = kstrdup(prop->name, allocflags);
	new->value = kmemdup(prop->value, prop->length, allocflags);
	new->length = prop->length;
	if (!new->name || !new->value)
		goto err_free;

	 
	of_property_set_flag(new, OF_DYNAMIC);

	return new;

 err_free:
	kfree(new->name);
	kfree(new->value);
	kfree(new);
	return NULL;
}

 
struct device_node *__of_node_dup(const struct device_node *np,
				  const char *full_name)
{
	struct device_node *node;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return NULL;
	node->full_name = kstrdup(full_name, GFP_KERNEL);
	if (!node->full_name) {
		kfree(node);
		return NULL;
	}

	of_node_set_flag(node, OF_DYNAMIC);
	of_node_set_flag(node, OF_DETACHED);
	of_node_init(node);

	 
	if (np) {
		struct property *pp, *new_pp;
		for_each_property_of_node(np, pp) {
			new_pp = __of_prop_dup(pp, GFP_KERNEL);
			if (!new_pp)
				goto err_prop;
			if (__of_add_property(node, new_pp)) {
				kfree(new_pp->name);
				kfree(new_pp->value);
				kfree(new_pp);
				goto err_prop;
			}
		}
	}
	return node;

 err_prop:
	of_node_put(node);  
	return NULL;
}

 
struct device_node *of_changeset_create_node(struct of_changeset *ocs,
					     struct device_node *parent,
					     const char *full_name)
{
	struct device_node *np;
	int ret;

	np = __of_node_dup(NULL, full_name);
	if (!np)
		return NULL;
	np->parent = parent;

	ret = of_changeset_attach_node(ocs, np);
	if (ret) {
		of_node_put(np);
		return NULL;
	}

	return np;
}
EXPORT_SYMBOL(of_changeset_create_node);

static void __of_changeset_entry_destroy(struct of_changeset_entry *ce)
{
	if (ce->action == OF_RECONFIG_ATTACH_NODE &&
	    of_node_check_flag(ce->np, OF_OVERLAY)) {
		if (kref_read(&ce->np->kobj.kref) > 1) {
			pr_err("ERROR: memory leak, expected refcount 1 instead of %d, of_node_get()/of_node_put() unbalanced - destroy cset entry: attach overlay node %pOF\n",
			       kref_read(&ce->np->kobj.kref), ce->np);
		} else {
			of_node_set_flag(ce->np, OF_OVERLAY_FREE_CSET);
		}
	}

	of_node_put(ce->np);
	list_del(&ce->node);
	kfree(ce);
}

static void __of_changeset_entry_invert(struct of_changeset_entry *ce,
					  struct of_changeset_entry *rce)
{
	memcpy(rce, ce, sizeof(*rce));

	switch (ce->action) {
	case OF_RECONFIG_ATTACH_NODE:
		rce->action = OF_RECONFIG_DETACH_NODE;
		break;
	case OF_RECONFIG_DETACH_NODE:
		rce->action = OF_RECONFIG_ATTACH_NODE;
		break;
	case OF_RECONFIG_ADD_PROPERTY:
		rce->action = OF_RECONFIG_REMOVE_PROPERTY;
		break;
	case OF_RECONFIG_REMOVE_PROPERTY:
		rce->action = OF_RECONFIG_ADD_PROPERTY;
		break;
	case OF_RECONFIG_UPDATE_PROPERTY:
		rce->old_prop = ce->prop;
		rce->prop = ce->old_prop;
		 
		if (!rce->prop) {
			rce->action = OF_RECONFIG_REMOVE_PROPERTY;
			rce->prop = ce->prop;
		}
		break;
	}
}

static int __of_changeset_entry_notify(struct of_changeset_entry *ce,
		bool revert)
{
	struct of_reconfig_data rd;
	struct of_changeset_entry ce_inverted;
	int ret = 0;

	if (revert) {
		__of_changeset_entry_invert(ce, &ce_inverted);
		ce = &ce_inverted;
	}

	switch (ce->action) {
	case OF_RECONFIG_ATTACH_NODE:
	case OF_RECONFIG_DETACH_NODE:
		memset(&rd, 0, sizeof(rd));
		rd.dn = ce->np;
		ret = of_reconfig_notify(ce->action, &rd);
		break;
	case OF_RECONFIG_ADD_PROPERTY:
	case OF_RECONFIG_REMOVE_PROPERTY:
	case OF_RECONFIG_UPDATE_PROPERTY:
		ret = of_property_notify(ce->action, ce->np, ce->prop, ce->old_prop);
		break;
	default:
		pr_err("invalid devicetree changeset action: %i\n",
			(int)ce->action);
		ret = -EINVAL;
	}

	if (ret)
		pr_err("changeset notifier error @%pOF\n", ce->np);
	return ret;
}

static int __of_changeset_entry_apply(struct of_changeset_entry *ce)
{
	int ret = 0;

	of_changeset_action_debug("apply: ", ce->action, ce->np, ce->prop);

	switch (ce->action) {
	case OF_RECONFIG_ATTACH_NODE:
		__of_attach_node(ce->np);
		break;
	case OF_RECONFIG_DETACH_NODE:
		__of_detach_node(ce->np);
		break;
	case OF_RECONFIG_ADD_PROPERTY:
		ret = __of_add_property(ce->np, ce->prop);
		break;
	case OF_RECONFIG_REMOVE_PROPERTY:
		ret = __of_remove_property(ce->np, ce->prop);
		break;

	case OF_RECONFIG_UPDATE_PROPERTY:
		ret = __of_update_property(ce->np, ce->prop, &ce->old_prop);
		break;
	default:
		ret = -EINVAL;
	}

	if (ret) {
		of_changeset_action_err("apply failed: ", ce->action, ce->np, ce->prop);
		return ret;
	}

	return 0;
}

static inline int __of_changeset_entry_revert(struct of_changeset_entry *ce)
{
	struct of_changeset_entry ce_inverted;

	__of_changeset_entry_invert(ce, &ce_inverted);
	return __of_changeset_entry_apply(&ce_inverted);
}

 
void of_changeset_init(struct of_changeset *ocs)
{
	memset(ocs, 0, sizeof(*ocs));
	INIT_LIST_HEAD(&ocs->entries);
}
EXPORT_SYMBOL_GPL(of_changeset_init);

 
void of_changeset_destroy(struct of_changeset *ocs)
{
	struct of_changeset_entry *ce, *cen;

	list_for_each_entry_safe_reverse(ce, cen, &ocs->entries, node)
		__of_changeset_entry_destroy(ce);
}
EXPORT_SYMBOL_GPL(of_changeset_destroy);

 
int __of_changeset_apply_entries(struct of_changeset *ocs, int *ret_revert)
{
	struct of_changeset_entry *ce;
	int ret, ret_tmp;

	pr_debug("changeset: applying...\n");
	list_for_each_entry(ce, &ocs->entries, node) {
		ret = __of_changeset_entry_apply(ce);
		if (ret) {
			pr_err("Error applying changeset (%d)\n", ret);
			list_for_each_entry_continue_reverse(ce, &ocs->entries,
							     node) {
				ret_tmp = __of_changeset_entry_revert(ce);
				if (ret_tmp)
					*ret_revert = ret_tmp;
			}
			return ret;
		}
	}

	return 0;
}

 
int __of_changeset_apply_notify(struct of_changeset *ocs)
{
	struct of_changeset_entry *ce;
	int ret = 0, ret_tmp;

	pr_debug("changeset: emitting notifiers.\n");

	 
	mutex_unlock(&of_mutex);
	list_for_each_entry(ce, &ocs->entries, node) {
		ret_tmp = __of_changeset_entry_notify(ce, 0);
		if (ret_tmp)
			ret = ret_tmp;
	}
	mutex_lock(&of_mutex);
	pr_debug("changeset: notifiers sent.\n");

	return ret;
}

 
static int __of_changeset_apply(struct of_changeset *ocs)
{
	int ret, ret_revert = 0;

	ret = __of_changeset_apply_entries(ocs, &ret_revert);
	if (!ret)
		ret = __of_changeset_apply_notify(ocs);

	return ret;
}

 
int of_changeset_apply(struct of_changeset *ocs)
{
	int ret;

	mutex_lock(&of_mutex);
	ret = __of_changeset_apply(ocs);
	mutex_unlock(&of_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(of_changeset_apply);

 
int __of_changeset_revert_entries(struct of_changeset *ocs, int *ret_apply)
{
	struct of_changeset_entry *ce;
	int ret, ret_tmp;

	pr_debug("changeset: reverting...\n");
	list_for_each_entry_reverse(ce, &ocs->entries, node) {
		ret = __of_changeset_entry_revert(ce);
		if (ret) {
			pr_err("Error reverting changeset (%d)\n", ret);
			list_for_each_entry_continue(ce, &ocs->entries, node) {
				ret_tmp = __of_changeset_entry_apply(ce);
				if (ret_tmp)
					*ret_apply = ret_tmp;
			}
			return ret;
		}
	}

	return 0;
}

 
int __of_changeset_revert_notify(struct of_changeset *ocs)
{
	struct of_changeset_entry *ce;
	int ret = 0, ret_tmp;

	pr_debug("changeset: emitting notifiers.\n");

	 
	mutex_unlock(&of_mutex);
	list_for_each_entry_reverse(ce, &ocs->entries, node) {
		ret_tmp = __of_changeset_entry_notify(ce, 1);
		if (ret_tmp)
			ret = ret_tmp;
	}
	mutex_lock(&of_mutex);
	pr_debug("changeset: notifiers sent.\n");

	return ret;
}

static int __of_changeset_revert(struct of_changeset *ocs)
{
	int ret, ret_reply;

	ret_reply = 0;
	ret = __of_changeset_revert_entries(ocs, &ret_reply);

	if (!ret)
		ret = __of_changeset_revert_notify(ocs);

	return ret;
}

 
int of_changeset_revert(struct of_changeset *ocs)
{
	int ret;

	mutex_lock(&of_mutex);
	ret = __of_changeset_revert(ocs);
	mutex_unlock(&of_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(of_changeset_revert);

 
int of_changeset_action(struct of_changeset *ocs, unsigned long action,
		struct device_node *np, struct property *prop)
{
	struct of_changeset_entry *ce;

	if (WARN_ON(action >= ARRAY_SIZE(action_names)))
		return -EINVAL;

	ce = kzalloc(sizeof(*ce), GFP_KERNEL);
	if (!ce)
		return -ENOMEM;

	 
	ce->action = action;
	ce->np = of_node_get(np);
	ce->prop = prop;

	 
	list_add_tail(&ce->node, &ocs->entries);
	return 0;
}
EXPORT_SYMBOL_GPL(of_changeset_action);

static int of_changeset_add_prop_helper(struct of_changeset *ocs,
					struct device_node *np,
					const struct property *pp)
{
	struct property *new_pp;
	int ret;

	new_pp = __of_prop_dup(pp, GFP_KERNEL);
	if (!new_pp)
		return -ENOMEM;

	ret = of_changeset_add_property(ocs, np, new_pp);
	if (ret) {
		kfree(new_pp->name);
		kfree(new_pp->value);
		kfree(new_pp);
	}

	return ret;
}

 
int of_changeset_add_prop_string(struct of_changeset *ocs,
				 struct device_node *np,
				 const char *prop_name, const char *str)
{
	struct property prop;

	prop.name = (char *)prop_name;
	prop.length = strlen(str) + 1;
	prop.value = (void *)str;

	return of_changeset_add_prop_helper(ocs, np, &prop);
}
EXPORT_SYMBOL_GPL(of_changeset_add_prop_string);

 
int of_changeset_add_prop_string_array(struct of_changeset *ocs,
				       struct device_node *np,
				       const char *prop_name,
				       const char **str_array, size_t sz)
{
	struct property prop;
	int i, ret;
	char *vp;

	prop.name = (char *)prop_name;

	prop.length = 0;
	for (i = 0; i < sz; i++)
		prop.length += strlen(str_array[i]) + 1;

	prop.value = kmalloc(prop.length, GFP_KERNEL);
	if (!prop.value)
		return -ENOMEM;

	vp = prop.value;
	for (i = 0; i < sz; i++) {
		vp += snprintf(vp, (char *)prop.value + prop.length - vp, "%s",
			       str_array[i]) + 1;
	}
	ret = of_changeset_add_prop_helper(ocs, np, &prop);
	kfree(prop.value);

	return ret;
}
EXPORT_SYMBOL_GPL(of_changeset_add_prop_string_array);

 
int of_changeset_add_prop_u32_array(struct of_changeset *ocs,
				    struct device_node *np,
				    const char *prop_name,
				    const u32 *array, size_t sz)
{
	struct property prop;
	__be32 *val;
	int i, ret;

	val = kcalloc(sz, sizeof(__be32), GFP_KERNEL);
	if (!val)
		return -ENOMEM;

	for (i = 0; i < sz; i++)
		val[i] = cpu_to_be32(array[i]);
	prop.name = (char *)prop_name;
	prop.length = sizeof(u32) * sz;
	prop.value = (void *)val;

	ret = of_changeset_add_prop_helper(ocs, np, &prop);
	kfree(val);

	return ret;
}
EXPORT_SYMBOL_GPL(of_changeset_add_prop_u32_array);
