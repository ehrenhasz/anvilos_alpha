
 

#define pr_fmt(fmt)	"OF: overlay: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/libfdt.h>
#include <linux/err.h>
#include <linux/idr.h>

#include "of_private.h"

 
struct target {
	struct device_node *np;
	bool in_livetree;
};

 
struct fragment {
	struct device_node *overlay;
	struct device_node *target;
};

 
struct overlay_changeset {
	int id;
	struct list_head ovcs_list;
	const void *new_fdt;
	const void *overlay_mem;
	struct device_node *overlay_root;
	enum of_overlay_notify_action notify_state;
	int count;
	struct fragment *fragments;
	bool symbols_fragment;
	struct of_changeset cset;
};

 
static int devicetree_state_flags;
#define DTSF_APPLY_FAIL		0x01
#define DTSF_REVERT_FAIL	0x02

 
static int devicetree_corrupt(void)
{
	return devicetree_state_flags &
		(DTSF_APPLY_FAIL | DTSF_REVERT_FAIL);
}

static int build_changeset_next_level(struct overlay_changeset *ovcs,
		struct target *target, const struct device_node *overlay_node);

 
static DEFINE_MUTEX(of_overlay_phandle_mutex);

void of_overlay_mutex_lock(void)
{
	mutex_lock(&of_overlay_phandle_mutex);
}

void of_overlay_mutex_unlock(void)
{
	mutex_unlock(&of_overlay_phandle_mutex);
}

static LIST_HEAD(ovcs_list);
static DEFINE_IDR(ovcs_idr);

static BLOCKING_NOTIFIER_HEAD(overlay_notify_chain);

 
int of_overlay_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&overlay_notify_chain, nb);
}
EXPORT_SYMBOL_GPL(of_overlay_notifier_register);

 
int of_overlay_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&overlay_notify_chain, nb);
}
EXPORT_SYMBOL_GPL(of_overlay_notifier_unregister);

static int overlay_notify(struct overlay_changeset *ovcs,
		enum of_overlay_notify_action action)
{
	struct of_overlay_notify_data nd;
	int i, ret;

	ovcs->notify_state = action;

	for (i = 0; i < ovcs->count; i++) {
		struct fragment *fragment = &ovcs->fragments[i];

		nd.target = fragment->target;
		nd.overlay = fragment->overlay;

		ret = blocking_notifier_call_chain(&overlay_notify_chain,
						   action, &nd);
		if (notifier_to_errno(ret)) {
			ret = notifier_to_errno(ret);
			pr_err("overlay changeset %s notifier error %d, target: %pOF\n",
			       of_overlay_action_name(action), ret, nd.target);
			return ret;
		}
	}

	return 0;
}

 
static struct property *dup_and_fixup_symbol_prop(
		struct overlay_changeset *ovcs, const struct property *prop)
{
	struct fragment *fragment;
	struct property *new_prop;
	struct device_node *fragment_node;
	struct device_node *overlay_node;
	const char *path;
	const char *path_tail;
	const char *target_path;
	int k;
	int overlay_name_len;
	int path_len;
	int path_tail_len;
	int target_path_len;

	if (!prop->value)
		return NULL;
	if (strnlen(prop->value, prop->length) >= prop->length)
		return NULL;
	path = prop->value;
	path_len = strlen(path);

	if (path_len < 1)
		return NULL;
	fragment_node = __of_find_node_by_path(ovcs->overlay_root, path + 1);
	overlay_node = __of_find_node_by_path(fragment_node, "__overlay__/");
	of_node_put(fragment_node);
	of_node_put(overlay_node);

	for (k = 0; k < ovcs->count; k++) {
		fragment = &ovcs->fragments[k];
		if (fragment->overlay == overlay_node)
			break;
	}
	if (k >= ovcs->count)
		return NULL;

	overlay_name_len = snprintf(NULL, 0, "%pOF", fragment->overlay);

	if (overlay_name_len > path_len)
		return NULL;
	path_tail = path + overlay_name_len;
	path_tail_len = strlen(path_tail);

	target_path = kasprintf(GFP_KERNEL, "%pOF", fragment->target);
	if (!target_path)
		return NULL;
	target_path_len = strlen(target_path);

	new_prop = kzalloc(sizeof(*new_prop), GFP_KERNEL);
	if (!new_prop)
		goto err_free_target_path;

	new_prop->name = kstrdup(prop->name, GFP_KERNEL);
	new_prop->length = target_path_len + path_tail_len + 1;
	new_prop->value = kzalloc(new_prop->length, GFP_KERNEL);
	if (!new_prop->name || !new_prop->value)
		goto err_free_new_prop;

