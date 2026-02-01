 

#include <linux/export.h>
#include <linux/nospec.h>
#include <linux/pci.h>
#include <linux/uaccess.h>

#include <drm/drm_auth.h>
#include <drm/drm_crtc.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_print.h>

#include "drm_crtc_internal.h"
#include "drm_internal.h"
#include "drm_legacy.h"

 

 
int drm_getunique(struct drm_device *dev, void *data,
		  struct drm_file *file_priv)
{
	struct drm_unique *u = data;
	struct drm_master *master;

	mutex_lock(&dev->master_mutex);
	master = file_priv->master;
	if (u->unique_len >= master->unique_len) {
		if (copy_to_user(u->unique, master->unique, master->unique_len)) {
			mutex_unlock(&dev->master_mutex);
			return -EFAULT;
		}
	}
	u->unique_len = master->unique_len;
	mutex_unlock(&dev->master_mutex);

	return 0;
}

static void
drm_unset_busid(struct drm_device *dev,
		struct drm_master *master)
{
	kfree(master->unique);
	master->unique = NULL;
	master->unique_len = 0;
}

static int drm_set_busid(struct drm_device *dev, struct drm_file *file_priv)
{
	struct drm_master *master = file_priv->master;
	int ret;

	if (master->unique != NULL)
		drm_unset_busid(dev, master);

	if (dev->dev && dev_is_pci(dev->dev)) {
		ret = drm_pci_set_busid(dev, master);
		if (ret) {
			drm_unset_busid(dev, master);
			return ret;
		}
	} else {
		WARN_ON(!dev->unique);
		master->unique = kstrdup(dev->unique, GFP_KERNEL);
		if (master->unique)
			master->unique_len = strlen(dev->unique);
	}

	return 0;
}

 
int drm_getclient(struct drm_device *dev, void *data,
		  struct drm_file *file_priv)
{
	struct drm_client *client = data;

	 
	if (client->idx == 0) {
		client->auth = file_priv->authenticated;
		client->pid = task_pid_vnr(current);
		client->uid = overflowuid;
		client->magic = 0;
		client->iocs = 0;

		return 0;
	} else {
		return -EINVAL;
	}
}

 
static int drm_getstats(struct drm_device *dev, void *data,
		 struct drm_file *file_priv)
{
	struct drm_stats *stats = data;

	 
	memset(stats, 0, sizeof(*stats));

