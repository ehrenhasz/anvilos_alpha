 

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/hw_random.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define RNG_MODULE_NAME		"hw_random"

static struct hwrng *current_rng;
 
static int cur_rng_set_by_user;
static struct task_struct *hwrng_fill;
 
static LIST_HEAD(rng_list);
 
static DEFINE_MUTEX(rng_mutex);
 
static DEFINE_MUTEX(reading_mutex);
static int data_avail;
static u8 *rng_buffer, *rng_fillbuf;
static unsigned short current_quality;
static unsigned short default_quality = 1024;  

module_param(current_quality, ushort, 0644);
MODULE_PARM_DESC(current_quality,
		 "current hwrng entropy estimation per 1024 bits of input -- obsolete, use rng_quality instead");
module_param(default_quality, ushort, 0644);
MODULE_PARM_DESC(default_quality,
		 "default maximum entropy content of hwrng per 1024 bits of input");

static void drop_current_rng(void);
static int hwrng_init(struct hwrng *rng);
static int hwrng_fillfn(void *unused);

static inline int rng_get_data(struct hwrng *rng, u8 *buffer, size_t size,
			       int wait);

static size_t rng_buffer_size(void)
{
	return SMP_CACHE_BYTES < 32 ? 32 : SMP_CACHE_BYTES;
}

static void add_early_randomness(struct hwrng *rng)
{
	int bytes_read;

	mutex_lock(&reading_mutex);
	bytes_read = rng_get_data(rng, rng_fillbuf, 32, 0);
	mutex_unlock(&reading_mutex);
	if (bytes_read > 0) {
		size_t entropy = bytes_read * 8 * rng->quality / 1024;
		add_hwgenerator_randomness(rng_fillbuf, bytes_read, entropy, false);
	}
}

static inline void cleanup_rng(struct kref *kref)
{
	struct hwrng *rng = container_of(kref, struct hwrng, ref);

	if (rng->cleanup)
		rng->cleanup(rng);

	complete(&rng->cleanup_done);
}

static int set_current_rng(struct hwrng *rng)
{
	int err;

	BUG_ON(!mutex_is_locked(&rng_mutex));

	err = hwrng_init(rng);
	if (err)
		return err;

	drop_current_rng();
	current_rng = rng;

	 
	if (!hwrng_fill) {
		hwrng_fill = kthread_run(hwrng_fillfn, NULL, "hwrng");
		if (IS_ERR(hwrng_fill)) {
			pr_err("hwrng_fill thread creation failed\n");
			hwrng_fill = NULL;
		}
	}

	return 0;
}

static void drop_current_rng(void)
{
	BUG_ON(!mutex_is_locked(&rng_mutex));
	if (!current_rng)
		return;

	 
	kref_put(&current_rng->ref, cleanup_rng);
	current_rng = NULL;
}

 
static struct hwrng *get_current_rng_nolock(void)
{
	if (current_rng)
		kref_get(&current_rng->ref);

	return current_rng;
}

static struct hwrng *get_current_rng(void)
{
	struct hwrng *rng;

	if (mutex_lock_interruptible(&rng_mutex))
		return ERR_PTR(-ERESTARTSYS);

	rng = get_current_rng_nolock();

	mutex_unlock(&rng_mutex);
	return rng;
}

static void put_rng(struct hwrng *rng)
{
	 
	mutex_lock(&rng_mutex);
	if (rng)
		kref_put(&rng->ref, cleanup_rng);
	mutex_unlock(&rng_mutex);
}

static int hwrng_init(struct hwrng *rng)
{
	if (kref_get_unless_zero(&rng->ref))
		goto skip_init;

	if (rng->init) {
		int ret;

		ret =  rng->init(rng);
		if (ret)
			return ret;
	}

	kref_init(&rng->ref);
	reinit_completion(&rng->cleanup_done);

skip_init:
	rng->quality = min_t(u16, min_t(u16, default_quality, 1024), rng->quality ?: 1024);
	current_quality = rng->quality;  

	return 0;
}

static int rng_dev_open(struct inode *inode, struct file *filp)
{
	 
	if ((filp->f_mode & FMODE_READ) == 0)
		return -EINVAL;
	if (filp->f_mode & FMODE_WRITE)
		return -EINVAL;
	return 0;
}

