
 

#include <linux/acpi.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_graph.h>
#include <linux/of_irq.h>
#include <linux/property.h>
#include <linux/phy.h>

struct fwnode_handle *__dev_fwnode(struct device *dev)
{
	return IS_ENABLED(CONFIG_OF) && dev->of_node ?
		of_fwnode_handle(dev->of_node) : dev->fwnode;
}
EXPORT_SYMBOL_GPL(__dev_fwnode);

const struct fwnode_handle *__dev_fwnode_const(const struct device *dev)
{
	return IS_ENABLED(CONFIG_OF) && dev->of_node ?
		of_fwnode_handle(dev->of_node) : dev->fwnode;
}
EXPORT_SYMBOL_GPL(__dev_fwnode_const);

 
bool device_property_present(const struct device *dev, const char *propname)
{
	return fwnode_property_present(dev_fwnode(dev), propname);
}
EXPORT_SYMBOL_GPL(device_property_present);

 
bool fwnode_property_present(const struct fwnode_handle *fwnode,
			     const char *propname)
{
	bool ret;

	if (IS_ERR_OR_NULL(fwnode))
		return false;

	ret = fwnode_call_bool_op(fwnode, property_present, propname);
	if (ret)
		return ret;

	return fwnode_call_bool_op(fwnode->secondary, property_present, propname);
}
EXPORT_SYMBOL_GPL(fwnode_property_present);

 
int device_property_read_u8_array(const struct device *dev, const char *propname,
				  u8 *val, size_t nval)
{
	return fwnode_property_read_u8_array(dev_fwnode(dev), propname, val, nval);
}
EXPORT_SYMBOL_GPL(device_property_read_u8_array);

 
int device_property_read_u16_array(const struct device *dev, const char *propname,
				   u16 *val, size_t nval)
{
	return fwnode_property_read_u16_array(dev_fwnode(dev), propname, val, nval);
}
EXPORT_SYMBOL_GPL(device_property_read_u16_array);

 
int device_property_read_u32_array(const struct device *dev, const char *propname,
				   u32 *val, size_t nval)
{
	return fwnode_property_read_u32_array(dev_fwnode(dev), propname, val, nval);
}
EXPORT_SYMBOL_GPL(device_property_read_u32_array);

 
int device_property_read_u64_array(const struct device *dev, const char *propname,
				   u64 *val, size_t nval)
{
	return fwnode_property_read_u64_array(dev_fwnode(dev), propname, val, nval);
}
EXPORT_SYMBOL_GPL(device_property_read_u64_array);

 
int device_property_read_string_array(const struct device *dev, const char *propname,
				      const char **val, size_t nval)
{
	return fwnode_property_read_string_array(dev_fwnode(dev), propname, val, nval);
}
EXPORT_SYMBOL_GPL(device_property_read_string_array);

 
int device_property_read_string(const struct device *dev, const char *propname,
				const char **val)
{
	return fwnode_property_read_string(dev_fwnode(dev), propname, val);
}
EXPORT_SYMBOL_GPL(device_property_read_string);

 
int device_property_match_string(const struct device *dev, const char *propname,
				 const char *string)
{
	return fwnode_property_match_string(dev_fwnode(dev), propname, string);
}
EXPORT_SYMBOL_GPL(device_property_match_string);

