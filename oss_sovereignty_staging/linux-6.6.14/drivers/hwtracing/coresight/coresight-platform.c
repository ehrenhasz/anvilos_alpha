
 

#include <linux/acpi.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/amba/bus.h>
#include <linux/coresight.h>
#include <linux/cpumask.h>
#include <asm/smp_plat.h>

#include "coresight-priv.h"

 
struct coresight_connection *
coresight_add_out_conn(struct device *dev,
		       struct coresight_platform_data *pdata,
		       const struct coresight_connection *new_conn)
{
	int i;
	struct coresight_connection *conn;

	 
	for (i = 0; i < pdata->nr_outconns; ++i) {
		conn = pdata->out_conns[i];
		 
		if (conn->src_port != -1 &&
		    conn->src_port == new_conn->src_port) {
			dev_warn(dev, "Duplicate output port %d\n",
				 conn->src_port);
			return ERR_PTR(-EINVAL);
		}
	}

	pdata->nr_outconns++;
	pdata->out_conns =
		devm_krealloc_array(dev, pdata->out_conns, pdata->nr_outconns,
				    sizeof(*pdata->out_conns), GFP_KERNEL);
	if (!pdata->out_conns)
		return ERR_PTR(-ENOMEM);

	conn = devm_kmalloc(dev, sizeof(struct coresight_connection),
			    GFP_KERNEL);
	if (!conn)
		return ERR_PTR(-ENOMEM);

	 
	*conn = *new_conn;
	pdata->out_conns[pdata->nr_outconns - 1] = conn;
	return conn;
}
EXPORT_SYMBOL_GPL(coresight_add_out_conn);

 
int coresight_add_in_conn(struct coresight_connection *out_conn)
{
	int i;
	struct device *dev = out_conn->dest_dev->dev.parent;
	struct coresight_platform_data *pdata = out_conn->dest_dev->pdata;

	for (i = 0; i < pdata->nr_inconns; ++i)
		if (!pdata->in_conns[i]) {
			pdata->in_conns[i] = out_conn;
			return 0;
		}

	pdata->nr_inconns++;
	pdata->in_conns =
		devm_krealloc_array(dev, pdata->in_conns, pdata->nr_inconns,
				    sizeof(*pdata->in_conns), GFP_KERNEL);
	if (!pdata->in_conns)
		return -ENOMEM;
	pdata->in_conns[pdata->nr_inconns - 1] = out_conn;
	return 0;
}
EXPORT_SYMBOL_GPL(coresight_add_in_conn);

static struct device *
coresight_find_device_by_fwnode(struct fwnode_handle *fwnode)
{
	struct device *dev = NULL;

	 
	dev = bus_find_device_by_fwnode(&platform_bus_type, fwnode);
	if (dev)
		return dev;

	 
	return bus_find_device_by_fwnode(&amba_bustype, fwnode);
}

 
struct coresight_device *
coresight_find_csdev_by_fwnode(struct fwnode_handle *r_fwnode)
{
	struct device *dev;
	struct coresight_device *csdev = NULL;

	dev = bus_find_device_by_fwnode(&coresight_bustype, r_fwnode);
	if (dev) {
		csdev = to_coresight_device(dev);
		put_device(dev);
	}
	return csdev;
}
EXPORT_SYMBOL_GPL(coresight_find_csdev_by_fwnode);

#ifdef CONFIG_OF
static inline bool of_coresight_legacy_ep_is_input(struct device_node *ep)
{
	return of_property_read_bool(ep, "slave-mode");
}

static struct device_node *of_coresight_get_port_parent(struct device_node *ep)
{
	struct device_node *parent = of_graph_get_port_parent(ep);

	 
	if (of_node_name_eq(parent, "in-ports") ||
	    of_node_name_eq(parent, "out-ports"))
		parent = of_get_next_parent(parent);

	return parent;
}

static inline struct device_node *
of_coresight_get_output_ports_node(const struct device_node *node)
{
	return of_get_child_by_name(node, "out-ports");
}

