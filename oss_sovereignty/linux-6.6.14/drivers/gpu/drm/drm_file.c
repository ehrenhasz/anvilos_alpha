 

 

#include <linux/anon_inodes.h>
#include <linux/dma-fence.h>
#include <linux/file.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/slab.h>

#include <drm/drm_client.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_print.h>

#include "drm_crtc_internal.h"
#include "drm_internal.h"
#include "drm_legacy.h"

 
DEFINE_MUTEX(drm_global_mutex);

bool drm_dev_needs_global_mutex(struct drm_device *dev)
{
	 
	if (drm_core_check_feature(dev, DRIVER_LEGACY))
		return true;

	 
	if (dev->driver->load || dev->driver->unload)
		return true;

	 
	if (dev->driver->lastclose)
		return true;

	return false;
}

 

 
struct drm_file *drm_file_alloc(struct drm_minor *minor)
{
	static atomic64_t ident = ATOMIC_INIT(0);
	struct drm_device *dev = minor->dev;
	struct drm_file *file;
	int ret;

	file = kzalloc(sizeof(*file), GFP_KERNEL);
	if (!file)
		return ERR_PTR(-ENOMEM);

	 
	file->client_id = atomic64_inc_return(&ident);
	rcu_assign_pointer(file->pid, get_pid(task_tgid(current)));
	file->minor = minor;

	 
	file->authenticated = capable(CAP_SYS_ADMIN);

	INIT_LIST_HEAD(&file->lhead);
	INIT_LIST_HEAD(&file->fbs);
	mutex_init(&file->fbs_lock);
	INIT_LIST_HEAD(&file->blobs);
	INIT_LIST_HEAD(&file->pending_event_list);
	INIT_LIST_HEAD(&file->event_list);
	init_waitqueue_head(&file->event_wait);
	file->event_space = 4096;  

	spin_lock_init(&file->master_lookup_lock);
	mutex_init(&file->event_read_lock);

	if (drm_core_check_feature(dev, DRIVER_GEM))
		drm_gem_open(dev, file);

	if (drm_core_check_feature(dev, DRIVER_SYNCOBJ))
		drm_syncobj_open(file);

	drm_prime_init_file_private(&file->prime);

	if (dev->driver->open) {
		ret = dev->driver->open(dev, file);
		if (ret < 0)
			goto out_prime_destroy;
	}

	return file;

out_prime_destroy:
	drm_prime_destroy_file_private(&file->prime);
	if (drm_core_check_feature(dev, DRIVER_SYNCOBJ))
		drm_syncobj_release(file);
	if (drm_core_check_feature(dev, DRIVER_GEM))
		drm_gem_release(dev, file);
	put_pid(rcu_access_pointer(file->pid));
	kfree(file);

	return ERR_PTR(ret);
}

static void drm_events_release(struct drm_file *file_priv)
{
	struct drm_device *dev = file_priv->minor->dev;
	struct drm_pending_event *e, *et;
	unsigned long flags;

	spin_lock_irqsave(&dev->event_lock, flags);

	 
	list_for_each_entry_safe(e, et, &file_priv->pending_event_list,
				 pending_link) {
		list_del(&e->pending_link);
		e->file_priv = NULL;
	}

	 
	list_for_each_entry_safe(e, et, &file_priv->event_list, link) {
		list_del(&e->link);
		kfree(e);
	}

	spin_unlock_irqrestore(&dev->event_lock, flags);
}

 
void drm_file_free(struct drm_file *file)
{
	struct drm_device *dev;

	if (!file)
		return;

	dev = file->minor->dev;

	drm_dbg_core(dev, "comm=\"%s\", pid=%d, dev=0x%lx, open_count=%d\n",
		     current->comm, task_pid_nr(current),
		     (long)old_encode_dev(file->minor->kdev->devt),
		     atomic_read(&dev->open_count));

#ifdef CONFIG_DRM_LEGACY
	if (drm_core_check_feature(dev, DRIVER_LEGACY) &&
	    dev->driver->preclose)
		dev->driver->preclose(dev, file);
#endif

	if (drm_core_check_feature(dev, DRIVER_LEGACY))
		drm_legacy_lock_release(dev, file->filp);

	if (drm_core_check_feature(dev, DRIVER_HAVE_DMA))
		drm_legacy_reclaim_buffers(dev, file);

	drm_events_release(file);

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		drm_fb_release(file);
		drm_property_destroy_user_blobs(dev, file);
	}

	if (drm_core_check_feature(dev, DRIVER_SYNCOBJ))
		drm_syncobj_release(file);

	if (drm_core_check_feature(dev, DRIVER_GEM))
		drm_gem_release(dev, file);

	drm_legacy_ctxbitmap_flush(dev, file);

	if (drm_is_primary_client(file))
		drm_master_release(file);

	if (dev->driver->postclose)
		dev->driver->postclose(dev, file);

	drm_prime_destroy_file_private(&file->prime);

	WARN_ON(!list_empty(&file->event_list));

	put_pid(rcu_access_pointer(file->pid));
	kfree(file);
}

