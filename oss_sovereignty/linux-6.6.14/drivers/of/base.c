
 

#define pr_fmt(fmt)	"OF: " fmt

#include <linux/console.h>
#include <linux/ctype.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/proc_fs.h>

#include "of_private.h"

LIST_HEAD(aliases_lookup);

struct device_node *of_root;
EXPORT_SYMBOL(of_root);
struct device_node *of_chosen;
EXPORT_SYMBOL(of_chosen);
struct device_node *of_aliases;
struct device_node *of_stdout;
static const char *of_stdout_options;

struct kset *of_kset;

 
DEFINE_MUTEX(of_mutex);

 
DEFINE_RAW_SPINLOCK(devtree_lock);

bool of_node_name_eq(const struct device_node *np, const char *name)
{
	const char *node_name;
	size_t len;

	if (!np)
		return false;

	node_name = kbasename(np->full_name);
	len = strchrnul(node_name, '@') - node_name;

	return (strlen(name) == len) && (strncmp(node_name, name, len) == 0);
}
EXPORT_SYMBOL(of_node_name_eq);

bool of_node_name_prefix(const struct device_node *np, const char *prefix)
{
	if (!np)
		return false;

	return strncmp(kbasename(np->full_name), prefix, strlen(prefix)) == 0;
}
EXPORT_SYMBOL(of_node_name_prefix);

static bool __of_node_is_type(const struct device_node *np, const char *type)
{
	const char *match = __of_get_property(np, "device_type", NULL);

	return np && match && type && !strcmp(match, type);
}

int of_bus_n_addr_cells(struct device_node *np)
{
	u32 cells;

	for (; np; np = np->parent)
		if (!of_property_read_u32(np, "#address-cells", &cells))
			return cells;

	 
	return OF_ROOT_NODE_ADDR_CELLS_DEFAULT;
}

int of_n_addr_cells(struct device_node *np)
{
	if (np->parent)
		np = np->parent;

	return of_bus_n_addr_cells(np);
}
EXPORT_SYMBOL(of_n_addr_cells);

int of_bus_n_size_cells(struct device_node *np)
{
	u32 cells;

	for (; np; np = np->parent)
		if (!of_property_read_u32(np, "#size-cells", &cells))
			return cells;

	 
	return OF_ROOT_NODE_SIZE_CELLS_DEFAULT;
}

int of_n_size_cells(struct device_node *np)
{
	if (np->parent)
		np = np->parent;

	return of_bus_n_size_cells(np);
}
EXPORT_SYMBOL(of_n_size_cells);

#ifdef CONFIG_NUMA
int __weak of_node_to_nid(struct device_node *np)
{
	return NUMA_NO_NODE;
}
#endif

#define OF_PHANDLE_CACHE_BITS	7
#define OF_PHANDLE_CACHE_SZ	BIT(OF_PHANDLE_CACHE_BITS)

static struct device_node *phandle_cache[OF_PHANDLE_CACHE_SZ];

static u32 of_phandle_cache_hash(phandle handle)
{
	return hash_32(handle, OF_PHANDLE_CACHE_BITS);
}

 
void __of_phandle_cache_inv_entry(phandle handle)
{
	u32 handle_hash;
	struct device_node *np;

	if (!handle)
		return;

	handle_hash = of_phandle_cache_hash(handle);

	np = phandle_cache[handle_hash];
	if (np && handle == np->phandle)
		phandle_cache[handle_hash] = NULL;
}

void __init of_core_init(void)
{
	struct device_node *np;

	of_platform_register_reconfig_notifier();

	 
	mutex_lock(&of_mutex);
	of_kset = kset_create_and_add("devicetree", NULL, firmware_kobj);
	if (!of_kset) {
		mutex_unlock(&of_mutex);
		pr_err("failed to register existing nodes\n");
		return;
	}
	for_each_of_allnodes(np) {
		__of_attach_node_sysfs(np);
		if (np->phandle && !phandle_cache[of_phandle_cache_hash(np->phandle)])
			phandle_cache[of_phandle_cache_hash(np->phandle)] = np;
	}
	mutex_unlock(&of_mutex);

	 
	if (of_root)
		proc_symlink("device-tree", NULL, "/sys/firmware/devicetree/base");
}

static struct property *__of_find_property(const struct device_node *np,
					   const char *name, int *lenp)
{
	struct property *pp;

	if (!np)
		return NULL;

	for (pp = np->properties; pp; pp = pp->next) {
		if (of_prop_cmp(pp->name, name) == 0) {
			if (lenp)
				*lenp = pp->length;
			break;
		}
	}

	return pp;
}