	return 0;
}

 
static int drm_getcap(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_get_cap *req = data;
	struct drm_crtc *crtc;

	req->value = 0;

	 
	switch (req->capability) {
	case DRM_CAP_TIMESTAMP_MONOTONIC:
		req->value = 1;
		return 0;
	case DRM_CAP_PRIME:
		req->value = DRM_PRIME_CAP_IMPORT | DRM_PRIME_CAP_EXPORT;
		return 0;
	case DRM_CAP_SYNCOBJ:
		req->value = drm_core_check_feature(dev, DRIVER_SYNCOBJ);
		return 0;
	case DRM_CAP_SYNCOBJ_TIMELINE:
		req->value = drm_core_check_feature(dev, DRIVER_SYNCOBJ_TIMELINE);
		return 0;
	}

	 
	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EOPNOTSUPP;

	switch (req->capability) {
	case DRM_CAP_DUMB_BUFFER:
		if (dev->driver->dumb_create)
			req->value = 1;
		break;
	case DRM_CAP_VBLANK_HIGH_CRTC:
		req->value = 1;
		break;
	case DRM_CAP_DUMB_PREFERRED_DEPTH:
		req->value = dev->mode_config.preferred_depth;
		break;
	case DRM_CAP_DUMB_PREFER_SHADOW:
		req->value = dev->mode_config.prefer_shadow;
		break;
	case DRM_CAP_ASYNC_PAGE_FLIP:
		req->value = dev->mode_config.async_page_flip;
		break;
	case DRM_CAP_PAGE_FLIP_TARGET:
		req->value = 1;
		drm_for_each_crtc(crtc, dev) {
			if (!crtc->funcs->page_flip_target)
				req->value = 0;
		}
		break;
	case DRM_CAP_CURSOR_WIDTH:
		if (dev->mode_config.cursor_width)
			req->value = dev->mode_config.cursor_width;
		else
			req->value = 64;
		break;
	case DRM_CAP_CURSOR_HEIGHT:
		if (dev->mode_config.cursor_height)
			req->value = dev->mode_config.cursor_height;
		else
			req->value = 64;
		break;
	case DRM_CAP_ADDFB2_MODIFIERS:
		req->value = !dev->mode_config.fb_modifiers_not_supported;
		break;
	case DRM_CAP_CRTC_IN_VBLANK_EVENT:
		req->value = 1;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

 
static int
drm_setclientcap(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_set_client_cap *req = data;

	 

	 
	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EOPNOTSUPP;

	switch (req->capability) {
	case DRM_CLIENT_CAP_STEREO_3D:
		if (req->value > 1)
			return -EINVAL;
		file_priv->stereo_allowed = req->value;
		break;
	case DRM_CLIENT_CAP_UNIVERSAL_PLANES:
		if (req->value > 1)
			return -EINVAL;
		file_priv->universal_planes = req->value;
		break;
	case DRM_CLIENT_CAP_ATOMIC:
		if (!drm_core_check_feature(dev, DRIVER_ATOMIC))
			return -EOPNOTSUPP;
		 
		if (current->comm[0] == 'X' && req->value == 1) {
			pr_info("broken atomic modeset userspace detected, disabling atomic\n");
			return -EOPNOTSUPP;
		}
		if (req->value > 2)
			return -EINVAL;
		file_priv->atomic = req->value;
		file_priv->universal_planes = req->value;
		 
		file_priv->aspect_ratio_allowed = req->value;
		break;
	case DRM_CLIENT_CAP_ASPECT_RATIO:
		if (req->value > 1)
			return -EINVAL;
		file_priv->aspect_ratio_allowed = req->value;
		break;
	case DRM_CLIENT_CAP_WRITEBACK_CONNECTORS:
		if (!file_priv->atomic)
			return -EINVAL;
		if (req->value > 1)
			return -EINVAL;
		file_priv->writeback_connectors = req->value;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

 
static int drm_setversion(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_set_version *sv = data;
	int if_version, retcode = 0;

	mutex_lock(&dev->master_mutex);
	if (sv->drm_di_major != -1) {
		if (sv->drm_di_major != DRM_IF_MAJOR ||
		    sv->drm_di_minor < 0 || sv->drm_di_minor > DRM_IF_MINOR) {
			retcode = -EINVAL;
			goto done;
		}
		if_version = DRM_IF_VERSION(sv->drm_di_major,
					    sv->drm_di_minor);
		dev->if_version = max(if_version, dev->if_version);
		if (sv->drm_di_minor >= 1) {
			 
			retcode = drm_set_busid(dev, file_priv);
			if (retcode)
				goto done;
		}
	}

	if (sv->drm_dd_major != -1) {
		if (sv->drm_dd_major != dev->driver->major ||
		    sv->drm_dd_minor < 0 || sv->drm_dd_minor >
		    dev->driver->minor) {
			retcode = -EINVAL;
			goto done;
		}
	}

done:
	sv->drm_di_major = DRM_IF_MAJOR;
	sv->drm_di_minor = DRM_IF_MINOR;
	sv->drm_dd_major = dev->driver->major;
	sv->drm_dd_minor = dev->driver->minor;
	mutex_unlock(&dev->master_mutex);

	return retcode;
}

 
int drm_noop(struct drm_device *dev, void *data,
	     struct drm_file *file_priv)
{
	drm_dbg_core(dev, "\n");
	return 0;
}
EXPORT_SYMBOL(drm_noop);

 
int drm_invalid_op(struct drm_device *dev, void *data,
		   struct drm_file *file_priv)
{
	return -EINVAL;
}
EXPORT_SYMBOL(drm_invalid_op);

 
static int drm_copy_field(char __user *buf, size_t *buf_len, const char *value)
{
	size_t len;

	 
	if (WARN_ONCE(!value, "BUG: the value to copy was not set!")) {
		*buf_len = 0;
		return 0;
	}

	 
	len = strlen(value);
	if (len > *buf_len)
		len = *buf_len;

	 
	*buf_len = strlen(value);

	 
	if (len && buf)
		if (copy_to_user(buf, value, len))
			return -EFAULT;
	return 0;
}

 
int drm_version(struct drm_device *dev, void *data,
		       struct drm_file *file_priv)
{
	struct drm_version *version = data;
	int err;

	version->version_major = dev->driver->major;
	version->version_minor = dev->driver->minor;
	version->version_patchlevel = dev->driver->patchlevel;
	err = drm_copy_field(version->name, &version->name_len,
			dev->driver->name);
	if (!err)
		err = drm_copy_field(version->date, &version->date_len,
				dev->driver->date);
	if (!err)
		err = drm_copy_field(version->desc, &version->desc_len,
				dev->driver->desc);

	return err;
}

static int drm_ioctl_permit(u32 flags, struct drm_file *file_priv)
{
	 
	if (unlikely((flags & DRM_ROOT_ONLY) && !capable(CAP_SYS_ADMIN)))
		return -EACCES;

	 
	if (unlikely((flags & DRM_AUTH) && !drm_is_render_client(file_priv) &&
		     !file_priv->authenticated))
		return -EACCES;

	 
	if (unlikely((flags & DRM_MASTER) &&
		     !drm_is_current_master(file_priv)))
		return -EACCES;

	 
	if (unlikely(!(flags & DRM_RENDER_ALLOW) &&
		     drm_is_render_client(file_priv)))
		return -EACCES;

	return 0;
}

#define DRM_IOCTL_DEF(ioctl, _func, _flags)	\
	[DRM_IOCTL_NR(ioctl)] = {		\
		.cmd = ioctl,			\
		.func = _func,			\
		.flags = _flags,		\
		.name = #ioctl			\
	}

#if IS_ENABLED(CONFIG_DRM_LEGACY)
#define DRM_LEGACY_IOCTL_DEF(ioctl, _func, _flags)  DRM_IOCTL_DEF(ioctl, _func, _flags)
#else
#define DRM_LEGACY_IOCTL_DEF(ioctl, _func, _flags) DRM_IOCTL_DEF(ioctl, drm_invalid_op, _flags)
#endif

 
static const struct drm_ioctl_desc drm_ioctls[] = {
	DRM_IOCTL_DEF(DRM_IOCTL_VERSION, drm_version, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_GET_UNIQUE, drm_getunique, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_GET_MAGIC, drm_getmagic, 0),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_IRQ_BUSID, drm_legacy_irq_by_busid,
			     DRM_MASTER|DRM_ROOT_ONLY),

	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_GET_MAP, drm_legacy_getmap_ioctl, 0),

	DRM_IOCTL_DEF(DRM_IOCTL_GET_CLIENT, drm_getclient, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_GET_STATS, drm_getstats, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_GET_CAP, drm_getcap, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_SET_CLIENT_CAP, drm_setclientcap, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_SET_VERSION, drm_setversion, DRM_MASTER),

	DRM_IOCTL_DEF(DRM_IOCTL_SET_UNIQUE, drm_invalid_op, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_BLOCK, drm_noop, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_UNBLOCK, drm_noop, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_AUTH_MAGIC, drm_authmagic, DRM_MASTER),

	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_ADD_MAP, drm_legacy_addmap_ioctl, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_RM_MAP, drm_legacy_rmmap_ioctl, DRM_AUTH),

	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_SET_SAREA_CTX, drm_legacy_setsareactx, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_GET_SAREA_CTX, drm_legacy_getsareactx, DRM_AUTH),

	DRM_IOCTL_DEF(DRM_IOCTL_SET_MASTER, drm_setmaster_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_DROP_MASTER, drm_dropmaster_ioctl, 0),

	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_ADD_CTX, drm_legacy_addctx, DRM_AUTH|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_RM_CTX, drm_legacy_rmctx, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_MOD_CTX, drm_noop, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_GET_CTX, drm_legacy_getctx, DRM_AUTH),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_SWITCH_CTX, drm_legacy_switchctx, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_NEW_CTX, drm_legacy_newctx, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_RES_CTX, drm_legacy_resctx, DRM_AUTH),

	DRM_IOCTL_DEF(DRM_IOCTL_ADD_DRAW, drm_noop, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_IOCTL_RM_DRAW, drm_noop, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),

	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_LOCK, drm_legacy_lock, DRM_AUTH),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_UNLOCK, drm_legacy_unlock, DRM_AUTH),

	DRM_IOCTL_DEF(DRM_IOCTL_FINISH, drm_noop, DRM_AUTH),

	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_ADD_BUFS, drm_legacy_addbufs, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_MARK_BUFS, drm_legacy_markbufs, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_INFO_BUFS, drm_legacy_infobufs, DRM_AUTH),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_MAP_BUFS, drm_legacy_mapbufs, DRM_AUTH),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_FREE_BUFS, drm_legacy_freebufs, DRM_AUTH),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_DMA, drm_legacy_dma_ioctl, DRM_AUTH),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_CONTROL, drm_legacy_irq_control, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),