static int fwnode_property_read_int_array(const struct fwnode_handle *fwnode,
					  const char *propname,
					  unsigned int elem_size, void *val,
					  size_t nval)
{
	int ret;

	if (IS_ERR_OR_NULL(fwnode))
		return -EINVAL;

	ret = fwnode_call_int_op(fwnode, property_read_int_array, propname,
				 elem_size, val, nval);
	if (ret != -EINVAL)
		return ret;

	return fwnode_call_int_op(fwnode->secondary, property_read_int_array, propname,
				  elem_size, val, nval);
}

 
int fwnode_property_read_u8_array(const struct fwnode_handle *fwnode,
				  const char *propname, u8 *val, size_t nval)
{
	return fwnode_property_read_int_array(fwnode, propname, sizeof(u8),
					      val, nval);
}
EXPORT_SYMBOL_GPL(fwnode_property_read_u8_array);

 
int fwnode_property_read_u16_array(const struct fwnode_handle *fwnode,
				   const char *propname, u16 *val, size_t nval)
{
	return fwnode_property_read_int_array(fwnode, propname, sizeof(u16),
					      val, nval);
}
EXPORT_SYMBOL_GPL(fwnode_property_read_u16_array);

 
int fwnode_property_read_u32_array(const struct fwnode_handle *fwnode,
				   const char *propname, u32 *val, size_t nval)
{
	return fwnode_property_read_int_array(fwnode, propname, sizeof(u32),
					      val, nval);
}
EXPORT_SYMBOL_GPL(fwnode_property_read_u32_array);

 
int fwnode_property_read_u64_array(const struct fwnode_handle *fwnode,
				   const char *propname, u64 *val, size_t nval)
{
	return fwnode_property_read_int_array(fwnode, propname, sizeof(u64),
					      val, nval);
}
EXPORT_SYMBOL_GPL(fwnode_property_read_u64_array);

 
int fwnode_property_read_string_array(const struct fwnode_handle *fwnode,
				      const char *propname, const char **val,
				      size_t nval)
{
	int ret;

	if (IS_ERR_OR_NULL(fwnode))
		return -EINVAL;

	ret = fwnode_call_int_op(fwnode, property_read_string_array, propname,
				 val, nval);
	if (ret != -EINVAL)
		return ret;

	return fwnode_call_int_op(fwnode->secondary, property_read_string_array, propname,
				  val, nval);
}
EXPORT_SYMBOL_GPL(fwnode_property_read_string_array);

 
int fwnode_property_read_string(const struct fwnode_handle *fwnode,
				const char *propname, const char **val)
{
	int ret = fwnode_property_read_string_array(fwnode, propname, val, 1);

	return ret < 0 ? ret : 0;
}
EXPORT_SYMBOL_GPL(fwnode_property_read_string);

 
int fwnode_property_match_string(const struct fwnode_handle *fwnode,
	const char *propname, const char *string)
{
	const char **values;
	int nval, ret;

	nval = fwnode_property_read_string_array(fwnode, propname, NULL, 0);
	if (nval < 0)
		return nval;

	if (nval == 0)
		return -ENODATA;

	values = kcalloc(nval, sizeof(*values), GFP_KERNEL);
	if (!values)
		return -ENOMEM;

	ret = fwnode_property_read_string_array(fwnode, propname, values, nval);
	if (ret < 0)
		goto out_free;

	ret = match_string(values, nval, string);
	if (ret < 0)
		ret = -ENODATA;

out_free:
	kfree(values);
	return ret;
}
EXPORT_SYMBOL_GPL(fwnode_property_match_string);

 
int fwnode_property_get_reference_args(const struct fwnode_handle *fwnode,
				       const char *prop, const char *nargs_prop,
				       unsigned int nargs, unsigned int index,
				       struct fwnode_reference_args *args)
{
	int ret;

	if (IS_ERR_OR_NULL(fwnode))
		return -ENOENT;

	ret = fwnode_call_int_op(fwnode, get_reference_args, prop, nargs_prop,
				 nargs, index, args);
	if (ret == 0)
		return ret;

	if (IS_ERR_OR_NULL(fwnode->secondary))
		return ret;