struct property *of_find_property(const struct device_node *np,
				  const char *name,
				  int *lenp)
{
	struct property *pp;
	unsigned long flags;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	pp = __of_find_property(np, name, lenp);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);

	return pp;
}
EXPORT_SYMBOL(of_find_property);

struct device_node *__of_find_all_nodes(struct device_node *prev)
{
	struct device_node *np;
	if (!prev) {
		np = of_root;
	} else if (prev->child) {
		np = prev->child;
	} else {
		 
		np = prev;
		while (np->parent && !np->sibling)
			np = np->parent;
		np = np->sibling;  
	}
	return np;
}

 
struct device_node *of_find_all_nodes(struct device_node *prev)
{
	struct device_node *np;
	unsigned long flags;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	np = __of_find_all_nodes(prev);
	of_node_get(np);
	of_node_put(prev);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return np;
}
EXPORT_SYMBOL(of_find_all_nodes);

 
const void *__of_get_property(const struct device_node *np,
			      const char *name, int *lenp)
{
	struct property *pp = __of_find_property(np, name, lenp);

	return pp ? pp->value : NULL;
}

 
const void *of_get_property(const struct device_node *np, const char *name,
			    int *lenp)
{
	struct property *pp = of_find_property(np, name, lenp);

	return pp ? pp->value : NULL;
}
EXPORT_SYMBOL(of_get_property);

 
static int __of_device_is_compatible(const struct device_node *device,
				     const char *compat, const char *type, const char *name)
{
	struct property *prop;
	const char *cp;
	int index = 0, score = 0;

	 
	if (compat && compat[0]) {
		prop = __of_find_property(device, "compatible", NULL);
		for (cp = of_prop_next_string(prop, NULL); cp;
		     cp = of_prop_next_string(prop, cp), index++) {
			if (of_compat_cmp(cp, compat, strlen(compat)) == 0) {
				score = INT_MAX/2 - (index << 2);
				break;
			}
		}
		if (!score)
			return 0;
	}

	 
	if (type && type[0]) {
		if (!__of_node_is_type(device, type))
			return 0;
		score += 2;
	}

	 
	if (name && name[0]) {
		if (!of_node_name_eq(device, name))
			return 0;
		score++;
	}

	return score;
}

 
int of_device_is_compatible(const struct device_node *device,
		const char *compat)
{
	unsigned long flags;
	int res;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	res = __of_device_is_compatible(device, compat, NULL, NULL);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return res;
}
EXPORT_SYMBOL(of_device_is_compatible);

 
int of_device_compatible_match(const struct device_node *device,
			       const char *const *compat)
{
	unsigned int tmp, score = 0;

	if (!compat)
		return 0;

	while (*compat) {
		tmp = of_device_is_compatible(device, *compat);
		if (tmp > score)
			score = tmp;
		compat++;
	}

	return score;
}
EXPORT_SYMBOL_GPL(of_device_compatible_match);

 
int of_machine_is_compatible(const char *compat)
{
	struct device_node *root;
	int rc = 0;

	root = of_find_node_by_path("/");
	if (root) {
		rc = of_device_is_compatible(root, compat);
		of_node_put(root);
	}
	return rc;
}
EXPORT_SYMBOL(of_machine_is_compatible);

 
static bool __of_device_is_available(const struct device_node *device)
{
	const char *status;
	int statlen;

	if (!device)
		return false;

	status = __of_get_property(device, "status", &statlen);
	if (status == NULL)
		return true;

	if (statlen > 0) {
		if (!strcmp(status, "okay") || !strcmp(status, "ok"))
			return true;
	}

	return false;
}

 
bool of_device_is_available(const struct device_node *device)
{
	unsigned long flags;
	bool res;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	res = __of_device_is_available(device);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return res;

}
EXPORT_SYMBOL(of_device_is_available);

 
static bool __of_device_is_fail(const struct device_node *device)
{
	const char *status;

	if (!device)
		return false;

	status = __of_get_property(device, "status", NULL);
	if (status == NULL)
		return false;

	return !strcmp(status, "fail") || !strncmp(status, "fail-", 5);
}

 
bool of_device_is_big_endian(const struct device_node *device)
{
	if (of_property_read_bool(device, "big-endian"))
		return true;
	if (IS_ENABLED(CONFIG_CPU_BIG_ENDIAN) &&
	    of_property_read_bool(device, "native-endian"))
		return true;
	return false;
}
EXPORT_SYMBOL(of_device_is_big_endian);

 
struct device_node *of_get_parent(const struct device_node *node)
{
	struct device_node *np;
	unsigned long flags;

	if (!node)
		return NULL;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	np = of_node_get(node->parent);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return np;
}
EXPORT_SYMBOL(of_get_parent);

 
struct device_node *of_get_next_parent(struct device_node *node)
{
	struct device_node *parent;
	unsigned long flags;