#if IS_ENABLED(CONFIG_AGP)
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_AGP_ACQUIRE, drm_legacy_agp_acquire_ioctl,
			     DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_AGP_RELEASE, drm_legacy_agp_release_ioctl,
			     DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_AGP_ENABLE, drm_legacy_agp_enable_ioctl,
			     DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_AGP_INFO, drm_legacy_agp_info_ioctl, DRM_AUTH),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_AGP_ALLOC, drm_legacy_agp_alloc_ioctl,
			     DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_AGP_FREE, drm_legacy_agp_free_ioctl,
			     DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_AGP_BIND, drm_legacy_agp_bind_ioctl,
			     DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_AGP_UNBIND, drm_legacy_agp_unbind_ioctl,
			     DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
#endif

	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_SG_ALLOC, drm_legacy_sg_alloc, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_LEGACY_IOCTL_DEF(DRM_IOCTL_SG_FREE, drm_legacy_sg_free, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),

	DRM_IOCTL_DEF(DRM_IOCTL_WAIT_VBLANK, drm_wait_vblank_ioctl, DRM_UNLOCKED),

	DRM_IOCTL_DEF(DRM_IOCTL_MODESET_CTL, drm_legacy_modeset_ctl_ioctl, 0),

	DRM_IOCTL_DEF(DRM_IOCTL_UPDATE_DRAW, drm_noop, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),

	DRM_IOCTL_DEF(DRM_IOCTL_GEM_CLOSE, drm_gem_close_ioctl, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_GEM_FLINK, drm_gem_flink_ioctl, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_IOCTL_GEM_OPEN, drm_gem_open_ioctl, DRM_AUTH),

	DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETRESOURCES, drm_mode_getresources, 0),

	DRM_IOCTL_DEF(DRM_IOCTL_PRIME_HANDLE_TO_FD, drm_prime_handle_to_fd_ioctl, DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_PRIME_FD_TO_HANDLE, drm_prime_fd_to_handle_ioctl, DRM_RENDER_ALLOW),

	DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETPLANERESOURCES, drm_mode_getplane_res, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETCRTC, drm_mode_getcrtc, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_SETCRTC, drm_mode_setcrtc, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETPLANE, drm_mode_getplane, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_SETPLANE, drm_mode_setplane, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_CURSOR, drm_mode_cursor_ioctl, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETGAMMA, drm_mode_gamma_get_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_SETGAMMA, drm_mode_gamma_set_ioctl, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETENCODER, drm_mode_getencoder, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETCONNECTOR, drm_mode_getconnector, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_ATTACHMODE, drm_noop, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_DETACHMODE, drm_noop, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETPROPERTY, drm_mode_getproperty_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_SETPROPERTY, drm_connector_property_set_ioctl, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETPROPBLOB, drm_mode_getblob_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETFB, drm_mode_getfb, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_GETFB2, drm_mode_getfb2_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_ADDFB, drm_mode_addfb_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_ADDFB2, drm_mode_addfb2_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_RMFB, drm_mode_rmfb_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_PAGE_FLIP, drm_mode_page_flip_ioctl, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_DIRTYFB, drm_mode_dirtyfb_ioctl, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_CREATE_DUMB, drm_mode_create_dumb_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_MAP_DUMB, drm_mode_mmap_dumb_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_DESTROY_DUMB, drm_mode_destroy_dumb_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_OBJ_GETPROPERTIES, drm_mode_obj_get_properties_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_OBJ_SETPROPERTY, drm_mode_obj_set_property_ioctl, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_CURSOR2, drm_mode_cursor2_ioctl, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_ATOMIC, drm_mode_atomic_ioctl, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_CREATEPROPBLOB, drm_mode_createblob_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_DESTROYPROPBLOB, drm_mode_destroyblob_ioctl, 0),

	DRM_IOCTL_DEF(DRM_IOCTL_SYNCOBJ_CREATE, drm_syncobj_create_ioctl,
		      DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_SYNCOBJ_DESTROY, drm_syncobj_destroy_ioctl,
		      DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD, drm_syncobj_handle_to_fd_ioctl,
		      DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE, drm_syncobj_fd_to_handle_ioctl,
		      DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_SYNCOBJ_TRANSFER, drm_syncobj_transfer_ioctl,
		      DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_SYNCOBJ_WAIT, drm_syncobj_wait_ioctl,
		      DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT, drm_syncobj_timeline_wait_ioctl,
		      DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_SYNCOBJ_EVENTFD, drm_syncobj_eventfd_ioctl,
		      DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_SYNCOBJ_RESET, drm_syncobj_reset_ioctl,
		      DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_SYNCOBJ_SIGNAL, drm_syncobj_signal_ioctl,
		      DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL, drm_syncobj_timeline_signal_ioctl,
		      DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_SYNCOBJ_QUERY, drm_syncobj_query_ioctl,
		      DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF(DRM_IOCTL_CRTC_GET_SEQUENCE, drm_crtc_get_sequence_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_CRTC_QUEUE_SEQUENCE, drm_crtc_queue_sequence_ioctl, 0),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_CREATE_LEASE, drm_mode_create_lease_ioctl, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_LIST_LESSEES, drm_mode_list_lessees_ioctl, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_GET_LEASE, drm_mode_get_lease_ioctl, DRM_MASTER),
	DRM_IOCTL_DEF(DRM_IOCTL_MODE_REVOKE_LEASE, drm_mode_revoke_lease_ioctl, DRM_MASTER),
};