static int of_coresight_get_cpu(struct device *dev)
{
	int cpu;
	struct device_node *dn;

	if (!dev->of_node)
		return -ENODEV;

	dn = of_parse_phandle(dev->of_node, "cpu", 0);
	if (!dn)
		return -ENODEV;

	cpu = of_cpu_node_to_id(dn);
	of_node_put(dn);

	return cpu;
}

 
static int of_coresight_parse_endpoint(struct device *dev,
				       struct device_node *ep,
				       struct coresight_platform_data *pdata)
{
	int ret = 0;
	struct of_endpoint endpoint, rendpoint;
	struct device_node *rparent = NULL;
	struct device_node *rep = NULL;
	struct device *rdev = NULL;
	struct fwnode_handle *rdev_fwnode;
	struct coresight_connection conn = {};
	struct coresight_connection *new_conn;

	do {
		 
		if (of_graph_parse_endpoint(ep, &endpoint))
			break;
		 
		rep = of_graph_get_remote_endpoint(ep);
		if (!rep)
			break;
		rparent = of_coresight_get_port_parent(rep);
		if (!rparent)
			break;
		if (of_graph_parse_endpoint(rep, &rendpoint))
			break;

		rdev_fwnode = of_fwnode_handle(rparent);
		 
		rdev = coresight_find_device_by_fwnode(rdev_fwnode);
		if (!rdev) {
			ret = -EPROBE_DEFER;
			break;
		}

		conn.src_port = endpoint.port;
		 
		conn.dest_fwnode = fwnode_handle_get(rdev_fwnode);
		conn.dest_port = rendpoint.port;

		new_conn = coresight_add_out_conn(dev, pdata, &conn);
		if (IS_ERR_VALUE(new_conn)) {
			fwnode_handle_put(conn.dest_fwnode);
			return PTR_ERR(new_conn);
		}
		 
	} while (0);

	of_node_put(rparent);
	of_node_put(rep);
	put_device(rdev);

	return ret;
}

static int of_get_coresight_platform_data(struct device *dev,
					  struct coresight_platform_data *pdata)
{
	int ret = 0;
	struct device_node *ep = NULL;
	const struct device_node *parent = NULL;
	bool legacy_binding = false;
	struct device_node *node = dev->of_node;

	parent = of_coresight_get_output_ports_node(node);
	 
	if (!parent) {
		 
		if (!of_graph_is_present(node))
			return 0;
		legacy_binding = true;
		parent = node;
		dev_warn_once(dev, "Uses obsolete Coresight DT bindings\n");
	}

	 
	while ((ep = of_graph_get_next_endpoint(parent, ep))) {
		 
		if (legacy_binding && of_coresight_legacy_ep_is_input(ep))
			continue;

		ret = of_coresight_parse_endpoint(dev, ep, pdata);
		if (ret)
			return ret;
	}

	return 0;
}
#else
static inline int
of_get_coresight_platform_data(struct device *dev,
			       struct coresight_platform_data *pdata)
{
	return -ENOENT;
}

static inline int of_coresight_get_cpu(struct device *dev)
{
	return -ENODEV;
}
#endif

#ifdef CONFIG_ACPI

#include <acpi/actypes.h>
#include <acpi/processor.h>

 
static const guid_t acpi_graph_uuid = GUID_INIT(0xab02a46b, 0x74c7, 0x45a2,
						0xbd, 0x68, 0xf7, 0xd3,
						0x44, 0xef, 0x21, 0x53);
 
static const guid_t coresight_graph_uuid = GUID_INIT(0x3ecbc8b6, 0x1d0e, 0x4fb3,
						     0x81, 0x07, 0xe6, 0x27,
						     0xf8, 0x05, 0xc6, 0xcd);
#define ACPI_CORESIGHT_LINK_SLAVE	0
#define ACPI_CORESIGHT_LINK_MASTER	1

static inline bool is_acpi_guid(const union acpi_object *obj)
{
	return (obj->type == ACPI_TYPE_BUFFER) && (obj->buffer.length == 16);
}

 
static inline bool acpi_guid_matches(const union acpi_object *obj,
				   const guid_t *guid)
{
	return is_acpi_guid(obj) &&
	       guid_equal((guid_t *)obj->buffer.pointer, guid);
}

static inline bool is_acpi_dsd_graph_guid(const union acpi_object *obj)
{
	return acpi_guid_matches(obj, &acpi_graph_uuid);
}

static inline bool is_acpi_coresight_graph_guid(const union acpi_object *obj)
{
	return acpi_guid_matches(obj, &coresight_graph_uuid);
}

