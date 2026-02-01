
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <media/media-devnode.h>
#include <media/media-device.h>

#define MEDIA_NUM_DEVICES	256
#define MEDIA_NAME		"media"

static dev_t media_dev_t;

 
static DEFINE_MUTEX(media_devnode_lock);
static DECLARE_BITMAP(media_devnode_nums, MEDIA_NUM_DEVICES);

 
static void media_devnode_release(struct device *cd)
{
	struct media_devnode *devnode = to_media_devnode(cd);

	mutex_lock(&media_devnode_lock);
	 
	clear_bit(devnode->minor, media_devnode_nums);
	mutex_unlock(&media_devnode_lock);

	 
	if (devnode->release)
		devnode->release(devnode);

	kfree(devnode);
	pr_debug("%s: Media Devnode Deallocated\n", __func__);
}

static struct bus_type media_bus_type = {
	.name = MEDIA_NAME,
};

static ssize_t media_read(struct file *filp, char __user *buf,
		size_t sz, loff_t *off)
{
	struct media_devnode *devnode = media_devnode_data(filp);

	if (!devnode->fops->read)
		return -EINVAL;
	if (!media_devnode_is_registered(devnode))
		return -EIO;
	return devnode->fops->read(filp, buf, sz, off);
}

static ssize_t media_write(struct file *filp, const char __user *buf,
		size_t sz, loff_t *off)
{
	struct media_devnode *devnode = media_devnode_data(filp);

	if (!devnode->fops->write)
		return -EINVAL;
	if (!media_devnode_is_registered(devnode))
		return -EIO;
	return devnode->fops->write(filp, buf, sz, off);
}

static __poll_t media_poll(struct file *filp,
			       struct poll_table_struct *poll)
{
	struct media_devnode *devnode = media_devnode_data(filp);

	if (!media_devnode_is_registered(devnode))
		return EPOLLERR | EPOLLHUP;
	if (!devnode->fops->poll)
		return DEFAULT_POLLMASK;
	return devnode->fops->poll(filp, poll);
}

static long
__media_ioctl(struct file *filp, unsigned int cmd, unsigned long arg,
	      long (*ioctl_func)(struct file *filp, unsigned int cmd,
				 unsigned long arg))
{
	struct media_devnode *devnode = media_devnode_data(filp);

	if (!ioctl_func)
		return -ENOTTY;

	if (!media_devnode_is_registered(devnode))
		return -EIO;

	return ioctl_func(filp, cmd, arg);
}

static long media_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct media_devnode *devnode = media_devnode_data(filp);

	return __media_ioctl(filp, cmd, arg, devnode->fops->ioctl);
}

#ifdef CONFIG_COMPAT

static long media_compat_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	struct media_devnode *devnode = media_devnode_data(filp);

	return __media_ioctl(filp, cmd, arg, devnode->fops->compat_ioctl);
}

#endif  

 
static int media_open(struct inode *inode, struct file *filp)
{
	struct media_devnode *devnode;
	int ret;

	 
	mutex_lock(&media_devnode_lock);
	devnode = container_of(inode->i_cdev, struct media_devnode, cdev);
	 
	if (!media_devnode_is_registered(devnode)) {
		mutex_unlock(&media_devnode_lock);
		return -ENXIO;
	}
	 
	get_device(&devnode->dev);
	mutex_unlock(&media_devnode_lock);

	filp->private_data = devnode;

	if (devnode->fops->open) {
		ret = devnode->fops->open(filp);
		if (ret) {
			put_device(&devnode->dev);
			filp->private_data = NULL;
			return ret;
		}
	}

	return 0;
}

 
static int media_release(struct inode *inode, struct file *filp)
{
	struct media_devnode *devnode = media_devnode_data(filp);

	if (devnode->fops->release)
		devnode->fops->release(filp);

	filp->private_data = NULL;

	 
	put_device(&devnode->dev);

	pr_debug("%s: Media Release\n", __func__);
	return 0;
}