	strcpy(new_prop->value, target_path);
	strcpy(new_prop->value + target_path_len, path_tail);

	of_property_set_flag(new_prop, OF_DYNAMIC);

	kfree(target_path);

	return new_prop;

err_free_new_prop:
	kfree(new_prop->name);
	kfree(new_prop->value);
	kfree(new_prop);
err_free_target_path:
	kfree(target_path);

	return NULL;
}

 
static int add_changeset_property(struct overlay_changeset *ovcs,
		struct target *target, struct property *overlay_prop,
		bool is_symbols_prop)
{
	struct property *new_prop = NULL, *prop;
	int ret = 0;

	if (target->in_livetree)
		if (!of_prop_cmp(overlay_prop->name, "name") ||
		    !of_prop_cmp(overlay_prop->name, "phandle") ||
		    !of_prop_cmp(overlay_prop->name, "linux,phandle"))
			return 0;

	if (target->in_livetree)
		prop = of_find_property(target->np, overlay_prop->name, NULL);
	else
		prop = NULL;

	if (prop) {
		if (!of_prop_cmp(prop->name, "#address-cells")) {
			if (!of_prop_val_eq(prop, overlay_prop)) {
				pr_err("ERROR: changing value of #address-cells is not allowed in %pOF\n",
				       target->np);
				ret = -EINVAL;
			}
			return ret;

		} else if (!of_prop_cmp(prop->name, "#size-cells")) {
			if (!of_prop_val_eq(prop, overlay_prop)) {
				pr_err("ERROR: changing value of #size-cells is not allowed in %pOF\n",
				       target->np);
				ret = -EINVAL;
			}
			return ret;
		}
	}

	if (is_symbols_prop) {
		if (prop)
			return -EINVAL;
		new_prop = dup_and_fixup_symbol_prop(ovcs, overlay_prop);
	} else {
		new_prop = __of_prop_dup(overlay_prop, GFP_KERNEL);
	}

	if (!new_prop)
		return -ENOMEM;

	if (!prop) {
		if (!target->in_livetree) {
			new_prop->next = target->np->deadprops;
			target->np->deadprops = new_prop;
		}
		ret = of_changeset_add_property(&ovcs->cset, target->np,
						new_prop);
	} else {
		ret = of_changeset_update_property(&ovcs->cset, target->np,
						   new_prop);
	}

	if (!of_node_check_flag(target->np, OF_OVERLAY))
		pr_err("WARNING: memory leak will occur if overlay removed, property: %pOF/%s\n",
		       target->np, new_prop->name);

	if (ret) {
		kfree(new_prop->name);
		kfree(new_prop->value);
		kfree(new_prop);
	}
	return ret;
}

 
static int add_changeset_node(struct overlay_changeset *ovcs,
		struct target *target, struct device_node *node)
{
	const char *node_kbasename;
	const __be32 *phandle;
	struct device_node *tchild;
	struct target target_child;
	int ret = 0, size;

	node_kbasename = kbasename(node->full_name);

	for_each_child_of_node(target->np, tchild)
		if (!of_node_cmp(node_kbasename, kbasename(tchild->full_name)))
			break;

	if (!tchild) {
		tchild = __of_node_dup(NULL, node_kbasename);
		if (!tchild)
			return -ENOMEM;

		tchild->parent = target->np;
		tchild->name = __of_get_property(node, "name", NULL);

		if (!tchild->name)
			tchild->name = "<NULL>";

		 
		phandle = __of_get_property(node, "phandle", &size);
		if (phandle && (size == 4))
			tchild->phandle = be32_to_cpup(phandle);

		of_node_set_flag(tchild, OF_OVERLAY);

		ret = of_changeset_attach_node(&ovcs->cset, tchild);
		if (ret)
			return ret;

		target_child.np = tchild;
		target_child.in_livetree = false;

		ret = build_changeset_next_level(ovcs, &target_child, node);
		of_node_put(tchild);
		return ret;
	}

	if (node->phandle && tchild->phandle) {
		ret = -EINVAL;
	} else {
		target_child.np = tchild;
		target_child.in_livetree = target->in_livetree;
		ret = build_changeset_next_level(ovcs, &target_child, node);
	}
	of_node_put(tchild);

	return ret;
}

 
static int build_changeset_next_level(struct overlay_changeset *ovcs,
		struct target *target, const struct device_node *overlay_node)
{
	struct device_node *child;
	struct property *prop;
	int ret;