static void drm_close_helper(struct file *filp)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_device *dev = file_priv->minor->dev;

	mutex_lock(&dev->filelist_mutex);
	list_del(&file_priv->lhead);
	mutex_unlock(&dev->filelist_mutex);

	drm_file_free(file_priv);
}

 
static int drm_cpu_valid(void)
{
#if defined(__sparc__) && !defined(__sparc_v9__)
	return 0;		 
#endif
	return 1;
}

 
int drm_open_helper(struct file *filp, struct drm_minor *minor)
{
	struct drm_device *dev = minor->dev;
	struct drm_file *priv;
	int ret;

	if (filp->f_flags & O_EXCL)
		return -EBUSY;	 
	if (!drm_cpu_valid())
		return -EINVAL;
	if (dev->switch_power_state != DRM_SWITCH_POWER_ON &&
	    dev->switch_power_state != DRM_SWITCH_POWER_DYNAMIC_OFF)
		return -EINVAL;

	drm_dbg_core(dev, "comm=\"%s\", pid=%d, minor=%d\n",
		     current->comm, task_pid_nr(current), minor->index);

	priv = drm_file_alloc(minor);
	if (IS_ERR(priv))
		return PTR_ERR(priv);

	if (drm_is_primary_client(priv)) {
		ret = drm_master_open(priv);
		if (ret) {
			drm_file_free(priv);
			return ret;
		}
	}

	filp->private_data = priv;
	filp->f_mode |= FMODE_UNSIGNED_OFFSET;
	priv->filp = filp;

	mutex_lock(&dev->filelist_mutex);
	list_add(&priv->lhead, &dev->filelist);
	mutex_unlock(&dev->filelist_mutex);

#ifdef CONFIG_DRM_LEGACY
#ifdef __alpha__
	 
	if (!dev->hose) {
		struct pci_dev *pci_dev;

		pci_dev = pci_get_class(PCI_CLASS_DISPLAY_VGA << 8, NULL);
		if (pci_dev) {
			dev->hose = pci_dev->sysdata;
			pci_dev_put(pci_dev);
		}
		if (!dev->hose) {
			struct pci_bus *b = list_entry(pci_root_buses.next,
				struct pci_bus, node);
			if (b)
				dev->hose = b->sysdata;
		}
	}
#endif
#endif

	return 0;
}

 
int drm_open(struct inode *inode, struct file *filp)
{
	struct drm_device *dev;
	struct drm_minor *minor;
	int retcode;
	int need_setup = 0;

	minor = drm_minor_acquire(iminor(inode));
	if (IS_ERR(minor))
		return PTR_ERR(minor);

	dev = minor->dev;
	if (drm_dev_needs_global_mutex(dev))
		mutex_lock(&drm_global_mutex);

	if (!atomic_fetch_inc(&dev->open_count))
		need_setup = 1;

	 
	filp->f_mapping = dev->anon_inode->i_mapping;

	retcode = drm_open_helper(filp, minor);
	if (retcode)
		goto err_undo;
	if (need_setup) {
		retcode = drm_legacy_setup(dev);
		if (retcode) {
			drm_close_helper(filp);
			goto err_undo;
		}
	}

	if (drm_dev_needs_global_mutex(dev))
		mutex_unlock(&drm_global_mutex);

	return 0;

err_undo:
	atomic_dec(&dev->open_count);
	if (drm_dev_needs_global_mutex(dev))
		mutex_unlock(&drm_global_mutex);
	drm_minor_release(minor);
	return retcode;
}
EXPORT_SYMBOL(drm_open);