#define DRM_CORE_IOCTL_COUNT	ARRAY_SIZE(drm_ioctls)

 

long drm_ioctl_kernel(struct file *file, drm_ioctl_t *func, void *kdata,
		      u32 flags)
{
	struct drm_file *file_priv = file->private_data;
	struct drm_device *dev = file_priv->minor->dev;
	int retcode;

	 
	drm_file_update_pid(file_priv);

	if (drm_dev_is_unplugged(dev))
		return -ENODEV;

	retcode = drm_ioctl_permit(flags, file_priv);
	if (unlikely(retcode))
		return retcode;

	 
	if (likely(!drm_core_check_feature(dev, DRIVER_LEGACY)) ||
	    (flags & DRM_UNLOCKED))
		retcode = func(dev, kdata, file_priv);
	else {
		mutex_lock(&drm_global_mutex);
		retcode = func(dev, kdata, file_priv);
		mutex_unlock(&drm_global_mutex);
	}
	return retcode;
}
EXPORT_SYMBOL(drm_ioctl_kernel);

 
long drm_ioctl(struct file *filp,
	      unsigned int cmd, unsigned long arg)
{
	struct drm_file *file_priv = filp->private_data;
	struct drm_device *dev;
	const struct drm_ioctl_desc *ioctl = NULL;
	drm_ioctl_t *func;
	unsigned int nr = DRM_IOCTL_NR(cmd);
	int retcode = -EINVAL;
	char stack_kdata[128];
	char *kdata = NULL;
	unsigned int in_size, out_size, drv_size, ksize;
	bool is_driver_ioctl;

	dev = file_priv->minor->dev;

	if (drm_dev_is_unplugged(dev))
		return -ENODEV;

	if (DRM_IOCTL_TYPE(cmd) != DRM_IOCTL_BASE)
		return -ENOTTY;

	is_driver_ioctl = nr >= DRM_COMMAND_BASE && nr < DRM_COMMAND_END;

	if (is_driver_ioctl) {
		 
		unsigned int index = nr - DRM_COMMAND_BASE;

		if (index >= dev->driver->num_ioctls)
			goto err_i1;
		index = array_index_nospec(index, dev->driver->num_ioctls);
		ioctl = &dev->driver->ioctls[index];
	} else {
		 
		if (nr >= DRM_CORE_IOCTL_COUNT)
			goto err_i1;
		nr = array_index_nospec(nr, DRM_CORE_IOCTL_COUNT);
		ioctl = &drm_ioctls[nr];
	}

	drv_size = _IOC_SIZE(ioctl->cmd);
	out_size = in_size = _IOC_SIZE(cmd);
	if ((cmd & ioctl->cmd & IOC_IN) == 0)
		in_size = 0;
	if ((cmd & ioctl->cmd & IOC_OUT) == 0)
		out_size = 0;
	ksize = max(max(in_size, out_size), drv_size);

	drm_dbg_core(dev, "comm=\"%s\" pid=%d, dev=0x%lx, auth=%d, %s\n",
		     current->comm, task_pid_nr(current),
		     (long)old_encode_dev(file_priv->minor->kdev->devt),
		     file_priv->authenticated, ioctl->name);

	 
	func = ioctl->func;

	if (unlikely(!func)) {
		drm_dbg_core(dev, "no function\n");
		retcode = -EINVAL;
		goto err_i1;
	}

	if (ksize <= sizeof(stack_kdata)) {
		kdata = stack_kdata;
	} else {
		kdata = kmalloc(ksize, GFP_KERNEL);
		if (!kdata) {
			retcode = -ENOMEM;
			goto err_i1;
		}
	}

	if (copy_from_user(kdata, (void __user *)arg, in_size) != 0) {
		retcode = -EFAULT;
		goto err_i1;
	}

	if (ksize > in_size)
		memset(kdata + in_size, 0, ksize - in_size);

	retcode = drm_ioctl_kernel(filp, func, kdata, ioctl->flags);
	if (copy_to_user((void __user *)arg, kdata, out_size) != 0)
		retcode = -EFAULT;

      err_i1:
	if (!ioctl)
		drm_dbg_core(dev,
			     "invalid ioctl: comm=\"%s\", pid=%d, dev=0x%lx, auth=%d, cmd=0x%02x, nr=0x%02x\n",
			     current->comm, task_pid_nr(current),
			     (long)old_encode_dev(file_priv->minor->kdev->devt),
			     file_priv->authenticated, cmd, nr);

	if (kdata != stack_kdata)
		kfree(kdata);
	if (retcode)
		drm_dbg_core(dev, "comm=\"%s\", pid=%d, ret=%d\n",
			     current->comm, task_pid_nr(current), retcode);
	return retcode;
}
EXPORT_SYMBOL(drm_ioctl);

 
bool drm_ioctl_flags(unsigned int nr, unsigned int *flags)
{
	if (nr >= DRM_COMMAND_BASE && nr < DRM_COMMAND_END)
		return false;

	if (nr >= DRM_CORE_IOCTL_COUNT)
		return false;
	nr = array_index_nospec(nr, DRM_CORE_IOCTL_COUNT);

	*flags = drm_ioctls[nr].flags;
	return true;
}
EXPORT_SYMBOL(drm_ioctl_flags);