static const struct file_operations media_devnode_fops = {
	.owner = THIS_MODULE,
	.read = media_read,
	.write = media_write,
	.open = media_open,
	.unlocked_ioctl = media_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = media_compat_ioctl,
#endif  
	.release = media_release,
	.poll = media_poll,
	.llseek = no_llseek,
};

int __must_check media_devnode_register(struct media_device *mdev,
					struct media_devnode *devnode,
					struct module *owner)
{
	int minor;
	int ret;

	 
	mutex_lock(&media_devnode_lock);
	minor = find_first_zero_bit(media_devnode_nums, MEDIA_NUM_DEVICES);
	if (minor == MEDIA_NUM_DEVICES) {
		mutex_unlock(&media_devnode_lock);
		pr_err("could not get a free minor\n");
		kfree(devnode);
		return -ENFILE;
	}

	set_bit(minor, media_devnode_nums);
	mutex_unlock(&media_devnode_lock);

	devnode->minor = minor;
	devnode->media_dev = mdev;

	 
	devnode->dev.bus = &media_bus_type;
	devnode->dev.devt = MKDEV(MAJOR(media_dev_t), devnode->minor);
	devnode->dev.release = media_devnode_release;
	if (devnode->parent)
		devnode->dev.parent = devnode->parent;
	dev_set_name(&devnode->dev, "media%d", devnode->minor);
	device_initialize(&devnode->dev);

	 
	cdev_init(&devnode->cdev, &media_devnode_fops);
	devnode->cdev.owner = owner;
	kobject_set_name(&devnode->cdev.kobj, "media%d", devnode->minor);

	 
	ret = cdev_device_add(&devnode->cdev, &devnode->dev);
	if (ret < 0) {
		pr_err("%s: cdev_device_add failed\n", __func__);
		goto cdev_add_error;
	}

	 
	set_bit(MEDIA_FLAG_REGISTERED, &devnode->flags);

	return 0;

cdev_add_error:
	mutex_lock(&media_devnode_lock);
	clear_bit(devnode->minor, media_devnode_nums);
	devnode->media_dev = NULL;
	mutex_unlock(&media_devnode_lock);

	put_device(&devnode->dev);
	return ret;
}

void media_devnode_unregister_prepare(struct media_devnode *devnode)
{
	 
	if (!media_devnode_is_registered(devnode))
		return;

	mutex_lock(&media_devnode_lock);
	clear_bit(MEDIA_FLAG_REGISTERED, &devnode->flags);
	mutex_unlock(&media_devnode_lock);
}

void media_devnode_unregister(struct media_devnode *devnode)
{
	mutex_lock(&media_devnode_lock);
	 
	cdev_device_del(&devnode->cdev, &devnode->dev);
	devnode->media_dev = NULL;
	mutex_unlock(&media_devnode_lock);

	put_device(&devnode->dev);
}

 
static int __init media_devnode_init(void)
{
	int ret;

	pr_info("Linux media interface: v0.10\n");
	ret = alloc_chrdev_region(&media_dev_t, 0, MEDIA_NUM_DEVICES,
				  MEDIA_NAME);
	if (ret < 0) {
		pr_warn("unable to allocate major\n");
		return ret;
	}

	ret = bus_register(&media_bus_type);
	if (ret < 0) {
		unregister_chrdev_region(media_dev_t, MEDIA_NUM_DEVICES);
		pr_warn("bus_register failed\n");
		return -EIO;
	}

	return 0;
}

static void __exit media_devnode_exit(void)
{
	bus_unregister(&media_bus_type);
	unregister_chrdev_region(media_dev_t, MEDIA_NUM_DEVICES);
}

subsys_initcall(media_devnode_init);
module_exit(media_devnode_exit)

MODULE_AUTHOR("Laurent Pinchart <laurent.pinchart@ideasonboard.com>");
MODULE_DESCRIPTION("Device node registration for media drivers");
MODULE_LICENSE("GPL");
