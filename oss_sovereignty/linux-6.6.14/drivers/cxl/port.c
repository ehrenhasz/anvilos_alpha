
 
#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "cxlmem.h"
#include "cxlpci.h"

 

static void schedule_detach(void *cxlmd)
{
	schedule_cxl_memdev_detach(cxlmd);
}

static int discover_region(struct device *dev, void *root)
{
	struct cxl_endpoint_decoder *cxled;
	int rc;

	if (!is_endpoint_decoder(dev))
		return 0;

	cxled = to_cxl_endpoint_decoder(dev);
	if ((cxled->cxld.flags & CXL_DECODER_F_ENABLE) == 0)
		return 0;

	if (cxled->state != CXL_DECODER_STATE_AUTO)
		return 0;

	 
	rc = cxl_add_to_region(root, cxled);
	if (rc)
		dev_dbg(dev, "failed to add to region: %#llx-%#llx\n",
			cxled->cxld.hpa_range.start, cxled->cxld.hpa_range.end);

	return 0;
}

static int cxl_switch_port_probe(struct cxl_port *port)
{
	struct cxl_hdm *cxlhdm;
	int rc;

	rc = devm_cxl_port_enumerate_dports(port);
	if (rc < 0)
		return rc;

	cxlhdm = devm_cxl_setup_hdm(port, NULL);
	if (!IS_ERR(cxlhdm))
		return devm_cxl_enumerate_decoders(cxlhdm, NULL);

	if (PTR_ERR(cxlhdm) != -ENODEV) {
		dev_err(&port->dev, "Failed to map HDM decoder capability\n");
		return PTR_ERR(cxlhdm);
	}

	if (rc == 1) {
		dev_dbg(&port->dev, "Fallback to passthrough decoder\n");
		return devm_cxl_add_passthrough_decoder(port);
	}

	dev_err(&port->dev, "HDM decoder capability not found\n");
	return -ENXIO;
}

static int cxl_endpoint_port_probe(struct cxl_port *port)
{
	struct cxl_endpoint_dvsec_info info = { .port = port };
	struct cxl_memdev *cxlmd = to_cxl_memdev(port->uport_dev);
	struct cxl_dev_state *cxlds = cxlmd->cxlds;
	struct cxl_hdm *cxlhdm;
	struct cxl_port *root;
	int rc;

	rc = cxl_dvsec_rr_decode(cxlds->dev, cxlds->cxl_dvsec, &info);
	if (rc < 0)
		return rc;

	cxlhdm = devm_cxl_setup_hdm(port, &info);
	if (IS_ERR(cxlhdm)) {
		if (PTR_ERR(cxlhdm) == -ENODEV)
			dev_err(&port->dev, "HDM decoder registers not found\n");
		return PTR_ERR(cxlhdm);
	}

	 
	read_cdat_data(port);

	get_device(&cxlmd->dev);
	rc = devm_add_action_or_reset(&port->dev, schedule_detach, cxlmd);
	if (rc)
		return rc;

	rc = cxl_hdm_decode_init(cxlds, cxlhdm, &info);
	if (rc)
		return rc;

	rc = devm_cxl_enumerate_decoders(cxlhdm, &info);
	if (rc)
		return rc;

	 
	root = find_cxl_root(port);

	 
	device_for_each_child(&port->dev, root, discover_region);
	put_device(&root->dev);

	return 0;
}

static int cxl_port_probe(struct device *dev)
{
	struct cxl_port *port = to_cxl_port(dev);

	if (is_cxl_endpoint(port))
		return cxl_endpoint_port_probe(port);
	return cxl_switch_port_probe(port);
}

static ssize_t CDAT_read(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *bin_attr, char *buf,
			 loff_t offset, size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct cxl_port *port = to_cxl_port(dev);

	if (!port->cdat_available)
		return -ENXIO;

	if (!port->cdat.table)
		return 0;

	return memory_read_from_buffer(buf, count, &offset,
				       port->cdat.table,
				       port->cdat.length);
}

static BIN_ATTR_ADMIN_RO(CDAT, 0);

static umode_t cxl_port_bin_attr_is_visible(struct kobject *kobj,
					    struct bin_attribute *attr, int i)
{
	struct device *dev = kobj_to_dev(kobj);
	struct cxl_port *port = to_cxl_port(dev);

	if ((attr == &bin_attr_CDAT) && port->cdat_available)
		return attr->attr.mode;

	return 0;
}

static struct bin_attribute *cxl_cdat_bin_attributes[] = {
	&bin_attr_CDAT,
	NULL,
};

static struct attribute_group cxl_cdat_attribute_group = {
	.bin_attrs = cxl_cdat_bin_attributes,
	.is_bin_visible = cxl_port_bin_attr_is_visible,
};

static const struct attribute_group *cxl_port_attribute_groups[] = {
	&cxl_cdat_attribute_group,
	NULL,
};

static struct cxl_driver cxl_port_driver = {
	.name = "cxl_port",
	.probe = cxl_port_probe,
	.id = CXL_DEVICE_PORT,
	.drv = {
		.dev_groups = cxl_port_attribute_groups,
	},
};

module_cxl_driver(cxl_port_driver);
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(CXL);
MODULE_ALIAS_CXL(CXL_DEVICE_PORT);