	for_each_property_of_node(overlay_node, prop) {
		ret = add_changeset_property(ovcs, target, prop, 0);
		if (ret) {
			pr_debug("Failed to apply prop @%pOF/%s, err=%d\n",
				 target->np, prop->name, ret);
			return ret;
		}
	}

	for_each_child_of_node(overlay_node, child) {
		ret = add_changeset_node(ovcs, target, child);
		if (ret) {
			pr_debug("Failed to apply node @%pOF/%pOFn, err=%d\n",
				 target->np, child, ret);
			of_node_put(child);
			return ret;
		}
	}

	return 0;
}

 
static int build_changeset_symbols_node(struct overlay_changeset *ovcs,
		struct target *target,
		const struct device_node *overlay_symbols_node)
{
	struct property *prop;
	int ret;

	for_each_property_of_node(overlay_symbols_node, prop) {
		ret = add_changeset_property(ovcs, target, prop, 1);
		if (ret) {
			pr_debug("Failed to apply symbols prop @%pOF/%s, err=%d\n",
				 target->np, prop->name, ret);
			return ret;
		}
	}

	return 0;
}

static int find_dup_cset_node_entry(struct overlay_changeset *ovcs,
		struct of_changeset_entry *ce_1)
{
	struct of_changeset_entry *ce_2;
	char *fn_1, *fn_2;
	int node_path_match;

	if (ce_1->action != OF_RECONFIG_ATTACH_NODE &&
	    ce_1->action != OF_RECONFIG_DETACH_NODE)
		return 0;

	ce_2 = ce_1;
	list_for_each_entry_continue(ce_2, &ovcs->cset.entries, node) {
		if ((ce_2->action != OF_RECONFIG_ATTACH_NODE &&
		     ce_2->action != OF_RECONFIG_DETACH_NODE) ||
		    of_node_cmp(ce_1->np->full_name, ce_2->np->full_name))
			continue;

		fn_1 = kasprintf(GFP_KERNEL, "%pOF", ce_1->np);
		fn_2 = kasprintf(GFP_KERNEL, "%pOF", ce_2->np);
		node_path_match = !fn_1 || !fn_2 || !strcmp(fn_1, fn_2);
		kfree(fn_1);
		kfree(fn_2);
		if (node_path_match) {
			pr_err("ERROR: multiple fragments add and/or delete node %pOF\n",
			       ce_1->np);
			return -EINVAL;
		}
	}

	return 0;
}

static int find_dup_cset_prop(struct overlay_changeset *ovcs,
		struct of_changeset_entry *ce_1)
{
	struct of_changeset_entry *ce_2;
	char *fn_1, *fn_2;
	int node_path_match;

	if (ce_1->action != OF_RECONFIG_ADD_PROPERTY &&
	    ce_1->action != OF_RECONFIG_REMOVE_PROPERTY &&
	    ce_1->action != OF_RECONFIG_UPDATE_PROPERTY)
		return 0;

	ce_2 = ce_1;
	list_for_each_entry_continue(ce_2, &ovcs->cset.entries, node) {
		if ((ce_2->action != OF_RECONFIG_ADD_PROPERTY &&
		     ce_2->action != OF_RECONFIG_REMOVE_PROPERTY &&
		     ce_2->action != OF_RECONFIG_UPDATE_PROPERTY) ||
		    of_node_cmp(ce_1->np->full_name, ce_2->np->full_name))
			continue;

		fn_1 = kasprintf(GFP_KERNEL, "%pOF", ce_1->np);
		fn_2 = kasprintf(GFP_KERNEL, "%pOF", ce_2->np);
		node_path_match = !fn_1 || !fn_2 || !strcmp(fn_1, fn_2);
		kfree(fn_1);
		kfree(fn_2);
		if (node_path_match &&
		    !of_prop_cmp(ce_1->prop->name, ce_2->prop->name)) {
			pr_err("ERROR: multiple fragments add, update, and/or delete property %pOF/%s\n",
			       ce_1->np, ce_1->prop->name);
			return -EINVAL;
		}
	}