void drm_lastclose(struct drm_device * dev)
{
	drm_dbg_core(dev, "\n");

	if (dev->driver->lastclose)
		dev->driver->lastclose(dev);
	drm_dbg_core(dev, "driver lastclose completed\n");

	if (drm_core_check_feature(dev, DRIVER_LEGACY))
		drm_legacy_dev_reinit(dev);

	drm_client_dev_restore(dev);
}

 
int drm_release(struct inode *inode, struct file *filp)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_minor *minor = file_priv->minor;
	struct drm_device *dev = minor->dev;

	if (drm_dev_needs_global_mutex(dev))
		mutex_lock(&drm_global_mutex);

	drm_dbg_core(dev, "open_count = %d\n", atomic_read(&dev->open_count));

	drm_close_helper(filp);

	if (atomic_dec_and_test(&dev->open_count))
		drm_lastclose(dev);

	if (drm_dev_needs_global_mutex(dev))
		mutex_unlock(&drm_global_mutex);

	drm_minor_release(minor);

	return 0;
}
EXPORT_SYMBOL(drm_release);

void drm_file_update_pid(struct drm_file *filp)
{
	struct drm_device *dev;
	struct pid *pid, *old;

	 
	if (filp->was_master)
		return;

	pid = task_tgid(current);

	 
	if (pid == rcu_access_pointer(filp->pid))
		return;

	dev = filp->minor->dev;
	mutex_lock(&dev->filelist_mutex);
	old = rcu_replace_pointer(filp->pid, pid, 1);
	mutex_unlock(&dev->filelist_mutex);

	if (pid != old) {
		get_pid(pid);
		synchronize_rcu();
		put_pid(old);
	}
}

 
int drm_release_noglobal(struct inode *inode, struct file *filp)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_minor *minor = file_priv->minor;
	struct drm_device *dev = minor->dev;

	drm_close_helper(filp);

	if (atomic_dec_and_mutex_lock(&dev->open_count, &drm_global_mutex)) {
		drm_lastclose(dev);
		mutex_unlock(&drm_global_mutex);
	}

	drm_minor_release(minor);

	return 0;
}
EXPORT_SYMBOL(drm_release_noglobal);

 
ssize_t drm_read(struct file *filp, char __user *buffer,
		 size_t count, loff_t *offset)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_device *dev = file_priv->minor->dev;
	ssize_t ret;

	ret = mutex_lock_interruptible(&file_priv->event_read_lock);
	if (ret)
		return ret;

	for (;;) {
		struct drm_pending_event *e = NULL;

		spin_lock_irq(&dev->event_lock);
		if (!list_empty(&file_priv->event_list)) {
			e = list_first_entry(&file_priv->event_list,
					struct drm_pending_event, link);
			file_priv->event_space += e->event->length;
			list_del(&e->link);
		}
		spin_unlock_irq(&dev->event_lock);

		if (e == NULL) {
			if (ret)
				break;

			if (filp->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				break;
			}

			mutex_unlock(&file_priv->event_read_lock);
			ret = wait_event_interruptible(file_priv->event_wait,
						       !list_empty(&file_priv->event_list));
			if (ret >= 0)
				ret = mutex_lock_interruptible(&file_priv->event_read_lock);
			if (ret)
				return ret;
		} else {
			unsigned length = e->event->length;

			if (length > count - ret) {
put_back_event:
				spin_lock_irq(&dev->event_lock);
				file_priv->event_space -= length;
				list_add(&e->link, &file_priv->event_list);
				spin_unlock_irq(&dev->event_lock);
				wake_up_interruptible_poll(&file_priv->event_wait,
					EPOLLIN | EPOLLRDNORM);
				break;
			}

			if (copy_to_user(buffer + ret, e->event, length)) {
				if (ret == 0)
					ret = -EFAULT;
				goto put_back_event;
			}

			ret += length;
			kfree(e);
		}
	}
	mutex_unlock(&file_priv->event_read_lock);

	return ret;
}
EXPORT_SYMBOL(drm_read);

 
__poll_t drm_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct drm_file *file_priv = filp->private_data;
	__poll_t mask = 0;

	poll_wait(filp, &file_priv->event_wait, wait);

	if (!list_empty(&file_priv->event_list))
		mask |= EPOLLIN | EPOLLRDNORM;

	return mask;
}
EXPORT_SYMBOL(drm_poll);

 
int drm_event_reserve_init_locked(struct drm_device *dev,
				  struct drm_file *file_priv,
				  struct drm_pending_event *p,
				  struct drm_event *e)
{
	if (file_priv->event_space < e->length)
		return -ENOMEM;

