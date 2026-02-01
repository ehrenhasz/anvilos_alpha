 

#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mount.h>
#include <linux/pseudo_fs.h>
#include <linux/slab.h>
#include <linux/srcu.h>

#include <drm/drm_accel.h>
#include <drm/drm_cache.h>
#include <drm/drm_client.h>
#include <drm/drm_color_mgmt.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_managed.h>
#include <drm/drm_mode_object.h>
#include <drm/drm_print.h>
#include <drm/drm_privacy_screen_machine.h>

#include "drm_crtc_internal.h"
#include "drm_internal.h"
#include "drm_legacy.h"

MODULE_AUTHOR("Gareth Hughes, Leif Delgass, José Fonseca, Jon Smirl");
MODULE_DESCRIPTION("DRM shared core routines");
MODULE_LICENSE("GPL and additional rights");

static DEFINE_SPINLOCK(drm_minor_lock);
static struct idr drm_minors_idr;

 
static bool drm_core_init_complete;

static struct dentry *drm_debugfs_root;

DEFINE_STATIC_SRCU(drm_unplug_srcu);

 

static struct drm_minor **drm_minor_get_slot(struct drm_device *dev,
					     enum drm_minor_type type)
{
	switch (type) {
	case DRM_MINOR_PRIMARY:
		return &dev->primary;
	case DRM_MINOR_RENDER:
		return &dev->render;
	case DRM_MINOR_ACCEL:
		return &dev->accel;
	default:
		BUG();
	}
}

static void drm_minor_alloc_release(struct drm_device *dev, void *data)
{
	struct drm_minor *minor = data;
	unsigned long flags;

	WARN_ON(dev != minor->dev);

	put_device(minor->kdev);

	if (minor->type == DRM_MINOR_ACCEL) {
		accel_minor_remove(minor->index);
	} else {
		spin_lock_irqsave(&drm_minor_lock, flags);
		idr_remove(&drm_minors_idr, minor->index);
		spin_unlock_irqrestore(&drm_minor_lock, flags);
	}
}

static int drm_minor_alloc(struct drm_device *dev, enum drm_minor_type type)
{
	struct drm_minor *minor;
	unsigned long flags;
	int r;

	minor = drmm_kzalloc(dev, sizeof(*minor), GFP_KERNEL);
	if (!minor)
		return -ENOMEM;

	minor->type = type;
	minor->dev = dev;

	idr_preload(GFP_KERNEL);
	if (type == DRM_MINOR_ACCEL) {
		r = accel_minor_alloc();
	} else {
		spin_lock_irqsave(&drm_minor_lock, flags);
		r = idr_alloc(&drm_minors_idr,
			NULL,
			64 * type,
			64 * (type + 1),
			GFP_NOWAIT);
		spin_unlock_irqrestore(&drm_minor_lock, flags);
	}
	idr_preload_end();

	if (r < 0)
		return r;

	minor->index = r;

	r = drmm_add_action_or_reset(dev, drm_minor_alloc_release, minor);
	if (r)
		return r;

	minor->kdev = drm_sysfs_minor_alloc(minor);
	if (IS_ERR(minor->kdev))
		return PTR_ERR(minor->kdev);

	*drm_minor_get_slot(dev, type) = minor;
	return 0;
}

static int drm_minor_register(struct drm_device *dev, enum drm_minor_type type)
{
	struct drm_minor *minor;
	unsigned long flags;
	int ret;

	DRM_DEBUG("\n");

	minor = *drm_minor_get_slot(dev, type);
	if (!minor)
		return 0;

	if (minor->type == DRM_MINOR_ACCEL) {
		accel_debugfs_init(minor, minor->index);
	} else {
		ret = drm_debugfs_init(minor, minor->index, drm_debugfs_root);
		if (ret) {
			DRM_ERROR("DRM: Failed to initialize /sys/kernel/debug/dri.\n");
			goto err_debugfs;
		}
	}

	ret = device_add(minor->kdev);
	if (ret)
		goto err_debugfs;

	 
	if (minor->type == DRM_MINOR_ACCEL) {
		accel_minor_replace(minor, minor->index);
	} else {
		spin_lock_irqsave(&drm_minor_lock, flags);
		idr_replace(&drm_minors_idr, minor, minor->index);
		spin_unlock_irqrestore(&drm_minor_lock, flags);
	}

	DRM_DEBUG("new minor registered %d\n", minor->index);
	return 0;

err_debugfs:
	drm_debugfs_cleanup(minor);
	return ret;
}