	return fwnode_call_int_op(fwnode->secondary, get_reference_args, prop, nargs_prop,
				  nargs, index, args);
}
EXPORT_SYMBOL_GPL(fwnode_property_get_reference_args);

 
struct fwnode_handle *fwnode_find_reference(const struct fwnode_handle *fwnode,
					    const char *name,
					    unsigned int index)
{
	struct fwnode_reference_args args;
	int ret;

	ret = fwnode_property_get_reference_args(fwnode, name, NULL, 0, index,
						 &args);
	return ret ? ERR_PTR(ret) : args.fwnode;
}
EXPORT_SYMBOL_GPL(fwnode_find_reference);

 
const char *fwnode_get_name(const struct fwnode_handle *fwnode)
{
	return fwnode_call_ptr_op(fwnode, get_name);
}
EXPORT_SYMBOL_GPL(fwnode_get_name);

 
const char *fwnode_get_name_prefix(const struct fwnode_handle *fwnode)
{
	return fwnode_call_ptr_op(fwnode, get_name_prefix);
}

 
struct fwnode_handle *fwnode_get_parent(const struct fwnode_handle *fwnode)
{
	return fwnode_call_ptr_op(fwnode, get_parent);
}
EXPORT_SYMBOL_GPL(fwnode_get_parent);

 
struct fwnode_handle *fwnode_get_next_parent(struct fwnode_handle *fwnode)
{
	struct fwnode_handle *parent = fwnode_get_parent(fwnode);

	fwnode_handle_put(fwnode);

	return parent;
}
EXPORT_SYMBOL_GPL(fwnode_get_next_parent);

 
struct device *fwnode_get_next_parent_dev(const struct fwnode_handle *fwnode)
{
	struct fwnode_handle *parent;
	struct device *dev;