	file_priv->event_space -= e->length;

	p->event = e;
	list_add(&p->pending_link, &file_priv->pending_event_list);
	p->file_priv = file_priv;

	return 0;
}
EXPORT_SYMBOL(drm_event_reserve_init_locked);

 
int drm_event_reserve_init(struct drm_device *dev,
			   struct drm_file *file_priv,
			   struct drm_pending_event *p,
			   struct drm_event *e)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&dev->event_lock, flags);
	ret = drm_event_reserve_init_locked(dev, file_priv, p, e);
	spin_unlock_irqrestore(&dev->event_lock, flags);

	return ret;
}
EXPORT_SYMBOL(drm_event_reserve_init);

 
void drm_event_cancel_free(struct drm_device *dev,
			   struct drm_pending_event *p)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->event_lock, flags);
	if (p->file_priv) {
		p->file_priv->event_space += p->event->length;
		list_del(&p->pending_link);
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);

	if (p->fence)
		dma_fence_put(p->fence);

	kfree(p);
}
EXPORT_SYMBOL(drm_event_cancel_free);

static void drm_send_event_helper(struct drm_device *dev,
			   struct drm_pending_event *e, ktime_t timestamp)
{
	assert_spin_locked(&dev->event_lock);

	if (e->completion) {
		complete_all(e->completion);
		e->completion_release(e->completion);
		e->completion = NULL;
	}

	if (e->fence) {
		if (timestamp)
			dma_fence_signal_timestamp(e->fence, timestamp);
		else
			dma_fence_signal(e->fence);
		dma_fence_put(e->fence);
	}

	if (!e->file_priv) {
		kfree(e);
		return;
	}

	list_del(&e->pending_link);
	list_add_tail(&e->link,
		      &e->file_priv->event_list);
	wake_up_interruptible_poll(&e->file_priv->event_wait,
		EPOLLIN | EPOLLRDNORM);
}

 
void drm_send_event_timestamp_locked(struct drm_device *dev,
				     struct drm_pending_event *e, ktime_t timestamp)
{
	drm_send_event_helper(dev, e, timestamp);
}
EXPORT_SYMBOL(drm_send_event_timestamp_locked);

 
void drm_send_event_locked(struct drm_device *dev, struct drm_pending_event *e)
{
	drm_send_event_helper(dev, e, 0);
}
EXPORT_SYMBOL(drm_send_event_locked);

 
void drm_send_event(struct drm_device *dev, struct drm_pending_event *e)
{
	unsigned long irqflags;

	spin_lock_irqsave(&dev->event_lock, irqflags);
	drm_send_event_helper(dev, e, 0);
	spin_unlock_irqrestore(&dev->event_lock, irqflags);
}
EXPORT_SYMBOL(drm_send_event);