	if (!node)
		return NULL;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	parent = of_node_get(node->parent);
	of_node_put(node);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return parent;
}
EXPORT_SYMBOL(of_get_next_parent);

static struct device_node *__of_get_next_child(const struct device_node *node,
						struct device_node *prev)
{
	struct device_node *next;

	if (!node)
		return NULL;

	next = prev ? prev->sibling : node->child;
	of_node_get(next);
	of_node_put(prev);
	return next;
}
#define __for_each_child_of_node(parent, child) \
	for (child = __of_get_next_child(parent, NULL); child != NULL; \
	     child = __of_get_next_child(parent, child))

 
struct device_node *of_get_next_child(const struct device_node *node,
	struct device_node *prev)
{
	struct device_node *next;
	unsigned long flags;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	next = __of_get_next_child(node, prev);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return next;
}
EXPORT_SYMBOL(of_get_next_child);

 
struct device_node *of_get_next_available_child(const struct device_node *node,
	struct device_node *prev)
{
	struct device_node *next;
	unsigned long flags;

	if (!node)
		return NULL;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	next = prev ? prev->sibling : node->child;
	for (; next; next = next->sibling) {
		if (!__of_device_is_available(next))
			continue;
		if (of_node_get(next))
			break;
	}
	of_node_put(prev);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return next;
}
EXPORT_SYMBOL(of_get_next_available_child);

 
struct device_node *of_get_next_cpu_node(struct device_node *prev)
{
	struct device_node *next = NULL;
	unsigned long flags;
	struct device_node *node;

	if (!prev)
		node = of_find_node_by_path("/cpus");

	raw_spin_lock_irqsave(&devtree_lock, flags);
	if (prev)
		next = prev->sibling;
	else if (node) {
		next = node->child;
		of_node_put(node);
	}
	for (; next; next = next->sibling) {
		if (__of_device_is_fail(next))
			continue;
		if (!(of_node_name_eq(next, "cpu") ||
		      __of_node_is_type(next, "cpu")))
			continue;
		if (of_node_get(next))
			break;
	}
	of_node_put(prev);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return next;
}
EXPORT_SYMBOL(of_get_next_cpu_node);

 
struct device_node *of_get_compatible_child(const struct device_node *parent,
				const char *compatible)
{
	struct device_node *child;

	for_each_child_of_node(parent, child) {
		if (of_device_is_compatible(child, compatible))
			break;
	}

	return child;
}
EXPORT_SYMBOL(of_get_compatible_child);

 
struct device_node *of_get_child_by_name(const struct device_node *node,
				const char *name)
{
	struct device_node *child;

	for_each_child_of_node(node, child)
		if (of_node_name_eq(child, name))
			break;
	return child;
}
EXPORT_SYMBOL(of_get_child_by_name);

struct device_node *__of_find_node_by_path(struct device_node *parent,
						const char *path)
{
	struct device_node *child;
	int len;

	len = strcspn(path, "/:");
	if (!len)
		return NULL;

	__for_each_child_of_node(parent, child) {
		const char *name = kbasename(child->full_name);
		if (strncmp(path, name, len) == 0 && (strlen(name) == len))
			return child;
	}
	return NULL;
}

struct device_node *__of_find_node_by_full_path(struct device_node *node,
						const char *path)
{
	const char *separator = strchr(path, ':');

	while (node && *path == '/') {
		struct device_node *tmp = node;

		path++;  
		node = __of_find_node_by_path(node, path);
		of_node_put(tmp);
		path = strchrnul(path, '/');
		if (separator && separator < path)
			break;
	}
	return node;
}

 
struct device_node *of_find_node_opts_by_path(const char *path, const char **opts)
{
	struct device_node *np = NULL;
	struct property *pp;
	unsigned long flags;
	const char *separator = strchr(path, ':');

	if (opts)
		*opts = separator ? separator + 1 : NULL;