static void drm_minor_unregister(struct drm_device *dev, enum drm_minor_type type)
{
	struct drm_minor *minor;
	unsigned long flags;

	minor = *drm_minor_get_slot(dev, type);
	if (!minor || !device_is_registered(minor->kdev))
		return;

	 
	if (minor->type == DRM_MINOR_ACCEL) {
		accel_minor_replace(NULL, minor->index);
	} else {
		spin_lock_irqsave(&drm_minor_lock, flags);
		idr_replace(&drm_minors_idr, NULL, minor->index);
		spin_unlock_irqrestore(&drm_minor_lock, flags);
	}

	device_del(minor->kdev);
	dev_set_drvdata(minor->kdev, NULL);  
	drm_debugfs_cleanup(minor);
}

 
struct drm_minor *drm_minor_acquire(unsigned int minor_id)
{
	struct drm_minor *minor;
	unsigned long flags;

	spin_lock_irqsave(&drm_minor_lock, flags);
	minor = idr_find(&drm_minors_idr, minor_id);
	if (minor)
		drm_dev_get(minor->dev);
	spin_unlock_irqrestore(&drm_minor_lock, flags);

	if (!minor) {
		return ERR_PTR(-ENODEV);
	} else if (drm_dev_is_unplugged(minor->dev)) {
		drm_dev_put(minor->dev);
		return ERR_PTR(-ENODEV);
	}

	return minor;
}

void drm_minor_release(struct drm_minor *minor)
{
	drm_dev_put(minor->dev);
}

 

 
void drm_put_dev(struct drm_device *dev)
{
	DRM_DEBUG("\n");

	if (!dev) {
		DRM_ERROR("cleanup called no dev\n");
		return;
	}

	drm_dev_unregister(dev);
	drm_dev_put(dev);
}
EXPORT_SYMBOL(drm_put_dev);

 
bool drm_dev_enter(struct drm_device *dev, int *idx)
{
	*idx = srcu_read_lock(&drm_unplug_srcu);

	if (dev->unplugged) {
		srcu_read_unlock(&drm_unplug_srcu, *idx);
		return false;
	}

	return true;
}
EXPORT_SYMBOL(drm_dev_enter);

 
void drm_dev_exit(int idx)
{
	srcu_read_unlock(&drm_unplug_srcu, idx);
}
EXPORT_SYMBOL(drm_dev_exit);

 
void drm_dev_unplug(struct drm_device *dev)
{
	 
	dev->unplugged = true;
	synchronize_srcu(&drm_unplug_srcu);

	drm_dev_unregister(dev);

	 
	unmap_mapping_range(dev->anon_inode->i_mapping, 0, 0, 1);
}
EXPORT_SYMBOL(drm_dev_unplug);

 

static int drm_fs_cnt;
static struct vfsmount *drm_fs_mnt;

static int drm_fs_init_fs_context(struct fs_context *fc)
{
	return init_pseudo(fc, 0x010203ff) ? 0 : -ENOMEM;
}

static struct file_system_type drm_fs_type = {
	.name		= "drm",
	.owner		= THIS_MODULE,
	.init_fs_context = drm_fs_init_fs_context,
	.kill_sb	= kill_anon_super,
};

static struct inode *drm_fs_inode_new(void)
{
	struct inode *inode;
	int r;

	r = simple_pin_fs(&drm_fs_type, &drm_fs_mnt, &drm_fs_cnt);
	if (r < 0) {
		DRM_ERROR("Cannot mount pseudo fs: %d\n", r);
		return ERR_PTR(r);
	}