static inline int rng_get_data(struct hwrng *rng, u8 *buffer, size_t size,
			int wait) {
	int present;

	BUG_ON(!mutex_is_locked(&reading_mutex));
	if (rng->read)
		return rng->read(rng, (void *)buffer, size, wait);

	if (rng->data_present)
		present = rng->data_present(rng, wait);
	else
		present = 1;

	if (present)
		return rng->data_read(rng, (u32 *)buffer);

	return 0;
}

static ssize_t rng_dev_read(struct file *filp, char __user *buf,
			    size_t size, loff_t *offp)
{
	ssize_t ret = 0;
	int err = 0;
	int bytes_read, len;
	struct hwrng *rng;

	while (size) {
		rng = get_current_rng();
		if (IS_ERR(rng)) {
			err = PTR_ERR(rng);
			goto out;
		}
		if (!rng) {
			err = -ENODEV;
			goto out;
		}

		if (mutex_lock_interruptible(&reading_mutex)) {
			err = -ERESTARTSYS;
			goto out_put;
		}
		if (!data_avail) {
			bytes_read = rng_get_data(rng, rng_buffer,
				rng_buffer_size(),
				!(filp->f_flags & O_NONBLOCK));
			if (bytes_read < 0) {
				err = bytes_read;
				goto out_unlock_reading;
			}
			data_avail = bytes_read;
		}

		if (!data_avail) {
			if (filp->f_flags & O_NONBLOCK) {
				err = -EAGAIN;
				goto out_unlock_reading;
			}
		} else {
			len = data_avail;
			if (len > size)
				len = size;

			data_avail -= len;

			if (copy_to_user(buf + ret, rng_buffer + data_avail,
								len)) {
				err = -EFAULT;
				goto out_unlock_reading;
			}

			size -= len;
			ret += len;
		}

		mutex_unlock(&reading_mutex);
		put_rng(rng);

		if (need_resched())
			schedule_timeout_interruptible(1);

		if (signal_pending(current)) {
			err = -ERESTARTSYS;
			goto out;
		}
	}
out:
	return ret ? : err;

out_unlock_reading:
	mutex_unlock(&reading_mutex);
out_put:
	put_rng(rng);
	goto out;
}

static const struct file_operations rng_chrdev_ops = {
	.owner		= THIS_MODULE,
	.open		= rng_dev_open,
	.read		= rng_dev_read,
	.llseek		= noop_llseek,
};

static const struct attribute_group *rng_dev_groups[];

static struct miscdevice rng_miscdev = {
	.minor		= HWRNG_MINOR,
	.name		= RNG_MODULE_NAME,
	.nodename	= "hwrng",
	.fops		= &rng_chrdev_ops,
	.groups		= rng_dev_groups,
};

static int enable_best_rng(void)
{
	struct hwrng *rng, *new_rng = NULL;
	int ret = -ENODEV;

	BUG_ON(!mutex_is_locked(&rng_mutex));

	 
	if (list_empty(&rng_list)) {
		drop_current_rng();
		cur_rng_set_by_user = 0;
		return 0;
	}

	 
	list_for_each_entry(rng, &rng_list, list) {
		if (!new_rng || rng->quality > new_rng->quality)
			new_rng = rng;
	}

	ret = ((new_rng == current_rng) ? 0 : set_current_rng(new_rng));
	if (!ret)
		cur_rng_set_by_user = 0;

	return ret;
}

static ssize_t rng_current_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len)
{
	int err;
	struct hwrng *rng, *old_rng, *new_rng;

	err = mutex_lock_interruptible(&rng_mutex);
	if (err)
		return -ERESTARTSYS;

	old_rng = current_rng;
	if (sysfs_streq(buf, "")) {
		err = enable_best_rng();
	} else {
		list_for_each_entry(rng, &rng_list, list) {
			if (sysfs_streq(rng->name, buf)) {
				err = set_current_rng(rng);
				if (!err)
					cur_rng_set_by_user = 1;
				break;
			}
		}
	}
	new_rng = get_current_rng_nolock();
	mutex_unlock(&rng_mutex);

	if (new_rng) {
		if (new_rng != old_rng)
			add_early_randomness(new_rng);
		put_rng(new_rng);
	}

	return err ? : len;
}