	return 0;
}

 
static int changeset_dup_entry_check(struct overlay_changeset *ovcs)
{
	struct of_changeset_entry *ce_1;
	int dup_entry = 0;

	list_for_each_entry(ce_1, &ovcs->cset.entries, node) {
		dup_entry |= find_dup_cset_node_entry(ovcs, ce_1);
		dup_entry |= find_dup_cset_prop(ovcs, ce_1);
	}

	return dup_entry ? -EINVAL : 0;
}

 
static int build_changeset(struct overlay_changeset *ovcs)
{
	struct fragment *fragment;
	struct target target;
	int fragments_count, i, ret;

	 
	if (ovcs->symbols_fragment)
		fragments_count = ovcs->count - 1;
	else
		fragments_count = ovcs->count;

	for (i = 0; i < fragments_count; i++) {
		fragment = &ovcs->fragments[i];

		target.np = fragment->target;
		target.in_livetree = true;
		ret = build_changeset_next_level(ovcs, &target,
						 fragment->overlay);
		if (ret) {
			pr_debug("fragment apply failed '%pOF'\n",
				 fragment->target);
			return ret;
		}
	}

	if (ovcs->symbols_fragment) {
		fragment = &ovcs->fragments[ovcs->count - 1];

		target.np = fragment->target;
		target.in_livetree = true;
		ret = build_changeset_symbols_node(ovcs, &target,
						   fragment->overlay);
		if (ret) {
			pr_debug("symbols fragment apply failed '%pOF'\n",
				 fragment->target);
			return ret;
		}
	}

	return changeset_dup_entry_check(ovcs);
}

 
static struct device_node *find_target(struct device_node *info_node,
				       struct device_node *target_base)
{
	struct device_node *node;
	char *target_path;
	const char *path;
	u32 val;
	int ret;

	ret = of_property_read_u32(info_node, "target", &val);
	if (!ret) {
		node = of_find_node_by_phandle(val);
		if (!node)
			pr_err("find target, node: %pOF, phandle 0x%x not found\n",
			       info_node, val);
		return node;
	}

	ret = of_property_read_string(info_node, "target-path", &path);
	if (!ret) {
		if (target_base) {
			target_path = kasprintf(GFP_KERNEL, "%pOF%s", target_base, path);
			if (!target_path)
				return NULL;
			node = of_find_node_by_path(target_path);
			if (!node) {
				pr_err("find target, node: %pOF, path '%s' not found\n",
				       info_node, target_path);
			}
			kfree(target_path);
		} else {
			node =  of_find_node_by_path(path);
			if (!node) {
				pr_err("find target, node: %pOF, path '%s' not found\n",
				       info_node, path);
			}
		}
		return node;
	}

	pr_err("find target, node: %pOF, no target property\n", info_node);

	return NULL;
}

 
static int init_overlay_changeset(struct overlay_changeset *ovcs,
				  struct device_node *target_base)
{
	struct device_node *node, *overlay_node;
	struct fragment *fragment;
	struct fragment *fragments;
	int cnt, ret;

	 

	 
	if (!of_node_check_flag(ovcs->overlay_root, OF_DYNAMIC))
		pr_debug("%s() ovcs->overlay_root is not dynamic\n", __func__);

	if (!of_node_check_flag(ovcs->overlay_root, OF_DETACHED))
		pr_debug("%s() ovcs->overlay_root is not detached\n", __func__);

	if (!of_node_is_root(ovcs->overlay_root))
		pr_debug("%s() ovcs->overlay_root is not root\n", __func__);

	cnt = 0;

	 
	for_each_child_of_node(ovcs->overlay_root, node) {
		overlay_node = of_get_child_by_name(node, "__overlay__");
		if (overlay_node) {
			cnt++;
			of_node_put(overlay_node);
		}
	}

	node = of_get_child_by_name(ovcs->overlay_root, "__symbols__");
	if (node) {
		cnt++;
		of_node_put(node);
	}

	fragments = kcalloc(cnt, sizeof(*fragments), GFP_KERNEL);
	if (!fragments) {
		ret = -ENOMEM;
		goto err_out;
	}
	ovcs->fragments = fragments;

	cnt = 0;
	for_each_child_of_node(ovcs->overlay_root, node) {
		overlay_node = of_get_child_by_name(node, "__overlay__");
		if (!overlay_node)
			continue;

		fragment = &fragments[cnt];
		fragment->overlay = overlay_node;
		fragment->target = find_target(node, target_base);
		if (!fragment->target) {
			of_node_put(fragment->overlay);
			ret = -EINVAL;
			of_node_put(node);
			goto err_out;
		}

		cnt++;
	}

	 
	node = of_get_child_by_name(ovcs->overlay_root, "__symbols__");
	if (node) {
		ovcs->symbols_fragment = 1;
		fragment = &fragments[cnt];
		fragment->overlay = node;
		fragment->target = of_find_node_by_path("/__symbols__");

		if (!fragment->target) {
			pr_err("symbols in overlay, but not in live tree\n");
			ret = -EINVAL;
			of_node_put(node);
			goto err_out;
		}

		cnt++;
	}

