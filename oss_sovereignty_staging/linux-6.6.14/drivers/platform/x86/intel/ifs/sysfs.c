
 

#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/slab.h>

#include "ifs.h"

 
static DEFINE_SEMAPHORE(ifs_sem, 1);

 
static ssize_t details_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct ifs_data *ifsd = ifs_get_data(dev);

	return sysfs_emit(buf, "%#llx\n", ifsd->scan_details);
}

static DEVICE_ATTR_RO(details);

static const char * const status_msg[] = {
	[SCAN_NOT_TESTED] = "untested",
	[SCAN_TEST_PASS] = "pass",
	[SCAN_TEST_FAIL] = "fail"
};

 
static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	struct ifs_data *ifsd = ifs_get_data(dev);

	return sysfs_emit(buf, "%s\n", status_msg[ifsd->status]);
}

static DEVICE_ATTR_RO(status);

 
static ssize_t run_test_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	unsigned int cpu;
	int rc;

	rc = kstrtouint(buf, 0, &cpu);
	if (rc < 0 || cpu >= nr_cpu_ids)
		return -EINVAL;

	if (down_interruptible(&ifs_sem))
		return -EINTR;

	rc = do_core_test(cpu, dev);

	up(&ifs_sem);

	return rc ? rc : count;
}

static DEVICE_ATTR_WO(run_test);

static ssize_t current_batch_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct ifs_data *ifsd = ifs_get_data(dev);
	unsigned int cur_batch;
	int rc;

	rc = kstrtouint(buf, 0, &cur_batch);
	if (rc < 0 || cur_batch > 0xff)
		return -EINVAL;

	if (down_interruptible(&ifs_sem))
		return -EINTR;

	ifsd->cur_batch = cur_batch;

	rc = ifs_load_firmware(dev);

	up(&ifs_sem);

	return (rc == 0) ? count : rc;
}

static ssize_t current_batch_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct ifs_data *ifsd = ifs_get_data(dev);

	if (!ifsd->loaded)
		return sysfs_emit(buf, "none\n");
	else
		return sysfs_emit(buf, "0x%02x\n", ifsd->cur_batch);
}

static DEVICE_ATTR_RW(current_batch);

 
static ssize_t image_version_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct ifs_data *ifsd = ifs_get_data(dev);

	if (!ifsd->loaded)
		return sysfs_emit(buf, "%s\n", "none");
	else
		return sysfs_emit(buf, "%#x\n", ifsd->loaded_version);
}

static DEVICE_ATTR_RO(image_version);

 
struct attribute *plat_ifs_attrs[] = {
	&dev_attr_details.attr,
	&dev_attr_status.attr,
	&dev_attr_run_test.attr,
	&dev_attr_current_batch.attr,
	&dev_attr_image_version.attr,
	NULL
};

 
struct attribute *plat_ifs_array_attrs[] = {
	&dev_attr_details.attr,
	&dev_attr_status.attr,
	&dev_attr_run_test.attr,
	NULL
};
