
 
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sizes.h>
#include <linux/ndctl.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/nd.h>
#include "label.h"
#include "nd.h"

static int nvdimm_probe(struct device *dev)
{
	struct nvdimm_drvdata *ndd;
	int rc;

	rc = nvdimm_security_setup_events(dev);
	if (rc < 0) {
		dev_err(dev, "security event setup failed: %d\n", rc);
		return rc;
	}

	rc = nvdimm_check_config_data(dev);
	if (rc) {
		 
		if (rc == -ENOTTY)
			rc = 0;
		return rc;
	}

	 
	nvdimm_clear_locked(dev);

	ndd = kzalloc(sizeof(*ndd), GFP_KERNEL);
	if (!ndd)
		return -ENOMEM;

	dev_set_drvdata(dev, ndd);
	ndd->dpa.name = dev_name(dev);
	ndd->ns_current = -1;
	ndd->ns_next = -1;
	ndd->dpa.start = 0;
	ndd->dpa.end = -1;
	ndd->dev = dev;
	get_device(dev);
	kref_init(&ndd->kref);

	 
	rc = nvdimm_security_unlock(dev);
	if (rc < 0)
		dev_dbg(dev, "failed to unlock dimm: %d\n", rc);


	 
	rc = nvdimm_init_nsarea(ndd);
	if (rc == -EACCES) {
		 
		nvdimm_set_locked(dev);
		rc = 0;
	}
	if (rc)
		goto err;

	 
	rc = nd_label_data_init(ndd);
	if (rc == -EACCES)
		nvdimm_set_locked(dev);
	if (rc)
		goto err;

	dev_dbg(dev, "config data size: %d\n", ndd->nsarea.config_size);

	nvdimm_bus_lock(dev);
	if (ndd->ns_current >= 0) {
		rc = nd_label_reserve_dpa(ndd);
		if (rc == 0)
			nvdimm_set_labeling(dev);
	}
	nvdimm_bus_unlock(dev);

	if (rc)
		goto err;

	return 0;

 err:
	put_ndd(ndd);
	return rc;
}

static void nvdimm_remove(struct device *dev)
{
	struct nvdimm_drvdata *ndd = dev_get_drvdata(dev);

	nvdimm_bus_lock(dev);
	dev_set_drvdata(dev, NULL);
	nvdimm_bus_unlock(dev);
	put_ndd(ndd);
}

static struct nd_device_driver nvdimm_driver = {
	.probe = nvdimm_probe,
	.remove = nvdimm_remove,
	.drv = {
		.name = "nvdimm",
	},
	.type = ND_DRIVER_DIMM,
};

int __init nvdimm_init(void)
{
	return nd_driver_register(&nvdimm_driver);
}

void nvdimm_exit(void)
{
	driver_unregister(&nvdimm_driver.drv);
}

MODULE_ALIAS_ND_DEVICE(ND_DEVICE_DIMM);