static inline bool is_acpi_coresight_graph(const union acpi_object *obj)
{
	const union acpi_object *graphid, *guid, *links;

	if (obj->type != ACPI_TYPE_PACKAGE ||
	    obj->package.count < 3)
		return false;

	graphid = &obj->package.elements[0];
	guid = &obj->package.elements[1];
	links = &obj->package.elements[2];

	if (graphid->type != ACPI_TYPE_INTEGER ||
	    links->type != ACPI_TYPE_INTEGER)
		return false;

	return is_acpi_coresight_graph_guid(guid);
}

 
static inline bool acpi_validate_dsd_graph(const union acpi_object *graph)
{
	int i, n;
	const union acpi_object *rev, *nr_graphs;

	 
	if (graph->package.count < 2)
		return false;

	rev = &graph->package.elements[0];
	nr_graphs = &graph->package.elements[1];

	if (rev->type != ACPI_TYPE_INTEGER ||
	    nr_graphs->type != ACPI_TYPE_INTEGER)
		return false;

	 
	if (rev->integer.value != 0)
		return false;

	n = nr_graphs->integer.value;
	 
	if (n != 1)
		return false;

	 
	if (graph->package.count != (n + 2))
		return false;

	 
	for (i = 2; i < n + 2; i++) {
		const union acpi_object *obj = &graph->package.elements[i];

		if (obj->type != ACPI_TYPE_PACKAGE ||
		    obj->package.count < 3)
			return false;
	}

	return true;
}

 
static const union acpi_object *
acpi_get_dsd_graph(struct acpi_device *adev, struct acpi_buffer *buf)
{
	int i;
	acpi_status status;
	const union acpi_object *dsd;

	status = acpi_evaluate_object_typed(adev->handle, "_DSD", NULL,
					    buf, ACPI_TYPE_PACKAGE);
	if (ACPI_FAILURE(status))
		return NULL;

	dsd = buf->pointer;

	 
	for (i = 0; i + 1 < dsd->package.count; i += 2) {
		const union acpi_object *guid, *package;

		guid = &dsd->package.elements[i];
		package = &dsd->package.elements[i + 1];

		 
		if (!is_acpi_guid(guid) || package->type != ACPI_TYPE_PACKAGE)
			break;
		 
		if (!is_acpi_dsd_graph_guid(guid))
			continue;
		if (acpi_validate_dsd_graph(package))
			return package;
		 
		dev_warn(&adev->dev, "Invalid Graph _DSD property\n");
	}

	return NULL;
}

static inline bool
acpi_validate_coresight_graph(const union acpi_object *cs_graph)
{
	int nlinks;

	nlinks = cs_graph->package.elements[2].integer.value;
	 
	if (cs_graph->package.count != (nlinks + 3))
		return false;
	 
	return true;
}

 
static const union acpi_object *
acpi_get_coresight_graph(struct acpi_device *adev, struct acpi_buffer *buf)
{
	const union acpi_object *graph_list, *graph;
	int i, nr_graphs;

	graph_list = acpi_get_dsd_graph(adev, buf);
	if (!graph_list)
		return graph_list;

	nr_graphs = graph_list->package.elements[1].integer.value;

	for (i = 2; i < nr_graphs + 2; i++) {
		graph = &graph_list->package.elements[i];
		if (!is_acpi_coresight_graph(graph))
			continue;
		if (acpi_validate_coresight_graph(graph))
			return graph;
		 
		break;
	}

	return NULL;
}

 
static int acpi_coresight_parse_link(struct acpi_device *adev,
				     const union acpi_object *link,
				     struct coresight_connection *conn)
{
	int dir;
	const union acpi_object *fields;
	struct acpi_device *r_adev;
	struct device *rdev;

	if (link->type != ACPI_TYPE_PACKAGE ||
	    link->package.count != 4)
		return -EINVAL;

	fields = link->package.elements;

	if (fields[0].type != ACPI_TYPE_INTEGER ||
	    fields[1].type != ACPI_TYPE_INTEGER ||
	    fields[2].type != ACPI_TYPE_LOCAL_REFERENCE ||
	    fields[3].type != ACPI_TYPE_INTEGER)
		return -EINVAL;

	r_adev = acpi_fetch_acpi_dev(fields[2].reference.handle);
	if (!r_adev)
		return -ENODEV;