	if (!cnt) {
		pr_err("no fragments or symbols in overlay\n");
		ret = -EINVAL;
		goto err_out;
	}

	ovcs->count = cnt;

	return 0;

err_out:
	pr_err("%s() failed, ret = %d\n", __func__, ret);

	return ret;
}

static void free_overlay_changeset(struct overlay_changeset *ovcs)
{
	int i;

	if (ovcs->cset.entries.next)
		of_changeset_destroy(&ovcs->cset);

	if (ovcs->id) {
		idr_remove(&ovcs_idr, ovcs->id);
		list_del(&ovcs->ovcs_list);
		ovcs->id = 0;
	}


	for (i = 0; i < ovcs->count; i++) {
		of_node_put(ovcs->fragments[i].target);
		of_node_put(ovcs->fragments[i].overlay);
	}
	kfree(ovcs->fragments);

	 

	if (ovcs->notify_state == OF_OVERLAY_INIT ||
	    ovcs->notify_state == OF_OVERLAY_POST_REMOVE) {
		kfree(ovcs->overlay_mem);
		kfree(ovcs->new_fdt);
	}
	kfree(ovcs);
}

 

static int of_overlay_apply(struct overlay_changeset *ovcs,
			    struct device_node *base)
{
	int ret = 0, ret_revert, ret_tmp;

	ret = of_resolve_phandles(ovcs->overlay_root);
	if (ret)
		goto out;

	ret = init_overlay_changeset(ovcs, base);
	if (ret)
		goto out;

	ret = overlay_notify(ovcs, OF_OVERLAY_PRE_APPLY);
	if (ret)
		goto out;

	ret = build_changeset(ovcs);
	if (ret)
		goto out;

	ret_revert = 0;
	ret = __of_changeset_apply_entries(&ovcs->cset, &ret_revert);
	if (ret) {
		if (ret_revert) {
			pr_debug("overlay changeset revert error %d\n",
				 ret_revert);
			devicetree_state_flags |= DTSF_APPLY_FAIL;
		}
		goto out;
	}

	ret = __of_changeset_apply_notify(&ovcs->cset);
	if (ret)
		pr_err("overlay apply changeset entry notify error %d\n", ret);
	 

	ret_tmp = overlay_notify(ovcs, OF_OVERLAY_POST_APPLY);
	if (ret_tmp)
		if (!ret)
			ret = ret_tmp;

out:
	pr_debug("%s() err=%d\n", __func__, ret);

	return ret;
}

 

