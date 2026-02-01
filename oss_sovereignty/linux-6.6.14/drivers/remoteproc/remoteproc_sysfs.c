
 

#include <linux/remoteproc.h>
#include <linux/slab.h>

#include "remoteproc_internal.h"

#define to_rproc(d) container_of(d, struct rproc, dev)

static ssize_t recovery_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct rproc *rproc = to_rproc(dev);

	return sysfs_emit(buf, "%s", rproc->recovery_disabled ? "disabled\n" : "enabled\n");
}

 
static ssize_t recovery_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct rproc *rproc = to_rproc(dev);

	if (sysfs_streq(buf, "enabled")) {
		 
		rproc->recovery_disabled = false;
		rproc_trigger_recovery(rproc);
	} else if (sysfs_streq(buf, "disabled")) {
		rproc->recovery_disabled = true;
	} else if (sysfs_streq(buf, "recover")) {
		 
		rproc_trigger_recovery(rproc);
	} else {
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_RW(recovery);

 
static const char * const rproc_coredump_str[] = {
	[RPROC_COREDUMP_DISABLED]	= "disabled",
	[RPROC_COREDUMP_ENABLED]	= "enabled",
	[RPROC_COREDUMP_INLINE]		= "inline",
};

 
static ssize_t coredump_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct rproc *rproc = to_rproc(dev);

	return sysfs_emit(buf, "%s\n", rproc_coredump_str[rproc->dump_conf]);
}

 
static ssize_t coredump_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct rproc *rproc = to_rproc(dev);

	if (rproc->state == RPROC_CRASHED) {
		dev_err(&rproc->dev, "can't change coredump configuration\n");
		return -EBUSY;
	}

	if (sysfs_streq(buf, "disabled")) {
		rproc->dump_conf = RPROC_COREDUMP_DISABLED;
	} else if (sysfs_streq(buf, "enabled")) {
		rproc->dump_conf = RPROC_COREDUMP_ENABLED;
	} else if (sysfs_streq(buf, "inline")) {
		rproc->dump_conf = RPROC_COREDUMP_INLINE;
	} else {
		dev_err(&rproc->dev, "Invalid coredump configuration\n");
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_RW(coredump);

 
static ssize_t firmware_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct rproc *rproc = to_rproc(dev);
	const char *firmware = rproc->firmware;

	 
	if (rproc->state == RPROC_ATTACHED)
		firmware = "unknown";

	return sprintf(buf, "%s\n", firmware);
}

 
static ssize_t firmware_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct rproc *rproc = to_rproc(dev);
	int err;

	err = rproc_set_firmware(rproc, buf);

	return err ? err : count;
}
static DEVICE_ATTR_RW(firmware);

 
static const char * const rproc_state_string[] = {
	[RPROC_OFFLINE]		= "offline",
	[RPROC_SUSPENDED]	= "suspended",
	[RPROC_RUNNING]		= "running",
	[RPROC_CRASHED]		= "crashed",
	[RPROC_DELETED]		= "deleted",
	[RPROC_ATTACHED]	= "attached",
	[RPROC_DETACHED]	= "detached",
	[RPROC_LAST]		= "invalid",
};

 
static ssize_t state_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct rproc *rproc = to_rproc(dev);
	unsigned int state;

	state = rproc->state > RPROC_LAST ? RPROC_LAST : rproc->state;
	return sprintf(buf, "%s\n", rproc_state_string[state]);
}

 
static ssize_t state_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct rproc *rproc = to_rproc(dev);
	int ret = 0;

	if (sysfs_streq(buf, "start")) {
		ret = rproc_boot(rproc);
		if (ret)
			dev_err(&rproc->dev, "Boot failed: %d\n", ret);
	} else if (sysfs_streq(buf, "stop")) {
		ret = rproc_shutdown(rproc);
	} else if (sysfs_streq(buf, "detach")) {
		ret = rproc_detach(rproc);
	} else {
		dev_err(&rproc->dev, "Unrecognised option: %s\n", buf);
		ret = -EINVAL;
	}
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(state);

 
static ssize_t name_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct rproc *rproc = to_rproc(dev);

	return sprintf(buf, "%s\n", rproc->name);
}
static DEVICE_ATTR_RO(name);

static umode_t rproc_is_visible(struct kobject *kobj, struct attribute *attr,
				int n)
{
	struct device *dev = kobj_to_dev(kobj);
	struct rproc *rproc = to_rproc(dev);
	umode_t mode = attr->mode;

	if (rproc->sysfs_read_only && (attr == &dev_attr_recovery.attr ||
				       attr == &dev_attr_firmware.attr ||
				       attr == &dev_attr_state.attr ||
				       attr == &dev_attr_coredump.attr))
		mode = 0444;

	return mode;
}

static struct attribute *rproc_attrs[] = {
	&dev_attr_coredump.attr,
	&dev_attr_recovery.attr,
	&dev_attr_firmware.attr,
	&dev_attr_state.attr,
	&dev_attr_name.attr,
	NULL
};

static const struct attribute_group rproc_devgroup = {
	.attrs = rproc_attrs,
	.is_visible = rproc_is_visible,
};

static const struct attribute_group *rproc_devgroups[] = {
	&rproc_devgroup,
	NULL
};

struct class rproc_class = {
	.name		= "remoteproc",
	.dev_groups	= rproc_devgroups,
};

int __init rproc_init_sysfs(void)
{
	 
	int err = class_register(&rproc_class);

	if (err)
		pr_err("remoteproc: unable to register class\n");
	return err;
}

void __exit rproc_exit_sysfs(void)
{
	class_unregister(&rproc_class);
}