	dir = fields[3].integer.value;
	if (dir == ACPI_CORESIGHT_LINK_MASTER) {
		conn->src_port = fields[0].integer.value;
		conn->dest_port = fields[1].integer.value;
		rdev = coresight_find_device_by_fwnode(&r_adev->fwnode);
		if (!rdev)
			return -EPROBE_DEFER;
		 
		conn->dest_fwnode = fwnode_handle_get(&r_adev->fwnode);
	} else if (dir == ACPI_CORESIGHT_LINK_SLAVE) {
		 
		conn->dest_port = fields[0].integer.value;
	} else {
		 
		return -EINVAL;
	}

	return dir;
}

 
static int acpi_coresight_parse_graph(struct device *dev,
				      struct acpi_device *adev,
				      struct coresight_platform_data *pdata)
{
	int ret = 0;
	int i, nlinks;
	const union acpi_object *graph;
	struct coresight_connection conn, zero_conn = {};
	struct coresight_connection *new_conn;
	struct acpi_buffer buf = { ACPI_ALLOCATE_BUFFER, NULL };

	graph = acpi_get_coresight_graph(adev, &buf);
	 
	if (!graph)
		goto free;

	nlinks = graph->package.elements[2].integer.value;
	if (!nlinks)
		goto free;

	for (i = 0; i < nlinks; i++) {
		const union acpi_object *link = &graph->package.elements[3 + i];
		int dir;

		conn = zero_conn;
		dir = acpi_coresight_parse_link(adev, link, &conn);
		if (dir < 0) {
			ret = dir;
			goto free;
		}

		if (dir == ACPI_CORESIGHT_LINK_MASTER) {
			new_conn = coresight_add_out_conn(dev, pdata, &conn);
			if (IS_ERR(new_conn)) {
				ret = PTR_ERR(new_conn);
				goto free;
			}
		}
	}

free:
	 
	ACPI_FREE(buf.pointer);
	return ret;
}

 
static int
acpi_handle_to_logical_cpuid(acpi_handle handle)
{
	int i;
	struct acpi_processor *pr;

	for_each_possible_cpu(i) {
		pr = per_cpu(processors, i);
		if (pr && pr->handle == handle)
			break;
	}

	return i;
}

 
static int acpi_coresight_get_cpu(struct device *dev)
{
	int cpu;
	acpi_handle cpu_handle;
	acpi_status status;
	struct acpi_device *adev = ACPI_COMPANION(dev);

	if (!adev)
		return -ENODEV;
	status = acpi_get_parent(adev->handle, &cpu_handle);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	cpu = acpi_handle_to_logical_cpuid(cpu_handle);
	if (cpu >= nr_cpu_ids)
		return -ENODEV;
	return cpu;
}

static int
acpi_get_coresight_platform_data(struct device *dev,
				 struct coresight_platform_data *pdata)
{
	struct acpi_device *adev;

	adev = ACPI_COMPANION(dev);
	if (!adev)
		return -EINVAL;

	return acpi_coresight_parse_graph(dev, adev, pdata);
}

#else

static inline int
acpi_get_coresight_platform_data(struct device *dev,
				 struct coresight_platform_data *pdata)
{
	return -ENOENT;
}

static inline int acpi_coresight_get_cpu(struct device *dev)
{
	return -ENODEV;
}
#endif

int coresight_get_cpu(struct device *dev)
{
	if (is_of_node(dev->fwnode))
		return of_coresight_get_cpu(dev);
	else if (is_acpi_device_node(dev->fwnode))
		return acpi_coresight_get_cpu(dev);
	return 0;
}
EXPORT_SYMBOL_GPL(coresight_get_cpu);

struct coresight_platform_data *
coresight_get_platform_data(struct device *dev)
{
	int ret = -ENOENT;
	struct coresight_platform_data *pdata = NULL;
	struct fwnode_handle *fwnode = dev_fwnode(dev);

	if (IS_ERR_OR_NULL(fwnode))
		goto error;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto error;
	}

	if (is_of_node(fwnode))
		ret = of_get_coresight_platform_data(dev, pdata);
	else if (is_acpi_device_node(fwnode))
		ret = acpi_get_coresight_platform_data(dev, pdata);

	if (!ret)
		return pdata;
error:
	if (!IS_ERR_OR_NULL(pdata))
		 
		coresight_release_platform_data(NULL, dev, pdata);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(coresight_get_platform_data);