int of_overlay_fdt_apply(const void *overlay_fdt, u32 overlay_fdt_size,
			 int *ret_ovcs_id, struct device_node *base)
{
	void *new_fdt;
	void *new_fdt_align;
	void *overlay_mem;
	int ret;
	u32 size;
	struct overlay_changeset *ovcs;

	*ret_ovcs_id = 0;

	if (devicetree_corrupt()) {
		pr_err("devicetree state suspect, refuse to apply overlay\n");
		return -EBUSY;
	}

	if (overlay_fdt_size < sizeof(struct fdt_header) ||
	    fdt_check_header(overlay_fdt)) {
		pr_err("Invalid overlay_fdt header\n");
		return -EINVAL;
	}

	size = fdt_totalsize(overlay_fdt);
	if (overlay_fdt_size < size)
		return -EINVAL;

	ovcs = kzalloc(sizeof(*ovcs), GFP_KERNEL);
	if (!ovcs)
		return -ENOMEM;

	of_overlay_mutex_lock();
	mutex_lock(&of_mutex);

	 

	ovcs->id = idr_alloc(&ovcs_idr, ovcs, 1, 0, GFP_KERNEL);
	if (ovcs->id <= 0) {
		ret = ovcs->id;
		goto err_free_ovcs;
	}

	INIT_LIST_HEAD(&ovcs->ovcs_list);
	list_add_tail(&ovcs->ovcs_list, &ovcs_list);
	of_changeset_init(&ovcs->cset);

	 
	new_fdt = kmalloc(size + FDT_ALIGN_SIZE, GFP_KERNEL);
	if (!new_fdt) {
		ret = -ENOMEM;
		goto err_free_ovcs;
	}
	ovcs->new_fdt = new_fdt;

	new_fdt_align = PTR_ALIGN(new_fdt, FDT_ALIGN_SIZE);
	memcpy(new_fdt_align, overlay_fdt, size);

	overlay_mem = of_fdt_unflatten_tree(new_fdt_align, NULL,
					    &ovcs->overlay_root);
	if (!overlay_mem) {
		pr_err("unable to unflatten overlay_fdt\n");
		ret = -EINVAL;
		goto err_free_ovcs;
	}
	ovcs->overlay_mem = overlay_mem;

	ret = of_overlay_apply(ovcs, base);
	 
	*ret_ovcs_id = ovcs->id;
	goto out_unlock;

err_free_ovcs:
	free_overlay_changeset(ovcs);

out_unlock:
	mutex_unlock(&of_mutex);
	of_overlay_mutex_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(of_overlay_fdt_apply);

 
static int find_node(struct device_node *tree, struct device_node *np)
{
	struct device_node *child;

	if (tree == np)
		return 1;

	for_each_child_of_node(tree, child) {
		if (find_node(child, np)) {
			of_node_put(child);
			return 1;
		}
	}

	return 0;
}

 
static int node_overlaps_later_cs(struct overlay_changeset *remove_ovcs,
		struct device_node *remove_ce_node)
{
	struct overlay_changeset *ovcs;
	struct of_changeset_entry *ce;

	list_for_each_entry_reverse(ovcs, &ovcs_list, ovcs_list) {
		if (ovcs == remove_ovcs)
			break;

		list_for_each_entry(ce, &ovcs->cset.entries, node) {
			if (find_node(ce->np, remove_ce_node)) {
				pr_err("%s: #%d overlaps with #%d @%pOF\n",
					__func__, remove_ovcs->id, ovcs->id,
					remove_ce_node);
				return 1;
			}
			if (find_node(remove_ce_node, ce->np)) {
				pr_err("%s: #%d overlaps with #%d @%pOF\n",
					__func__, remove_ovcs->id, ovcs->id,
					remove_ce_node);
				return 1;
			}
		}
	}

	return 0;
}

 
static int overlay_removal_is_ok(struct overlay_changeset *remove_ovcs)
{
	struct of_changeset_entry *remove_ce;

	list_for_each_entry(remove_ce, &remove_ovcs->cset.entries, node) {
		if (node_overlaps_later_cs(remove_ovcs, remove_ce->np)) {
			pr_err("overlay #%d is not topmost\n", remove_ovcs->id);
			return 0;
		}
	}

	return 1;
}

 
int of_overlay_remove(int *ovcs_id)
{
	struct overlay_changeset *ovcs;
	int ret, ret_apply, ret_tmp;

	if (devicetree_corrupt()) {
		pr_err("suspect devicetree state, refuse to remove overlay\n");
		ret = -EBUSY;
		goto out;
	}

	mutex_lock(&of_mutex);

	ovcs = idr_find(&ovcs_idr, *ovcs_id);
	if (!ovcs) {
		ret = -ENODEV;
		pr_err("remove: Could not find overlay #%d\n", *ovcs_id);
		goto err_unlock;
	}

	if (!overlay_removal_is_ok(ovcs)) {
		ret = -EBUSY;
		goto err_unlock;
	}

	ret = overlay_notify(ovcs, OF_OVERLAY_PRE_REMOVE);
	if (ret)
		goto err_unlock;

	ret_apply = 0;
	ret = __of_changeset_revert_entries(&ovcs->cset, &ret_apply);
	if (ret) {
		if (ret_apply)
			devicetree_state_flags |= DTSF_REVERT_FAIL;
		goto err_unlock;
	}

	ret = __of_changeset_revert_notify(&ovcs->cset);
	if (ret)
		pr_err("overlay remove changeset entry notify error %d\n", ret);
	 

	*ovcs_id = 0;

	 
	ret_tmp = overlay_notify(ovcs, OF_OVERLAY_POST_REMOVE);
	if (ret_tmp)
		if (!ret)
			ret = ret_tmp;

	free_overlay_changeset(ovcs);

err_unlock:
	 
	mutex_unlock(&of_mutex);

out:
	pr_debug("%s() err=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(of_overlay_remove);

 
int of_overlay_remove_all(void)
{
	struct overlay_changeset *ovcs, *ovcs_n;
	int ret;

	 
	list_for_each_entry_safe_reverse(ovcs, ovcs_n, &ovcs_list, ovcs_list) {
		ret = of_overlay_remove(&ovcs->id);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(of_overlay_remove_all);
