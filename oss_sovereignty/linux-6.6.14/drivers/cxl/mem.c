
 
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/pci.h>

#include "cxlmem.h"
#include "cxlpci.h"

 

static void enable_suspend(void *data)
{
	cxl_mem_active_dec();
}

static void remove_debugfs(void *dentry)
{
	debugfs_remove_recursive(dentry);
}

static int cxl_mem_dpa_show(struct seq_file *file, void *data)
{
	struct device *dev = file->private;
	struct cxl_memdev *cxlmd = to_cxl_memdev(dev);

	cxl_dpa_debug(file, cxlmd->cxlds);

	return 0;
}

static int devm_cxl_add_endpoint(struct device *host, struct cxl_memdev *cxlmd,
				 struct cxl_dport *parent_dport)
{
	struct cxl_port *parent_port = parent_dport->port;
	struct cxl_dev_state *cxlds = cxlmd->cxlds;
	struct cxl_port *endpoint, *iter, *down;
	int rc;

	 
	for (iter = parent_port, down = NULL; !is_cxl_root(iter);
	     down = iter, iter = to_cxl_port(iter->dev.parent)) {
		struct cxl_ep *ep;

		ep = cxl_ep_load(iter, cxlmd);
		ep->next = down;
	}

	endpoint = devm_cxl_add_port(host, &cxlmd->dev,
				     cxlds->component_reg_phys,
				     parent_dport);
	if (IS_ERR(endpoint))
		return PTR_ERR(endpoint);

	rc = cxl_endpoint_autoremove(cxlmd, endpoint);
	if (rc)
		return rc;

	if (!endpoint->dev.driver) {
		dev_err(&cxlmd->dev, "%s failed probe\n",
			dev_name(&endpoint->dev));
		return -ENXIO;
	}

	return 0;
}

static int cxl_debugfs_poison_inject(void *data, u64 dpa)
{
	struct cxl_memdev *cxlmd = data;

	return cxl_inject_poison(cxlmd, dpa);
}

DEFINE_DEBUGFS_ATTRIBUTE(cxl_poison_inject_fops, NULL,
			 cxl_debugfs_poison_inject, "%llx\n");

static int cxl_debugfs_poison_clear(void *data, u64 dpa)
{
	struct cxl_memdev *cxlmd = data;

	return cxl_clear_poison(cxlmd, dpa);
}

DEFINE_DEBUGFS_ATTRIBUTE(cxl_poison_clear_fops, NULL,
			 cxl_debugfs_poison_clear, "%llx\n");

static int cxl_mem_probe(struct device *dev)
{
	struct cxl_memdev *cxlmd = to_cxl_memdev(dev);
	struct cxl_memdev_state *mds = to_cxl_memdev_state(cxlmd->cxlds);
	struct cxl_dev_state *cxlds = cxlmd->cxlds;
	struct device *endpoint_parent;
	struct cxl_port *parent_port;
	struct cxl_dport *dport;
	struct dentry *dentry;
	int rc;

	if (!cxlds->media_ready)
		return -EBUSY;

	 
	if (work_pending(&cxlmd->detach_work))
		return -EBUSY;

	dentry = cxl_debugfs_create_dir(dev_name(dev));
	debugfs_create_devm_seqfile(dev, "dpamem", dentry, cxl_mem_dpa_show);

	if (test_bit(CXL_POISON_ENABLED_INJECT, mds->poison.enabled_cmds))
		debugfs_create_file("inject_poison", 0200, dentry, cxlmd,
				    &cxl_poison_inject_fops);
	if (test_bit(CXL_POISON_ENABLED_CLEAR, mds->poison.enabled_cmds))
		debugfs_create_file("clear_poison", 0200, dentry, cxlmd,
				    &cxl_poison_clear_fops);

	rc = devm_add_action_or_reset(dev, remove_debugfs, dentry);
	if (rc)
		return rc;

	rc = devm_cxl_enumerate_ports(cxlmd);
	if (rc)
		return rc;

	parent_port = cxl_mem_find_port(cxlmd, &dport);
	if (!parent_port) {
		dev_err(dev, "CXL port topology not found\n");
		return -ENXIO;
	}

	if (dport->rch)
		endpoint_parent = parent_port->uport_dev;
	else
		endpoint_parent = &parent_port->dev;

	device_lock(endpoint_parent);
	if (!endpoint_parent->driver) {
		dev_err(dev, "CXL port topology %s not enabled\n",
			dev_name(endpoint_parent));
		rc = -ENXIO;
		goto unlock;
	}

	rc = devm_cxl_add_endpoint(endpoint_parent, cxlmd, dport);
unlock:
	device_unlock(endpoint_parent);
	put_device(&parent_port->dev);
	if (rc)
		return rc;

	if (resource_size(&cxlds->pmem_res) && IS_ENABLED(CONFIG_CXL_PMEM)) {
		rc = devm_cxl_add_nvdimm(cxlmd);
		if (rc == -ENODEV)
			dev_info(dev, "PMEM disabled by platform\n");
		else
			return rc;
	}

	 
	cxl_mem_active_inc();
	return devm_add_action_or_reset(dev, enable_suspend, NULL);
}

static ssize_t trigger_poison_list_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t len)
{
	bool trigger;
	int rc;

	if (kstrtobool(buf, &trigger) || !trigger)
		return -EINVAL;

	rc = cxl_trigger_poison_list(to_cxl_memdev(dev));

	return rc ? rc : len;
}
static DEVICE_ATTR_WO(trigger_poison_list);

static umode_t cxl_mem_visible(struct kobject *kobj, struct attribute *a, int n)
{
	if (a == &dev_attr_trigger_poison_list.attr) {
		struct device *dev = kobj_to_dev(kobj);
		struct cxl_memdev *cxlmd = to_cxl_memdev(dev);
		struct cxl_memdev_state *mds =
			to_cxl_memdev_state(cxlmd->cxlds);

		if (!test_bit(CXL_POISON_ENABLED_LIST,
			      mds->poison.enabled_cmds))
			return 0;
	}
	return a->mode;
}

static struct attribute *cxl_mem_attrs[] = {
	&dev_attr_trigger_poison_list.attr,
	NULL
};

static struct attribute_group cxl_mem_group = {
	.attrs = cxl_mem_attrs,
	.is_visible = cxl_mem_visible,
};

__ATTRIBUTE_GROUPS(cxl_mem);

static struct cxl_driver cxl_mem_driver = {
	.name = "cxl_mem",
	.probe = cxl_mem_probe,
	.id = CXL_DEVICE_MEMORY_EXPANDER,
	.drv = {
		.dev_groups = cxl_mem_groups,
	},
};

module_cxl_driver(cxl_mem_driver);

MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(CXL);
MODULE_ALIAS_CXL(CXL_DEVICE_MEMORY_EXPANDER);
 
MODULE_SOFTDEP("pre: cxl_port");