	inode = alloc_anon_inode(drm_fs_mnt->mnt_sb);
	if (IS_ERR(inode))
		simple_release_fs(&drm_fs_mnt, &drm_fs_cnt);

	return inode;
}

static void drm_fs_inode_free(struct inode *inode)
{
	if (inode) {
		iput(inode);
		simple_release_fs(&drm_fs_mnt, &drm_fs_cnt);
	}
}

 

static void drm_dev_init_release(struct drm_device *dev, void *res)
{
	drm_legacy_ctxbitmap_cleanup(dev);
	drm_legacy_remove_map_hash(dev);
	drm_fs_inode_free(dev->anon_inode);

	put_device(dev->dev);
	 
	dev->dev = NULL;
	mutex_destroy(&dev->master_mutex);
	mutex_destroy(&dev->clientlist_mutex);
	mutex_destroy(&dev->filelist_mutex);
	mutex_destroy(&dev->struct_mutex);
	mutex_destroy(&dev->debugfs_mutex);
	drm_legacy_destroy_members(dev);
}

static int drm_dev_init(struct drm_device *dev,
			const struct drm_driver *driver,
			struct device *parent)
{
	struct inode *inode;
	int ret;

	if (!drm_core_init_complete) {
		DRM_ERROR("DRM core is not initialized\n");
		return -ENODEV;
	}

	if (WARN_ON(!parent))
		return -EINVAL;

	kref_init(&dev->ref);
	dev->dev = get_device(parent);
	dev->driver = driver;

	INIT_LIST_HEAD(&dev->managed.resources);
	spin_lock_init(&dev->managed.lock);

	 
	dev->driver_features = ~0u;

	if (drm_core_check_feature(dev, DRIVER_COMPUTE_ACCEL) &&
				(drm_core_check_feature(dev, DRIVER_RENDER) ||
				drm_core_check_feature(dev, DRIVER_MODESET))) {
		DRM_ERROR("DRM driver can't be both a compute acceleration and graphics driver\n");
		return -EINVAL;
	}

	drm_legacy_init_members(dev);
	INIT_LIST_HEAD(&dev->filelist);
	INIT_LIST_HEAD(&dev->filelist_internal);
	INIT_LIST_HEAD(&dev->clientlist);
	INIT_LIST_HEAD(&dev->vblank_event_list);
	INIT_LIST_HEAD(&dev->debugfs_list);

	spin_lock_init(&dev->event_lock);
	mutex_init(&dev->struct_mutex);
	mutex_init(&dev->filelist_mutex);
	mutex_init(&dev->clientlist_mutex);
	mutex_init(&dev->master_mutex);
	mutex_init(&dev->debugfs_mutex);

	ret = drmm_add_action_or_reset(dev, drm_dev_init_release, NULL);
	if (ret)
		return ret;

	inode = drm_fs_inode_new();
	if (IS_ERR(inode)) {
		ret = PTR_ERR(inode);
		DRM_ERROR("Cannot allocate anonymous inode: %d\n", ret);
		goto err;
	}

	dev->anon_inode = inode;

	if (drm_core_check_feature(dev, DRIVER_COMPUTE_ACCEL)) {
		ret = drm_minor_alloc(dev, DRM_MINOR_ACCEL);
		if (ret)
			goto err;
	} else {
		if (drm_core_check_feature(dev, DRIVER_RENDER)) {
			ret = drm_minor_alloc(dev, DRM_MINOR_RENDER);
			if (ret)
				goto err;
		}

		ret = drm_minor_alloc(dev, DRM_MINOR_PRIMARY);
		if (ret)
			goto err;
	}

	ret = drm_legacy_create_map_hash(dev);
	if (ret)
		goto err;

	drm_legacy_ctxbitmap_init(dev);

	if (drm_core_check_feature(dev, DRIVER_GEM)) {
		ret = drm_gem_init(dev);
		if (ret) {
			DRM_ERROR("Cannot initialize graphics execution manager (GEM)\n");
			goto err;
		}
	}