	if (strcmp(path, "/") == 0)
		return of_node_get(of_root);

	 
	if (*path != '/') {
		int len;
		const char *p = separator;

		if (!p)
			p = strchrnul(path, '/');
		len = p - path;

		 
		if (!of_aliases)
			return NULL;

		for_each_property_of_node(of_aliases, pp) {
			if (strlen(pp->name) == len && !strncmp(pp->name, path, len)) {
				np = of_find_node_by_path(pp->value);
				break;
			}
		}
		if (!np)
			return NULL;
		path = p;
	}

	 
	raw_spin_lock_irqsave(&devtree_lock, flags);
	if (!np)
		np = of_node_get(of_root);
	np = __of_find_node_by_full_path(np, path);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return np;
}
EXPORT_SYMBOL(of_find_node_opts_by_path);

 
struct device_node *of_find_node_by_name(struct device_node *from,
	const char *name)
{
	struct device_node *np;
	unsigned long flags;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	for_each_of_allnodes_from(from, np)
		if (of_node_name_eq(np, name) && of_node_get(np))
			break;
	of_node_put(from);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return np;
}
EXPORT_SYMBOL(of_find_node_by_name);

 
struct device_node *of_find_node_by_type(struct device_node *from,
	const char *type)
{
	struct device_node *np;
	unsigned long flags;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	for_each_of_allnodes_from(from, np)
		if (__of_node_is_type(np, type) && of_node_get(np))
			break;
	of_node_put(from);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return np;
}
EXPORT_SYMBOL(of_find_node_by_type);

 
struct device_node *of_find_compatible_node(struct device_node *from,
	const char *type, const char *compatible)
{
	struct device_node *np;
	unsigned long flags;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	for_each_of_allnodes_from(from, np)
		if (__of_device_is_compatible(np, compatible, type, NULL) &&
		    of_node_get(np))
			break;
	of_node_put(from);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return np;
}
EXPORT_SYMBOL(of_find_compatible_node);

 
struct device_node *of_find_node_with_property(struct device_node *from,
	const char *prop_name)
{
	struct device_node *np;
	struct property *pp;
	unsigned long flags;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	for_each_of_allnodes_from(from, np) {
		for (pp = np->properties; pp; pp = pp->next) {
			if (of_prop_cmp(pp->name, prop_name) == 0) {
				of_node_get(np);
				goto out;
			}
		}
	}
out:
	of_node_put(from);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return np;
}
EXPORT_SYMBOL(of_find_node_with_property);

static
const struct of_device_id *__of_match_node(const struct of_device_id *matches,
					   const struct device_node *node)
{
	const struct of_device_id *best_match = NULL;
	int score, best_score = 0;

	if (!matches)
		return NULL;

	for (; matches->name[0] || matches->type[0] || matches->compatible[0]; matches++) {
		score = __of_device_is_compatible(node, matches->compatible,
						  matches->type, matches->name);
		if (score > best_score) {
			best_match = matches;
			best_score = score;
		}
	}

	return best_match;
}

 
const struct of_device_id *of_match_node(const struct of_device_id *matches,
					 const struct device_node *node)
{
	const struct of_device_id *match;
	unsigned long flags;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	match = __of_match_node(matches, node);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return match;
}
EXPORT_SYMBOL(of_match_node);

 
struct device_node *of_find_matching_node_and_match(struct device_node *from,
					const struct of_device_id *matches,
					const struct of_device_id **match)
{
	struct device_node *np;
	const struct of_device_id *m;
	unsigned long flags;

	if (match)
		*match = NULL;

	raw_spin_lock_irqsave(&devtree_lock, flags);
	for_each_of_allnodes_from(from, np) {
		m = __of_match_node(matches, np);
		if (m && of_node_get(np)) {
			if (match)
				*match = m;
			break;
		}
	}
	of_node_put(from);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return np;
}
EXPORT_SYMBOL(of_find_matching_node_and_match);

 
int of_alias_from_compatible(const struct device_node *node, char *alias, int len)
{
	const char *compatible, *p;
	int cplen;

	compatible = of_get_property(node, "compatible", &cplen);
	if (!compatible || strlen(compatible) > cplen)
		return -ENODEV;
	p = strchr(compatible, ',');
	strscpy(alias, p ? p + 1 : compatible, len);
	return 0;
}
EXPORT_SYMBOL_GPL(of_alias_from_compatible);

 
struct device_node *of_find_node_by_phandle(phandle handle)
{
	struct device_node *np = NULL;
	unsigned long flags;
	u32 handle_hash;

	if (!handle)
		return NULL;

	handle_hash = of_phandle_cache_hash(handle);

	raw_spin_lock_irqsave(&devtree_lock, flags);

	if (phandle_cache[handle_hash] &&
	    handle == phandle_cache[handle_hash]->phandle)
		np = phandle_cache[handle_hash];

	if (!np) {
		for_each_of_allnodes(np)
			if (np->phandle == handle &&
			    !of_node_check_flag(np, OF_DETACHED)) {
				phandle_cache[handle_hash] = np;
				break;
			}
	}

	of_node_get(np);
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	return np;
}
EXPORT_SYMBOL(of_find_node_by_phandle);

void of_print_phandle_args(const char *msg, const struct of_phandle_args *args)
{
	int i;
	printk("%s %pOF", msg, args->np);
	for (i = 0; i < args->args_count; i++) {
		const char delim = i ? ',' : ':';

		pr_cont("%c%08x", delim, args->args[i]);
	}
	pr_cont("\n");
}