	fwnode_for_each_parent_node(fwnode, parent) {
		dev = get_dev_from_fwnode(parent);
		if (dev) {
			fwnode_handle_put(parent);
			return dev;
		}
	}
	return NULL;
}

 
unsigned int fwnode_count_parents(const struct fwnode_handle *fwnode)
{
	struct fwnode_handle *parent;
	unsigned int count = 0;

	fwnode_for_each_parent_node(fwnode, parent)
		count++;

	return count;
}
EXPORT_SYMBOL_GPL(fwnode_count_parents);

 
struct fwnode_handle *fwnode_get_nth_parent(struct fwnode_handle *fwnode,
					    unsigned int depth)
{
	struct fwnode_handle *parent;

	if (depth == 0)
		return fwnode_handle_get(fwnode);

	fwnode_for_each_parent_node(fwnode, parent) {
		if (--depth == 0)
			return parent;
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(fwnode_get_nth_parent);

 
bool fwnode_is_ancestor_of(const struct fwnode_handle *ancestor, const struct fwnode_handle *child)
{
	struct fwnode_handle *parent;

	if (IS_ERR_OR_NULL(ancestor))
		return false;

	if (child == ancestor)
		return true;

	fwnode_for_each_parent_node(child, parent) {
		if (parent == ancestor) {
			fwnode_handle_put(parent);
			return true;
		}
	}
	return false;
}

 
struct fwnode_handle *
fwnode_get_next_child_node(const struct fwnode_handle *fwnode,
			   struct fwnode_handle *child)
{
	return fwnode_call_ptr_op(fwnode, get_next_child_node, child);
}
EXPORT_SYMBOL_GPL(fwnode_get_next_child_node);

 
struct fwnode_handle *
fwnode_get_next_available_child_node(const struct fwnode_handle *fwnode,
				     struct fwnode_handle *child)
{
	struct fwnode_handle *next_child = child;

	if (IS_ERR_OR_NULL(fwnode))
		return NULL;

	do {
		next_child = fwnode_get_next_child_node(fwnode, next_child);
		if (!next_child)
			return NULL;
	} while (!fwnode_device_is_available(next_child));

	return next_child;
}
EXPORT_SYMBOL_GPL(fwnode_get_next_available_child_node);

 
struct fwnode_handle *device_get_next_child_node(const struct device *dev,
						 struct fwnode_handle *child)
{
	const struct fwnode_handle *fwnode = dev_fwnode(dev);
	struct fwnode_handle *next;

	if (IS_ERR_OR_NULL(fwnode))
		return NULL;

	 
	next = fwnode_get_next_child_node(fwnode, child);
	if (next)
		return next;

	 
	return fwnode_get_next_child_node(fwnode->secondary, child);
}
EXPORT_SYMBOL_GPL(device_get_next_child_node);

 
struct fwnode_handle *
fwnode_get_named_child_node(const struct fwnode_handle *fwnode,
			    const char *childname)
{
	return fwnode_call_ptr_op(fwnode, get_named_child_node, childname);
}
EXPORT_SYMBOL_GPL(fwnode_get_named_child_node);

 
struct fwnode_handle *device_get_named_child_node(const struct device *dev,
						  const char *childname)
{
	return fwnode_get_named_child_node(dev_fwnode(dev), childname);
}
EXPORT_SYMBOL_GPL(device_get_named_child_node);

 
struct fwnode_handle *fwnode_handle_get(struct fwnode_handle *fwnode)
{
	if (!fwnode_has_op(fwnode, get))
		return fwnode;

	return fwnode_call_ptr_op(fwnode, get);
}
EXPORT_SYMBOL_GPL(fwnode_handle_get);

 
void fwnode_handle_put(struct fwnode_handle *fwnode)
{
	fwnode_call_void_op(fwnode, put);
}
EXPORT_SYMBOL_GPL(fwnode_handle_put);

 
bool fwnode_device_is_available(const struct fwnode_handle *fwnode)
{
	if (IS_ERR_OR_NULL(fwnode))
		return false;

	if (!fwnode_has_op(fwnode, device_is_available))
		return true;

	return fwnode_call_bool_op(fwnode, device_is_available);
}
EXPORT_SYMBOL_GPL(fwnode_device_is_available);

 
unsigned int device_get_child_node_count(const struct device *dev)
{
	struct fwnode_handle *child;
	unsigned int count = 0;

	device_for_each_child_node(dev, child)
		count++;

	return count;
}
EXPORT_SYMBOL_GPL(device_get_child_node_count);

bool device_dma_supported(const struct device *dev)
{
	return fwnode_call_bool_op(dev_fwnode(dev), device_dma_supported);
}
EXPORT_SYMBOL_GPL(device_dma_supported);

enum dev_dma_attr device_get_dma_attr(const struct device *dev)
{
	if (!fwnode_has_op(dev_fwnode(dev), device_get_dma_attr))
		return DEV_DMA_NOT_SUPPORTED;

	return fwnode_call_int_op(dev_fwnode(dev), device_get_dma_attr);
}
EXPORT_SYMBOL_GPL(device_get_dma_attr);

 
int fwnode_get_phy_mode(const struct fwnode_handle *fwnode)
{
	const char *pm;
	int err, i;

	err = fwnode_property_read_string(fwnode, "phy-mode", &pm);
	if (err < 0)
		err = fwnode_property_read_string(fwnode,
						  "phy-connection-type", &pm);
	if (err < 0)
		return err;

	for (i = 0; i < PHY_INTERFACE_MODE_MAX; i++)
		if (!strcasecmp(pm, phy_modes(i)))
			return i;

	return -ENODEV;
}
EXPORT_SYMBOL_GPL(fwnode_get_phy_mode);

 
int device_get_phy_mode(struct device *dev)
{
	return fwnode_get_phy_mode(dev_fwnode(dev));
}
EXPORT_SYMBOL_GPL(device_get_phy_mode);

 
void __iomem *fwnode_iomap(struct fwnode_handle *fwnode, int index)
{
	return fwnode_call_ptr_op(fwnode, iomap, index);
}
EXPORT_SYMBOL(fwnode_iomap);

 
int fwnode_irq_get(const struct fwnode_handle *fwnode, unsigned int index)
{
	int ret;

	ret = fwnode_call_int_op(fwnode, irq_get, index);
	 
	if (ret == 0)
		return -EINVAL;

	return ret;
}
EXPORT_SYMBOL(fwnode_irq_get);

 
int fwnode_irq_get_byname(const struct fwnode_handle *fwnode, const char *name)
{
	int index;

	if (!name)
		return -EINVAL;

	index = fwnode_property_match_string(fwnode, "interrupt-names",  name);
	if (index < 0)
		return index;

	return fwnode_irq_get(fwnode, index);
}
EXPORT_SYMBOL(fwnode_irq_get_byname);

 
struct fwnode_handle *
fwnode_graph_get_next_endpoint(const struct fwnode_handle *fwnode,
			       struct fwnode_handle *prev)
{
	struct fwnode_handle *ep, *port_parent = NULL;
	const struct fwnode_handle *parent;

	 
	if (prev) {
		port_parent = fwnode_graph_get_port_parent(prev);
		parent = port_parent;
	} else {
		parent = fwnode;
	}
	if (IS_ERR_OR_NULL(parent))
		return NULL;

	ep = fwnode_call_ptr_op(parent, graph_get_next_endpoint, prev);
	if (ep)
		goto out_put_port_parent;

	ep = fwnode_graph_get_next_endpoint(parent->secondary, NULL);

out_put_port_parent:
	fwnode_handle_put(port_parent);
	return ep;
}
EXPORT_SYMBOL_GPL(fwnode_graph_get_next_endpoint);

 
struct fwnode_handle *
fwnode_graph_get_port_parent(const struct fwnode_handle *endpoint)
{
	struct fwnode_handle *port, *parent;

	port = fwnode_get_parent(endpoint);
	parent = fwnode_call_ptr_op(port, graph_get_port_parent);

	fwnode_handle_put(port);

	return parent;
}
EXPORT_SYMBOL_GPL(fwnode_graph_get_port_parent);

 
struct fwnode_handle *
fwnode_graph_get_remote_port_parent(const struct fwnode_handle *fwnode)
{
	struct fwnode_handle *endpoint, *parent;

	endpoint = fwnode_graph_get_remote_endpoint(fwnode);
	parent = fwnode_graph_get_port_parent(endpoint);

	fwnode_handle_put(endpoint);

	return parent;
}
EXPORT_SYMBOL_GPL(fwnode_graph_get_remote_port_parent);

 
struct fwnode_handle *
fwnode_graph_get_remote_port(const struct fwnode_handle *fwnode)
{
	return fwnode_get_next_parent(fwnode_graph_get_remote_endpoint(fwnode));
}
EXPORT_SYMBOL_GPL(fwnode_graph_get_remote_port);

 
struct fwnode_handle *
fwnode_graph_get_remote_endpoint(const struct fwnode_handle *fwnode)
{
	return fwnode_call_ptr_op(fwnode, graph_get_remote_endpoint);
}
EXPORT_SYMBOL_GPL(fwnode_graph_get_remote_endpoint);

static bool fwnode_graph_remote_available(struct fwnode_handle *ep)
{
	struct fwnode_handle *dev_node;
	bool available;

	dev_node = fwnode_graph_get_remote_port_parent(ep);
	available = fwnode_device_is_available(dev_node);
	fwnode_handle_put(dev_node);

	return available;
}

 
struct fwnode_handle *
fwnode_graph_get_endpoint_by_id(const struct fwnode_handle *fwnode,
				u32 port, u32 endpoint, unsigned long flags)
{
	struct fwnode_handle *ep, *best_ep = NULL;
	unsigned int best_ep_id = 0;
	bool endpoint_next = flags & FWNODE_GRAPH_ENDPOINT_NEXT;
	bool enabled_only = !(flags & FWNODE_GRAPH_DEVICE_DISABLED);

	fwnode_graph_for_each_endpoint(fwnode, ep) {
		struct fwnode_endpoint fwnode_ep = { 0 };
		int ret;

		if (enabled_only && !fwnode_graph_remote_available(ep))
			continue;

		ret = fwnode_graph_parse_endpoint(ep, &fwnode_ep);
		if (ret < 0)
			continue;

		if (fwnode_ep.port != port)
			continue;

		if (fwnode_ep.id == endpoint)
			return ep;

		if (!endpoint_next)
			continue;

		 
		if (fwnode_ep.id < endpoint ||
		    (best_ep && best_ep_id < fwnode_ep.id))
			continue;

		fwnode_handle_put(best_ep);
		best_ep = fwnode_handle_get(ep);
		best_ep_id = fwnode_ep.id;
	}

	return best_ep;
}
EXPORT_SYMBOL_GPL(fwnode_graph_get_endpoint_by_id);

 
unsigned int fwnode_graph_get_endpoint_count(const struct fwnode_handle *fwnode,
					     unsigned long flags)
{
	struct fwnode_handle *ep;
	unsigned int count = 0;

	fwnode_graph_for_each_endpoint(fwnode, ep) {
		if (flags & FWNODE_GRAPH_DEVICE_DISABLED ||
		    fwnode_graph_remote_available(ep))
			count++;
	}

	return count;
}
EXPORT_SYMBOL_GPL(fwnode_graph_get_endpoint_count);

 
int fwnode_graph_parse_endpoint(const struct fwnode_handle *fwnode,
				struct fwnode_endpoint *endpoint)
{
	memset(endpoint, 0, sizeof(*endpoint));

	return fwnode_call_int_op(fwnode, graph_parse_endpoint, endpoint);
}
EXPORT_SYMBOL(fwnode_graph_parse_endpoint);

const void *device_get_match_data(const struct device *dev)
{
	return fwnode_call_ptr_op(dev_fwnode(dev), device_get_match_data, dev);
}
EXPORT_SYMBOL_GPL(device_get_match_data);

static unsigned int fwnode_graph_devcon_matches(const struct fwnode_handle *fwnode,
						const char *con_id, void *data,
						devcon_match_fn_t match,
						void **matches,
						unsigned int matches_len)
{
	struct fwnode_handle *node;
	struct fwnode_handle *ep;
	unsigned int count = 0;
	void *ret;

	fwnode_graph_for_each_endpoint(fwnode, ep) {
		if (matches && count >= matches_len) {
			fwnode_handle_put(ep);
			break;
		}

		node = fwnode_graph_get_remote_port_parent(ep);
		if (!fwnode_device_is_available(node)) {
			fwnode_handle_put(node);
			continue;
		}

		ret = match(node, con_id, data);
		fwnode_handle_put(node);
		if (ret) {
			if (matches)
				matches[count] = ret;
			count++;
		}
	}
	return count;
}

static unsigned int fwnode_devcon_matches(const struct fwnode_handle *fwnode,
					  const char *con_id, void *data,
					  devcon_match_fn_t match,
					  void **matches,
					  unsigned int matches_len)
{
	struct fwnode_handle *node;
	unsigned int count = 0;
	unsigned int i;
	void *ret;

	for (i = 0; ; i++) {
		if (matches && count >= matches_len)
			break;

		node = fwnode_find_reference(fwnode, con_id, i);
		if (IS_ERR(node))
			break;

		ret = match(node, NULL, data);
		fwnode_handle_put(node);
		if (ret) {
			if (matches)
				matches[count] = ret;
			count++;
		}
	}

	return count;
}

 
void *fwnode_connection_find_match(const struct fwnode_handle *fwnode,
				   const char *con_id, void *data,
				   devcon_match_fn_t match)
{
	unsigned int count;
	void *ret;

	if (!fwnode || !match)
		return NULL;

	count = fwnode_graph_devcon_matches(fwnode, con_id, data, match, &ret, 1);
	if (count)
		return ret;

	count = fwnode_devcon_matches(fwnode, con_id, data, match, &ret, 1);
	return count ? ret : NULL;
}
EXPORT_SYMBOL_GPL(fwnode_connection_find_match);

 
int fwnode_connection_find_matches(const struct fwnode_handle *fwnode,
				   const char *con_id, void *data,
				   devcon_match_fn_t match,
				   void **matches, unsigned int matches_len)
{
	unsigned int count_graph;
	unsigned int count_ref;

	if (!fwnode || !match)
		return -EINVAL;

	count_graph = fwnode_graph_devcon_matches(fwnode, con_id, data, match,
						  matches, matches_len);

	if (matches) {
		matches += count_graph;
		matches_len -= count_graph;
	}

	count_ref = fwnode_devcon_matches(fwnode, con_id, data, match,
					  matches, matches_len);

	return count_graph + count_ref;
}
EXPORT_SYMBOL_GPL(fwnode_connection_find_matches);