	dev->unique = drmm_kstrdup(dev, dev_name(parent), GFP_KERNEL);
	if (!dev->unique) {
		ret = -ENOMEM;
		goto err;
	}

	return 0;

err:
	drm_managed_release(dev);

	return ret;
}

static void devm_drm_dev_init_release(void *data)
{
	drm_dev_put(data);
}

static int devm_drm_dev_init(struct device *parent,
			     struct drm_device *dev,
			     const struct drm_driver *driver)
{
	int ret;

	ret = drm_dev_init(dev, driver, parent);
	if (ret)
		return ret;

	return devm_add_action_or_reset(parent,
					devm_drm_dev_init_release, dev);
}

void *__devm_drm_dev_alloc(struct device *parent,
			   const struct drm_driver *driver,
			   size_t size, size_t offset)
{
	void *container;
	struct drm_device *drm;
	int ret;

	container = kzalloc(size, GFP_KERNEL);
	if (!container)
		return ERR_PTR(-ENOMEM);

	drm = container + offset;
	ret = devm_drm_dev_init(parent, drm, driver);
	if (ret) {
		kfree(container);
		return ERR_PTR(ret);
	}
	drmm_add_final_kfree(drm, container);

	return container;
}
EXPORT_SYMBOL(__devm_drm_dev_alloc);

 
struct drm_device *drm_dev_alloc(const struct drm_driver *driver,
				 struct device *parent)
{
	struct drm_device *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return ERR_PTR(-ENOMEM);

	ret = drm_dev_init(dev, driver, parent);
	if (ret) {
		kfree(dev);
		return ERR_PTR(ret);
	}

	drmm_add_final_kfree(dev, dev);

	return dev;
}
EXPORT_SYMBOL(drm_dev_alloc);

static void drm_dev_release(struct kref *ref)
{
	struct drm_device *dev = container_of(ref, struct drm_device, ref);

	if (dev->driver->release)
		dev->driver->release(dev);

	drm_managed_release(dev);

	kfree(dev->managed.final_kfree);
}

 
void drm_dev_get(struct drm_device *dev)
{
	if (dev)
		kref_get(&dev->ref);
}
EXPORT_SYMBOL(drm_dev_get);

 
void drm_dev_put(struct drm_device *dev)
{
	if (dev)
		kref_put(&dev->ref, drm_dev_release);
}
EXPORT_SYMBOL(drm_dev_put);

static int create_compat_control_link(struct drm_device *dev)
{
	struct drm_minor *minor;
	char *name;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return 0;

	minor = *drm_minor_get_slot(dev, DRM_MINOR_PRIMARY);
	if (!minor)
		return 0;

	 
	name = kasprintf(GFP_KERNEL, "controlD%d", minor->index + 64);
	if (!name)
		return -ENOMEM;

	ret = sysfs_create_link(minor->kdev->kobj.parent,
				&minor->kdev->kobj,
				name);

	kfree(name);

	return ret;
}

static void remove_compat_control_link(struct drm_device *dev)
{
	struct drm_minor *minor;
	char *name;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return;

	minor = *drm_minor_get_slot(dev, DRM_MINOR_PRIMARY);
	if (!minor)
		return;

	name = kasprintf(GFP_KERNEL, "controlD%d", minor->index + 64);
	if (!name)
		return;

	sysfs_remove_link(minor->kdev->kobj.parent, name);

	kfree(name);
}

 
int drm_dev_register(struct drm_device *dev, unsigned long flags)
{
	const struct drm_driver *driver = dev->driver;
	int ret;

	if (!driver->load)
		drm_mode_config_validate(dev);

	WARN_ON(!dev->managed.final_kfree);

	if (drm_dev_needs_global_mutex(dev))
		mutex_lock(&drm_global_mutex);

	ret = drm_minor_register(dev, DRM_MINOR_RENDER);
	if (ret)
		goto err_minors;

	ret = drm_minor_register(dev, DRM_MINOR_PRIMARY);
	if (ret)
		goto err_minors;

	ret = drm_minor_register(dev, DRM_MINOR_ACCEL);
	if (ret)
		goto err_minors;

	ret = create_compat_control_link(dev);
	if (ret)
		goto err_minors;

	dev->registered = true;

	if (driver->load) {
		ret = driver->load(dev, flags);
		if (ret)
			goto err_minors;
	}

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		ret = drm_modeset_register_all(dev);
		if (ret)
			goto err_unload;
	}

	DRM_INFO("Initialized %s %d.%d.%d %s for %s on minor %d\n",
		 driver->name, driver->major, driver->minor,
		 driver->patchlevel, driver->date,
		 dev->dev ? dev_name(dev->dev) : "virtual device",
		 dev->primary ? dev->primary->index : dev->accel->index);

	goto out_unlock;