int of_phandle_iterator_init(struct of_phandle_iterator *it,
		const struct device_node *np,
		const char *list_name,
		const char *cells_name,
		int cell_count)
{
	const __be32 *list;
	int size;

	memset(it, 0, sizeof(*it));

	 
	if (cell_count < 0 && !cells_name)
		return -EINVAL;

	list = of_get_property(np, list_name, &size);
	if (!list)
		return -ENOENT;

	it->cells_name = cells_name;
	it->cell_count = cell_count;
	it->parent = np;
	it->list_end = list + size / sizeof(*list);
	it->phandle_end = list;
	it->cur = list;

	return 0;
}
EXPORT_SYMBOL_GPL(of_phandle_iterator_init);

int of_phandle_iterator_next(struct of_phandle_iterator *it)
{
	uint32_t count = 0;

	if (it->node) {
		of_node_put(it->node);
		it->node = NULL;
	}

	if (!it->cur || it->phandle_end >= it->list_end)
		return -ENOENT;

	it->cur = it->phandle_end;

	 
	it->phandle = be32_to_cpup(it->cur++);

	if (it->phandle) {

		 
		it->node = of_find_node_by_phandle(it->phandle);

		if (it->cells_name) {
			if (!it->node) {
				pr_err("%pOF: could not find phandle %d\n",
				       it->parent, it->phandle);
				goto err;
			}

			if (of_property_read_u32(it->node, it->cells_name,
						 &count)) {
				 
				if (it->cell_count >= 0) {
					count = it->cell_count;
				} else {
					pr_err("%pOF: could not get %s for %pOF\n",
					       it->parent,
					       it->cells_name,
					       it->node);
					goto err;
				}
			}
		} else {
			count = it->cell_count;
		}

		 
		if (it->cur + count > it->list_end) {
			if (it->cells_name)
				pr_err("%pOF: %s = %d found %td\n",
					it->parent, it->cells_name,
					count, it->list_end - it->cur);
			else
				pr_err("%pOF: phandle %s needs %d, found %td\n",
					it->parent, of_node_full_name(it->node),
					count, it->list_end - it->cur);
			goto err;
		}
	}

	it->phandle_end = it->cur + count;
	it->cur_count = count;

	return 0;

err:
	if (it->node) {
		of_node_put(it->node);
		it->node = NULL;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(of_phandle_iterator_next);

int of_phandle_iterator_args(struct of_phandle_iterator *it,
			     uint32_t *args,
			     int size)
{
	int i, count;

	count = it->cur_count;

	if (WARN_ON(size < count))
		count = size;

	for (i = 0; i < count; i++)
		args[i] = be32_to_cpup(it->cur++);

	return count;
}

int __of_parse_phandle_with_args(const struct device_node *np,
				 const char *list_name,
				 const char *cells_name,
				 int cell_count, int index,
				 struct of_phandle_args *out_args)
{
	struct of_phandle_iterator it;
	int rc, cur_index = 0;

	if (index < 0)
		return -EINVAL;

	 
	of_for_each_phandle(&it, rc, np, list_name, cells_name, cell_count) {
		 
		rc = -ENOENT;
		if (cur_index == index) {
			if (!it.phandle)
				goto err;

			if (out_args) {
				int c;

				c = of_phandle_iterator_args(&it,
							     out_args->args,
							     MAX_PHANDLE_ARGS);
				out_args->np = it.node;
				out_args->args_count = c;
			} else {
				of_node_put(it.node);
			}

			 
			return 0;
		}

		cur_index++;
	}

	 

 err:
	of_node_put(it.node);
	return rc;
}
EXPORT_SYMBOL(__of_parse_phandle_with_args);

 
int of_parse_phandle_with_args_map(const struct device_node *np,
				   const char *list_name,
				   const char *stem_name,
				   int index, struct of_phandle_args *out_args)
{
	char *cells_name, *map_name = NULL, *mask_name = NULL;
	char *pass_name = NULL;
	struct device_node *cur, *new = NULL;
	const __be32 *map, *mask, *pass;
	static const __be32 dummy_mask[] = { [0 ... MAX_PHANDLE_ARGS] = ~0 };
	static const __be32 dummy_pass[] = { [0 ... MAX_PHANDLE_ARGS] = 0 };
	__be32 initial_match_array[MAX_PHANDLE_ARGS];
	const __be32 *match_array = initial_match_array;
	int i, ret, map_len, match;
	u32 list_size, new_size;

	if (index < 0)
		return -EINVAL;

	cells_name = kasprintf(GFP_KERNEL, "#%s-cells", stem_name);
	if (!cells_name)
		return -ENOMEM;

	ret = -ENOMEM;
	map_name = kasprintf(GFP_KERNEL, "%s-map", stem_name);
	if (!map_name)
		goto free;

	mask_name = kasprintf(GFP_KERNEL, "%s-map-mask", stem_name);
	if (!mask_name)
		goto free;

	pass_name = kasprintf(GFP_KERNEL, "%s-map-pass-thru", stem_name);
	if (!pass_name)
		goto free;

	ret = __of_parse_phandle_with_args(np, list_name, cells_name, -1, index,
					   out_args);
	if (ret)
		goto free;

	 
	cur = out_args->np;
	ret = of_property_read_u32(cur, cells_name, &list_size);
	if (ret < 0)
		goto put;

	 
	for (i = 0; i < list_size; i++)
		initial_match_array[i] = cpu_to_be32(out_args->args[i]);

	ret = -EINVAL;
	while (cur) {
		 
		map = of_get_property(cur, map_name, &map_len);
		if (!map) {
			ret = 0;
			goto free;
		}
		map_len /= sizeof(u32);

		 
		mask = of_get_property(cur, mask_name, NULL);
		if (!mask)
			mask = dummy_mask;
		 
		match = 0;
		while (map_len > (list_size + 1) && !match) {
			 
			match = 1;
			for (i = 0; i < list_size; i++, map_len--)
				match &= !((match_array[i] ^ *map++) & mask[i]);

			of_node_put(new);
			new = of_find_node_by_phandle(be32_to_cpup(map));
			map++;
			map_len--;

			 
			if (!new)
				goto put;

			if (!of_device_is_available(new))
				match = 0;

			ret = of_property_read_u32(new, cells_name, &new_size);
			if (ret)
				goto put;

			 
			if (WARN_ON(new_size > MAX_PHANDLE_ARGS))
				goto put;
			if (map_len < new_size)
				goto put;

			 
			map += new_size;
			map_len -= new_size;
		}
		if (!match)
			goto put;

		 
		pass = of_get_property(cur, pass_name, NULL);
		if (!pass)
			pass = dummy_pass;

		 
		match_array = map - new_size;
		for (i = 0; i < new_size; i++) {
			__be32 val = *(map - new_size + i);

			if (i < list_size) {
				val &= ~pass[i];
				val |= cpu_to_be32(out_args->args[i]) & pass[i];
			}

			out_args->args[i] = be32_to_cpu(val);
		}
		out_args->args_count = list_size = new_size;
		 
		out_args->np = new;
		of_node_put(cur);
		cur = new;
		new = NULL;
	}
put:
	of_node_put(cur);
	of_node_put(new);
free:
	kfree(mask_name);
	kfree(map_name);
	kfree(cells_name);
	kfree(pass_name);

	return ret;
}
EXPORT_SYMBOL(of_parse_phandle_with_args_map);

 
int of_count_phandle_with_args(const struct device_node *np, const char *list_name,
				const char *cells_name)
{
	struct of_phandle_iterator it;
	int rc, cur_index = 0;

	 
	if (!cells_name) {
		const __be32 *list;
		int size;

		list = of_get_property(np, list_name, &size);
		if (!list)
			return -ENOENT;

		return size / sizeof(*list);
	}

	rc = of_phandle_iterator_init(&it, np, list_name, cells_name, -1);
	if (rc)
		return rc;

	while ((rc = of_phandle_iterator_next(&it)) == 0)
		cur_index += 1;

	if (rc != -ENOENT)
		return rc;

	return cur_index;
}
EXPORT_SYMBOL(of_count_phandle_with_args);

static struct property *__of_remove_property_from_list(struct property **list, struct property *prop)
{
	struct property **next;

	for (next = list; *next; next = &(*next)->next) {
		if (*next == prop) {
			*next = prop->next;
			prop->next = NULL;
			return prop;
		}
	}
	return NULL;
}

 
int __of_add_property(struct device_node *np, struct property *prop)
{
	int rc = 0;
	unsigned long flags;
	struct property **next;

	raw_spin_lock_irqsave(&devtree_lock, flags);

	__of_remove_property_from_list(&np->deadprops, prop);

	prop->next = NULL;
	next = &np->properties;
	while (*next) {
		if (strcmp(prop->name, (*next)->name) == 0) {
			 
			rc = -EEXIST;
			goto out_unlock;
		}
		next = &(*next)->next;
	}
	*next = prop;

out_unlock:
	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	if (rc)
		return rc;

	__of_add_property_sysfs(np, prop);
	return 0;
}

 
int of_add_property(struct device_node *np, struct property *prop)
{
	int rc;

	mutex_lock(&of_mutex);
	rc = __of_add_property(np, prop);
	mutex_unlock(&of_mutex);

	if (!rc)
		of_property_notify(OF_RECONFIG_ADD_PROPERTY, np, prop, NULL);

	return rc;
}
EXPORT_SYMBOL_GPL(of_add_property);

int __of_remove_property(struct device_node *np, struct property *prop)
{
	unsigned long flags;
	int rc = -ENODEV;

	raw_spin_lock_irqsave(&devtree_lock, flags);

	if (__of_remove_property_from_list(&np->properties, prop)) {
		 
		prop->next = np->deadprops;
		np->deadprops = prop;
		rc = 0;
	}

	raw_spin_unlock_irqrestore(&devtree_lock, flags);
	if (rc)
		return rc;

	__of_remove_property_sysfs(np, prop);
	return 0;
}

 
int of_remove_property(struct device_node *np, struct property *prop)
{
	int rc;

	if (!prop)
		return -ENODEV;

	mutex_lock(&of_mutex);
	rc = __of_remove_property(np, prop);
	mutex_unlock(&of_mutex);

	if (!rc)
		of_property_notify(OF_RECONFIG_REMOVE_PROPERTY, np, prop, NULL);

	return rc;
}
EXPORT_SYMBOL_GPL(of_remove_property);

int __of_update_property(struct device_node *np, struct property *newprop,
		struct property **oldpropp)
{
	struct property **next, *oldprop;
	unsigned long flags;

	raw_spin_lock_irqsave(&devtree_lock, flags);

	__of_remove_property_from_list(&np->deadprops, newprop);

	for (next = &np->properties; *next; next = &(*next)->next) {
		if (of_prop_cmp((*next)->name, newprop->name) == 0)
			break;
	}
	*oldpropp = oldprop = *next;

	if (oldprop) {
		 
		newprop->next = oldprop->next;
		*next = newprop;
		oldprop->next = np->deadprops;
		np->deadprops = oldprop;
	} else {
		 
		newprop->next = NULL;
		*next = newprop;
	}

	raw_spin_unlock_irqrestore(&devtree_lock, flags);

	__of_update_property_sysfs(np, newprop, oldprop);

	return 0;
}

 
int of_update_property(struct device_node *np, struct property *newprop)
{
	struct property *oldprop;
	int rc;

	if (!newprop->name)
		return -EINVAL;

	mutex_lock(&of_mutex);
	rc = __of_update_property(np, newprop, &oldprop);
	mutex_unlock(&of_mutex);

	if (!rc)
		of_property_notify(OF_RECONFIG_UPDATE_PROPERTY, np, newprop, oldprop);

	return rc;
}

static void of_alias_add(struct alias_prop *ap, struct device_node *np,
			 int id, const char *stem, int stem_len)
{
	ap->np = np;
	ap->id = id;
	strscpy(ap->stem, stem, stem_len + 1);
	list_add_tail(&ap->link, &aliases_lookup);
	pr_debug("adding DT alias:%s: stem=%s id=%i node=%pOF\n",
		 ap->alias, ap->stem, ap->id, np);
}

 
void of_alias_scan(void * (*dt_alloc)(u64 size, u64 align))
{
	struct property *pp;

	of_aliases = of_find_node_by_path("/aliases");
	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen == NULL)
		of_chosen = of_find_node_by_path("/chosen@0");

	if (of_chosen) {
		 
		const char *name = NULL;

		if (of_property_read_string(of_chosen, "stdout-path", &name))
			of_property_read_string(of_chosen, "linux,stdout-path",
						&name);
		if (IS_ENABLED(CONFIG_PPC) && !name)
			of_property_read_string(of_aliases, "stdout", &name);
		if (name)
			of_stdout = of_find_node_opts_by_path(name, &of_stdout_options);
		if (of_stdout)
			of_stdout->fwnode.flags |= FWNODE_FLAG_BEST_EFFORT;
	}

	if (!of_aliases)
		return;

	for_each_property_of_node(of_aliases, pp) {
		const char *start = pp->name;
		const char *end = start + strlen(start);
		struct device_node *np;
		struct alias_prop *ap;
		int id, len;

		 
		if (!strcmp(pp->name, "name") ||
		    !strcmp(pp->name, "phandle") ||
		    !strcmp(pp->name, "linux,phandle"))
			continue;

		np = of_find_node_by_path(pp->value);
		if (!np)
			continue;

		 
		while (isdigit(*(end-1)) && end > start)
			end--;
		len = end - start;

		if (kstrtoint(end, 10, &id) < 0)
			continue;

		 
		ap = dt_alloc(sizeof(*ap) + len + 1, __alignof__(*ap));
		if (!ap)
			continue;
		memset(ap, 0, sizeof(*ap) + len + 1);
		ap->alias = start;
		of_alias_add(ap, np, id, start, len);
	}
}

 
int of_alias_get_id(struct device_node *np, const char *stem)
{
	struct alias_prop *app;
	int id = -ENODEV;

	mutex_lock(&of_mutex);
	list_for_each_entry(app, &aliases_lookup, link) {
		if (strcmp(app->stem, stem) != 0)
			continue;

		if (np == app->np) {
			id = app->id;
			break;
		}
	}
	mutex_unlock(&of_mutex);

	return id;
}
EXPORT_SYMBOL_GPL(of_alias_get_id);

 
int of_alias_get_highest_id(const char *stem)
{
	struct alias_prop *app;
	int id = -ENODEV;

	mutex_lock(&of_mutex);
	list_for_each_entry(app, &aliases_lookup, link) {
		if (strcmp(app->stem, stem) != 0)
			continue;

		if (app->id > id)
			id = app->id;
	}
	mutex_unlock(&of_mutex);

	return id;
}
EXPORT_SYMBOL_GPL(of_alias_get_highest_id);

 
bool of_console_check(struct device_node *dn, char *name, int index)
{
	if (!dn || dn != of_stdout || console_set_on_cmdline)
		return false;

	 
	return !add_preferred_console(name, index, (char *)of_stdout_options);
}
EXPORT_SYMBOL_GPL(of_console_check);

 
struct device_node *of_find_next_cache_node(const struct device_node *np)
{
	struct device_node *child, *cache_node;

	cache_node = of_parse_phandle(np, "l2-cache", 0);
	if (!cache_node)
		cache_node = of_parse_phandle(np, "next-level-cache", 0);

	if (cache_node)
		return cache_node;

	 
	if (IS_ENABLED(CONFIG_PPC_PMAC) && of_node_is_type(np, "cpu"))
		for_each_child_of_node(np, child)
			if (of_node_is_type(child, "cache"))
				return child;

	return NULL;
}

 
int of_find_last_cache_level(unsigned int cpu)
{
	u32 cache_level = 0;
	struct device_node *prev = NULL, *np = of_cpu_device_node_get(cpu);

	while (np) {
		of_node_put(prev);
		prev = np;
		np = of_find_next_cache_node(np);
	}

	of_property_read_u32(prev, "cache-level", &cache_level);
	of_node_put(prev);

	return cache_level;
}

 
int of_map_id(struct device_node *np, u32 id,
	       const char *map_name, const char *map_mask_name,
	       struct device_node **target, u32 *id_out)
{
	u32 map_mask, masked_id;
	int map_len;
	const __be32 *map = NULL;

	if (!np || !map_name || (!target && !id_out))
		return -EINVAL;

	map = of_get_property(np, map_name, &map_len);
	if (!map) {
		if (target)
			return -ENODEV;
		 
		*id_out = id;
		return 0;
	}

	if (!map_len || map_len % (4 * sizeof(*map))) {
		pr_err("%pOF: Error: Bad %s length: %d\n", np,
			map_name, map_len);
		return -EINVAL;
	}

	 
	map_mask = 0xffffffff;

	 
	if (map_mask_name)
		of_property_read_u32(np, map_mask_name, &map_mask);

	masked_id = map_mask & id;
	for ( ; map_len > 0; map_len -= 4 * sizeof(*map), map += 4) {
		struct device_node *phandle_node;
		u32 id_base = be32_to_cpup(map + 0);
		u32 phandle = be32_to_cpup(map + 1);
		u32 out_base = be32_to_cpup(map + 2);
		u32 id_len = be32_to_cpup(map + 3);

		if (id_base & ~map_mask) {
			pr_err("%pOF: Invalid %s translation - %s-mask (0x%x) ignores id-base (0x%x)\n",
				np, map_name, map_name,
				map_mask, id_base);
			return -EFAULT;
		}

		if (masked_id < id_base || masked_id >= id_base + id_len)
			continue;

		phandle_node = of_find_node_by_phandle(phandle);
		if (!phandle_node)
			return -ENODEV;

		if (target) {
			if (*target)
				of_node_put(phandle_node);
			else
				*target = phandle_node;

			if (*target != phandle_node)
				continue;
		}

		if (id_out)
			*id_out = masked_id - id_base + out_base;

		pr_debug("%pOF: %s, using mask %08x, id-base: %08x, out-base: %08x, length: %08x, id: %08x -> %08x\n",
			np, map_name, map_mask, id_base, out_base,
			id_len, id, masked_id - id_base + out_base);
		return 0;
	}

	pr_info("%pOF: no %s translation for id 0x%x on %pOF\n", np, map_name,
		id, target && *target ? *target : NULL);

	 
	if (id_out)
		*id_out = id;
	return 0;
}
EXPORT_SYMBOL_GPL(of_map_id);