static ssize_t rng_current_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	ssize_t ret;
	struct hwrng *rng;

	rng = get_current_rng();
	if (IS_ERR(rng))
		return PTR_ERR(rng);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", rng ? rng->name : "none");
	put_rng(rng);

	return ret;
}

static ssize_t rng_available_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	int err;
	struct hwrng *rng;

	err = mutex_lock_interruptible(&rng_mutex);
	if (err)
		return -ERESTARTSYS;
	buf[0] = '\0';
	list_for_each_entry(rng, &rng_list, list) {
		strlcat(buf, rng->name, PAGE_SIZE);
		strlcat(buf, " ", PAGE_SIZE);
	}
	strlcat(buf, "\n", PAGE_SIZE);
	mutex_unlock(&rng_mutex);

	return strlen(buf);
}

static ssize_t rng_selected_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	return sysfs_emit(buf, "%d\n", cur_rng_set_by_user);
}

static ssize_t rng_quality_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	ssize_t ret;
	struct hwrng *rng;

	rng = get_current_rng();
	if (IS_ERR(rng))
		return PTR_ERR(rng);

	if (!rng)  
		return -ENODEV;

	ret = sysfs_emit(buf, "%hu\n", rng->quality);
	put_rng(rng);

	return ret;
}

static ssize_t rng_quality_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len)
{
	u16 quality;
	int ret = -EINVAL;

	if (len < 2)
		return -EINVAL;

	ret = mutex_lock_interruptible(&rng_mutex);
	if (ret)
		return -ERESTARTSYS;

	ret = kstrtou16(buf, 0, &quality);
	if (ret || quality > 1024) {
		ret = -EINVAL;
		goto out;
	}

	if (!current_rng) {
		ret = -ENODEV;
		goto out;
	}

	current_rng->quality = quality;
	current_quality = quality;  

	 
	ret = enable_best_rng();

out:
	mutex_unlock(&rng_mutex);
	return ret ? ret : len;
}

static DEVICE_ATTR_RW(rng_current);
static DEVICE_ATTR_RO(rng_available);
static DEVICE_ATTR_RO(rng_selected);
static DEVICE_ATTR_RW(rng_quality);

static struct attribute *rng_dev_attrs[] = {
	&dev_attr_rng_current.attr,
	&dev_attr_rng_available.attr,
	&dev_attr_rng_selected.attr,
	&dev_attr_rng_quality.attr,
	NULL
};

ATTRIBUTE_GROUPS(rng_dev);

static void __exit unregister_miscdev(void)
{
	misc_deregister(&rng_miscdev);
}

static int __init register_miscdev(void)
{
	return misc_register(&rng_miscdev);
}

static int hwrng_fillfn(void *unused)
{
	size_t entropy, entropy_credit = 0;  
	long rc;

	while (!kthread_should_stop()) {
		unsigned short quality;
		struct hwrng *rng;

		rng = get_current_rng();
		if (IS_ERR(rng) || !rng)
			break;
		mutex_lock(&reading_mutex);
		rc = rng_get_data(rng, rng_fillbuf,
				  rng_buffer_size(), 1);
		if (current_quality != rng->quality)
			rng->quality = current_quality;  
		quality = rng->quality;
		mutex_unlock(&reading_mutex);

		if (rc <= 0)
			hwrng_msleep(rng, 10000);

		put_rng(rng);

		if (rc <= 0)
			continue;

		 
		entropy = rc * quality * 8 + entropy_credit;
		if ((entropy >> 10) == 0)
			entropy_credit = entropy;

		 
		add_hwgenerator_randomness((void *)rng_fillbuf, rc,
					   entropy >> 10, true);
	}
	hwrng_fill = NULL;
	return 0;
}