static void print_size(struct drm_printer *p, const char *stat,
		       const char *region, u64 sz)
{
	const char *units[] = {"", " KiB", " MiB"};
	unsigned u;

	for (u = 0; u < ARRAY_SIZE(units) - 1; u++) {
		if (sz < SZ_1K)
			break;
		sz = div_u64(sz, SZ_1K);
	}

	drm_printf(p, "drm-%s-%s:\t%llu%s\n", stat, region, sz, units[u]);
}

 
void drm_print_memory_stats(struct drm_printer *p,
			    const struct drm_memory_stats *stats,
			    enum drm_gem_object_status supported_status,
			    const char *region)
{
	print_size(p, "total", region, stats->private + stats->shared);
	print_size(p, "shared", region, stats->shared);
	print_size(p, "active", region, stats->active);

	if (supported_status & DRM_GEM_OBJECT_RESIDENT)
		print_size(p, "resident", region, stats->resident);

	if (supported_status & DRM_GEM_OBJECT_PURGEABLE)
		print_size(p, "purgeable", region, stats->purgeable);
}
EXPORT_SYMBOL(drm_print_memory_stats);

 
void drm_show_memory_stats(struct drm_printer *p, struct drm_file *file)
{
	struct drm_gem_object *obj;
	struct drm_memory_stats status = {};
	enum drm_gem_object_status supported_status;
	int id;

	spin_lock(&file->table_lock);
	idr_for_each_entry (&file->object_idr, obj, id) {
		enum drm_gem_object_status s = 0;

		if (obj->funcs && obj->funcs->status) {
			s = obj->funcs->status(obj);
			supported_status = DRM_GEM_OBJECT_RESIDENT |
					DRM_GEM_OBJECT_PURGEABLE;
		}

		if (obj->handle_count > 1) {
			status.shared += obj->size;
		} else {
			status.private += obj->size;
		}

		if (s & DRM_GEM_OBJECT_RESIDENT) {
			status.resident += obj->size;
		} else {
			 
			s &= ~DRM_GEM_OBJECT_PURGEABLE;
		}

		if (!dma_resv_test_signaled(obj->resv, dma_resv_usage_rw(true))) {
			status.active += obj->size;

			 
			s &= ~DRM_GEM_OBJECT_PURGEABLE;
		}

		if (s & DRM_GEM_OBJECT_PURGEABLE)
			status.purgeable += obj->size;
	}
	spin_unlock(&file->table_lock);

	drm_print_memory_stats(p, &status, supported_status, "memory");
}
EXPORT_SYMBOL(drm_show_memory_stats);

 
void drm_show_fdinfo(struct seq_file *m, struct file *f)
{
	struct drm_file *file = f->private_data;
	struct drm_device *dev = file->minor->dev;
	struct drm_printer p = drm_seq_file_printer(m);

	drm_printf(&p, "drm-driver:\t%s\n", dev->driver->name);
	drm_printf(&p, "drm-client-id:\t%llu\n", file->client_id);

	if (dev_is_pci(dev->dev)) {
		struct pci_dev *pdev = to_pci_dev(dev->dev);

		drm_printf(&p, "drm-pdev:\t%04x:%02x:%02x.%d\n",
			   pci_domain_nr(pdev->bus), pdev->bus->number,
			   PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
	}

	if (dev->driver->show_fdinfo)
		dev->driver->show_fdinfo(&p, file);
}
EXPORT_SYMBOL(drm_show_fdinfo);

 
struct file *mock_drm_getfile(struct drm_minor *minor, unsigned int flags)
{
	struct drm_device *dev = minor->dev;
	struct drm_file *priv;
	struct file *file;

	priv = drm_file_alloc(minor);
	if (IS_ERR(priv))
		return ERR_CAST(priv);

	file = anon_inode_getfile("drm", dev->driver->fops, priv, flags);
	if (IS_ERR(file)) {
		drm_file_free(priv);
		return file;
	}

	 
	file->f_mapping = dev->anon_inode->i_mapping;

	drm_dev_get(dev);
	priv->filp = file;

	return file;
}
EXPORT_SYMBOL_FOR_TESTS_ONLY(mock_drm_getfile);