err_unload:
	if (dev->driver->unload)
		dev->driver->unload(dev);
err_minors:
	remove_compat_control_link(dev);
	drm_minor_unregister(dev, DRM_MINOR_ACCEL);
	drm_minor_unregister(dev, DRM_MINOR_PRIMARY);
	drm_minor_unregister(dev, DRM_MINOR_RENDER);
out_unlock:
	if (drm_dev_needs_global_mutex(dev))
		mutex_unlock(&drm_global_mutex);
	return ret;
}
EXPORT_SYMBOL(drm_dev_register);

 
void drm_dev_unregister(struct drm_device *dev)
{
	if (drm_core_check_feature(dev, DRIVER_LEGACY))
		drm_lastclose(dev);

	dev->registered = false;

	drm_client_dev_unregister(dev);

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		drm_modeset_unregister_all(dev);

	if (dev->driver->unload)
		dev->driver->unload(dev);

	drm_legacy_pci_agp_destroy(dev);
	drm_legacy_rmmaps(dev);

	remove_compat_control_link(dev);
	drm_minor_unregister(dev, DRM_MINOR_ACCEL);
	drm_minor_unregister(dev, DRM_MINOR_PRIMARY);
	drm_minor_unregister(dev, DRM_MINOR_RENDER);
}
EXPORT_SYMBOL(drm_dev_unregister);

 

static int drm_stub_open(struct inode *inode, struct file *filp)
{
	const struct file_operations *new_fops;
	struct drm_minor *minor;
	int err;

	DRM_DEBUG("\n");

	minor = drm_minor_acquire(iminor(inode));
	if (IS_ERR(minor))
		return PTR_ERR(minor);

	new_fops = fops_get(minor->dev->driver->fops);
	if (!new_fops) {
		err = -ENODEV;
		goto out;
	}

	replace_fops(filp, new_fops);
	if (filp->f_op->open)
		err = filp->f_op->open(inode, filp);
	else
		err = 0;

out:
	drm_minor_release(minor);

	return err;
}

static const struct file_operations drm_stub_fops = {
	.owner = THIS_MODULE,
	.open = drm_stub_open,
	.llseek = noop_llseek,
};

static void drm_core_exit(void)
{
	drm_privacy_screen_lookup_exit();
	accel_core_exit();
	unregister_chrdev(DRM_MAJOR, "drm");
	debugfs_remove(drm_debugfs_root);
	drm_sysfs_destroy();
	idr_destroy(&drm_minors_idr);
	drm_connector_ida_destroy();
}

static int __init drm_core_init(void)
{
	int ret;

	drm_connector_ida_init();
	idr_init(&drm_minors_idr);
	drm_memcpy_init_early();

	ret = drm_sysfs_init();
	if (ret < 0) {
		DRM_ERROR("Cannot create DRM class: %d\n", ret);
		goto error;
	}

	drm_debugfs_root = debugfs_create_dir("dri", NULL);

	ret = register_chrdev(DRM_MAJOR, "drm", &drm_stub_fops);
	if (ret < 0)
		goto error;

	ret = accel_core_init();
	if (ret < 0)
		goto error;

	drm_privacy_screen_lookup_init();

	drm_core_init_complete = true;

	DRM_DEBUG("Initialized\n");
	return 0;

error:
	drm_core_exit();
	return ret;
}

module_init(drm_core_init);
module_exit(drm_core_exit);