int hwrng_register(struct hwrng *rng)
{
	int err = -EINVAL;
	struct hwrng *tmp;
	bool is_new_current = false;

	if (!rng->name || (!rng->data_read && !rng->read))
		goto out;

	mutex_lock(&rng_mutex);

	 
	err = -EEXIST;
	list_for_each_entry(tmp, &rng_list, list) {
		if (strcmp(tmp->name, rng->name) == 0)
			goto out_unlock;
	}
	list_add_tail(&rng->list, &rng_list);

	init_completion(&rng->cleanup_done);
	complete(&rng->cleanup_done);
	init_completion(&rng->dying);

	if (!current_rng ||
	    (!cur_rng_set_by_user && rng->quality > current_rng->quality)) {
		 
		err = set_current_rng(rng);
		if (err)
			goto out_unlock;
		 
		is_new_current = true;
		kref_get(&rng->ref);
	}
	mutex_unlock(&rng_mutex);
	if (is_new_current || !rng->init) {
		 
		add_early_randomness(rng);
	}
	if (is_new_current)
		put_rng(rng);
	return 0;
out_unlock:
	mutex_unlock(&rng_mutex);
out:
	return err;
}
EXPORT_SYMBOL_GPL(hwrng_register);

void hwrng_unregister(struct hwrng *rng)
{
	struct hwrng *old_rng, *new_rng;
	int err;

	mutex_lock(&rng_mutex);

	old_rng = current_rng;
	list_del(&rng->list);
	complete_all(&rng->dying);
	if (current_rng == rng) {
		err = enable_best_rng();
		if (err) {
			drop_current_rng();
			cur_rng_set_by_user = 0;
		}
	}

	new_rng = get_current_rng_nolock();
	if (list_empty(&rng_list)) {
		mutex_unlock(&rng_mutex);
		if (hwrng_fill)
			kthread_stop(hwrng_fill);
	} else
		mutex_unlock(&rng_mutex);

	if (new_rng) {
		if (old_rng != new_rng)
			add_early_randomness(new_rng);
		put_rng(new_rng);
	}

	wait_for_completion(&rng->cleanup_done);
}
EXPORT_SYMBOL_GPL(hwrng_unregister);

static void devm_hwrng_release(struct device *dev, void *res)
{
	hwrng_unregister(*(struct hwrng **)res);
}

static int devm_hwrng_match(struct device *dev, void *res, void *data)
{
	struct hwrng **r = res;

	if (WARN_ON(!r || !*r))
		return 0;

	return *r == data;
}

int devm_hwrng_register(struct device *dev, struct hwrng *rng)
{
	struct hwrng **ptr;
	int error;

	ptr = devres_alloc(devm_hwrng_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	error = hwrng_register(rng);
	if (error) {
		devres_free(ptr);
		return error;
	}

	*ptr = rng;
	devres_add(dev, ptr);
	return 0;
}
EXPORT_SYMBOL_GPL(devm_hwrng_register);

void devm_hwrng_unregister(struct device *dev, struct hwrng *rng)
{
	devres_release(dev, devm_hwrng_release, devm_hwrng_match, rng);
}
EXPORT_SYMBOL_GPL(devm_hwrng_unregister);

long hwrng_msleep(struct hwrng *rng, unsigned int msecs)
{
	unsigned long timeout = msecs_to_jiffies(msecs) + 1;

	return wait_for_completion_interruptible_timeout(&rng->dying, timeout);
}
EXPORT_SYMBOL_GPL(hwrng_msleep);

long hwrng_yield(struct hwrng *rng)
{
	return wait_for_completion_interruptible_timeout(&rng->dying, 1);
}
EXPORT_SYMBOL_GPL(hwrng_yield);

static int __init hwrng_modinit(void)
{
	int ret;

	 
	rng_buffer = kmalloc(rng_buffer_size(), GFP_KERNEL);
	if (!rng_buffer)
		return -ENOMEM;

	rng_fillbuf = kmalloc(rng_buffer_size(), GFP_KERNEL);
	if (!rng_fillbuf) {
		kfree(rng_buffer);
		return -ENOMEM;
	}

	ret = register_miscdev();
	if (ret) {
		kfree(rng_fillbuf);
		kfree(rng_buffer);
	}

	return ret;
}

static void __exit hwrng_modexit(void)
{
	mutex_lock(&rng_mutex);
	BUG_ON(current_rng);
	kfree(rng_buffer);
	kfree(rng_fillbuf);
	mutex_unlock(&rng_mutex);

	unregister_miscdev();
}

fs_initcall(hwrng_modinit);  
module_exit(hwrng_modexit);

MODULE_DESCRIPTION("H/W Random Number Generator (RNG) driver");
MODULE_LICENSE("GPL");
