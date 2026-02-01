 

#include <drm/drm_auth.h>
#include <drm/drm_connector.h>
#include <drm/drm_drv.h>
#include <drm/drm_edid.h>
#include <drm/drm_encoder.h>
#include <drm/drm_file.h>
#include <drm/drm_managed.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_privacy_screen_consumer.h>
#include <drm/drm_sysfs.h>
#include <drm/drm_utils.h>

#include <linux/property.h>
#include <linux/uaccess.h>

#include <video/cmdline.h>

#include "drm_crtc_internal.h"
#include "drm_internal.h"

 

 
static DEFINE_MUTEX(connector_list_lock);
static LIST_HEAD(connector_list);

struct drm_conn_prop_enum_list {
	int type;
	const char *name;
	struct ida ida;
};

 
static struct drm_conn_prop_enum_list drm_connector_enum_list[] = {
	{ DRM_MODE_CONNECTOR_Unknown, "Unknown" },
	{ DRM_MODE_CONNECTOR_VGA, "VGA" },
	{ DRM_MODE_CONNECTOR_DVII, "DVI-I" },
	{ DRM_MODE_CONNECTOR_DVID, "DVI-D" },
	{ DRM_MODE_CONNECTOR_DVIA, "DVI-A" },
	{ DRM_MODE_CONNECTOR_Composite, "Composite" },
	{ DRM_MODE_CONNECTOR_SVIDEO, "SVIDEO" },
	{ DRM_MODE_CONNECTOR_LVDS, "LVDS" },
	{ DRM_MODE_CONNECTOR_Component, "Component" },
	{ DRM_MODE_CONNECTOR_9PinDIN, "DIN" },
	{ DRM_MODE_CONNECTOR_DisplayPort, "DP" },
	{ DRM_MODE_CONNECTOR_HDMIA, "HDMI-A" },
	{ DRM_MODE_CONNECTOR_HDMIB, "HDMI-B" },
	{ DRM_MODE_CONNECTOR_TV, "TV" },
	{ DRM_MODE_CONNECTOR_eDP, "eDP" },
	{ DRM_MODE_CONNECTOR_VIRTUAL, "Virtual" },
	{ DRM_MODE_CONNECTOR_DSI, "DSI" },
	{ DRM_MODE_CONNECTOR_DPI, "DPI" },
	{ DRM_MODE_CONNECTOR_WRITEBACK, "Writeback" },
	{ DRM_MODE_CONNECTOR_SPI, "SPI" },
	{ DRM_MODE_CONNECTOR_USB, "USB" },
};

void drm_connector_ida_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(drm_connector_enum_list); i++)
		ida_init(&drm_connector_enum_list[i].ida);
}

void drm_connector_ida_destroy(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(drm_connector_enum_list); i++)
		ida_destroy(&drm_connector_enum_list[i].ida);
}

 
const char *drm_get_connector_type_name(unsigned int type)
{
	if (type < ARRAY_SIZE(drm_connector_enum_list))
		return drm_connector_enum_list[type].name;

	return NULL;
}
EXPORT_SYMBOL(drm_get_connector_type_name);

 
static void drm_connector_get_cmdline_mode(struct drm_connector *connector)
{
	struct drm_cmdline_mode *mode = &connector->cmdline_mode;
	const char *option;

	option = video_get_options(connector->name);
	if (!option)
		return;

	if (!drm_mode_parse_command_line_for_connector(option,
						       connector,
						       mode))
		return;

	if (mode->force) {
		DRM_INFO("forcing %s connector %s\n", connector->name,
			 drm_get_connector_force_name(mode->force));
		connector->force = mode->force;
	}

	if (mode->panel_orientation != DRM_MODE_PANEL_ORIENTATION_UNKNOWN) {
		DRM_INFO("cmdline forces connector %s panel_orientation to %d\n",
			 connector->name, mode->panel_orientation);
		drm_connector_set_panel_orientation(connector,
						    mode->panel_orientation);
	}

	DRM_DEBUG_KMS("cmdline mode for connector %s %s %dx%d@%dHz%s%s%s\n",
		      connector->name, mode->name,
		      mode->xres, mode->yres,
		      mode->refresh_specified ? mode->refresh : 60,
		      mode->rb ? " reduced blanking" : "",
		      mode->margins ? " with margins" : "",
		      mode->interlace ?  " interlaced" : "");
}

static void drm_connector_free(struct kref *kref)
{
	struct drm_connector *connector =
		container_of(kref, struct drm_connector, base.refcount);
	struct drm_device *dev = connector->dev;

	drm_mode_object_unregister(dev, &connector->base);
	connector->funcs->destroy(connector);
}

void drm_connector_free_work_fn(struct work_struct *work)
{
	struct drm_connector *connector, *n;
	struct drm_device *dev =
		container_of(work, struct drm_device, mode_config.connector_free_work);
	struct drm_mode_config *config = &dev->mode_config;
	unsigned long flags;
	struct llist_node *freed;

	spin_lock_irqsave(&config->connector_list_lock, flags);
	freed = llist_del_all(&config->connector_free_list);
	spin_unlock_irqrestore(&config->connector_list_lock, flags);

	llist_for_each_entry_safe(connector, n, freed, free_node) {
		drm_mode_object_unregister(dev, &connector->base);
		connector->funcs->destroy(connector);
	}
}

static int __drm_connector_init(struct drm_device *dev,
				struct drm_connector *connector,
				const struct drm_connector_funcs *funcs,
				int connector_type,
				struct i2c_adapter *ddc)
{
	struct drm_mode_config *config = &dev->mode_config;
	int ret;
	struct ida *connector_ida =
		&drm_connector_enum_list[connector_type].ida;

	WARN_ON(drm_drv_uses_atomic_modeset(dev) &&
		(!funcs->atomic_destroy_state ||
		 !funcs->atomic_duplicate_state));

	ret = __drm_mode_object_add(dev, &connector->base,
				    DRM_MODE_OBJECT_CONNECTOR,
				    false, drm_connector_free);
	if (ret)
		return ret;

	connector->base.properties = &connector->properties;
	connector->dev = dev;
	connector->funcs = funcs;

	 
	ret = ida_alloc_max(&config->connector_ida, 31, GFP_KERNEL);
	if (ret < 0) {
		DRM_DEBUG_KMS("Failed to allocate %s connector index: %d\n",
			      drm_connector_enum_list[connector_type].name,
			      ret);
		goto out_put;
	}
	connector->index = ret;
	ret = 0;

	connector->connector_type = connector_type;
	connector->connector_type_id =
		ida_alloc_min(connector_ida, 1, GFP_KERNEL);
	if (connector->connector_type_id < 0) {
		ret = connector->connector_type_id;
		goto out_put_id;
	}
	connector->name =
		kasprintf(GFP_KERNEL, "%s-%d",
			  drm_connector_enum_list[connector_type].name,
			  connector->connector_type_id);
	if (!connector->name) {
		ret = -ENOMEM;
		goto out_put_type_id;
	}

	 
	connector->ddc = ddc;

	INIT_LIST_HEAD(&connector->global_connector_list_entry);
	INIT_LIST_HEAD(&connector->probed_modes);
	INIT_LIST_HEAD(&connector->modes);
	mutex_init(&connector->mutex);
	mutex_init(&connector->edid_override_mutex);
	connector->edid_blob_ptr = NULL;
	connector->epoch_counter = 0;
	connector->tile_blob_ptr = NULL;
	connector->status = connector_status_unknown;
	connector->display_info.panel_orientation =
		DRM_MODE_PANEL_ORIENTATION_UNKNOWN;

	drm_connector_get_cmdline_mode(connector);

	 
	spin_lock_irq(&config->connector_list_lock);
	list_add_tail(&connector->head, &config->connector_list);
	config->num_connector++;
	spin_unlock_irq(&config->connector_list_lock);

	if (connector_type != DRM_MODE_CONNECTOR_VIRTUAL &&
	    connector_type != DRM_MODE_CONNECTOR_WRITEBACK)
		drm_connector_attach_edid_property(connector);

	drm_object_attach_property(&connector->base,
				      config->dpms_property, 0);

	drm_object_attach_property(&connector->base,
				   config->link_status_property,
				   0);

	drm_object_attach_property(&connector->base,
				   config->non_desktop_property,
				   0);
	drm_object_attach_property(&connector->base,
				   config->tile_property,
				   0);

	if (drm_core_check_feature(dev, DRIVER_ATOMIC)) {
		drm_object_attach_property(&connector->base, config->prop_crtc_id, 0);
	}

	connector->debugfs_entry = NULL;
out_put_type_id:
	if (ret)
		ida_free(connector_ida, connector->connector_type_id);
out_put_id:
	if (ret)
		ida_free(&config->connector_ida, connector->index);
out_put:
	if (ret)
		drm_mode_object_unregister(dev, &connector->base);

	return ret;
}

 
int drm_connector_init(struct drm_device *dev,
		       struct drm_connector *connector,
		       const struct drm_connector_funcs *funcs,
		       int connector_type)
{
	if (drm_WARN_ON(dev, !(funcs && funcs->destroy)))
		return -EINVAL;

	return __drm_connector_init(dev, connector, funcs, connector_type, NULL);
}
EXPORT_SYMBOL(drm_connector_init);

 
int drm_connector_init_with_ddc(struct drm_device *dev,
				struct drm_connector *connector,
				const struct drm_connector_funcs *funcs,
				int connector_type,
				struct i2c_adapter *ddc)
{
	if (drm_WARN_ON(dev, !(funcs && funcs->destroy)))
		return -EINVAL;

	return __drm_connector_init(dev, connector, funcs, connector_type, ddc);
}
EXPORT_SYMBOL(drm_connector_init_with_ddc);

static void drm_connector_cleanup_action(struct drm_device *dev,
					 void *ptr)
{
	struct drm_connector *connector = ptr;

	drm_connector_cleanup(connector);
}

 
int drmm_connector_init(struct drm_device *dev,
			struct drm_connector *connector,
			const struct drm_connector_funcs *funcs,
			int connector_type,
			struct i2c_adapter *ddc)
{
	int ret;

	if (drm_WARN_ON(dev, funcs && funcs->destroy))
		return -EINVAL;

	ret = __drm_connector_init(dev, connector, funcs, connector_type, ddc);
	if (ret)
		return ret;

	ret = drmm_add_action_or_reset(dev, drm_connector_cleanup_action,
				       connector);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(drmm_connector_init);

 
void drm_connector_attach_edid_property(struct drm_connector *connector)
{
	struct drm_mode_config *config = &connector->dev->mode_config;

	drm_object_attach_property(&connector->base,
				   config->edid_property,
				   0);
}
EXPORT_SYMBOL(drm_connector_attach_edid_property);

 
int drm_connector_attach_encoder(struct drm_connector *connector,
				 struct drm_encoder *encoder)
{
	 
	if (WARN_ON(connector->encoder))
		return -EINVAL;

	connector->possible_encoders |= drm_encoder_mask(encoder);

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_encoder);

 
bool drm_connector_has_possible_encoder(struct drm_connector *connector,
					struct drm_encoder *encoder)
{
	return connector->possible_encoders & drm_encoder_mask(encoder);
}
EXPORT_SYMBOL(drm_connector_has_possible_encoder);

static void drm_mode_remove(struct drm_connector *connector,
			    struct drm_display_mode *mode)
{
	list_del(&mode->head);
	drm_mode_destroy(connector->dev, mode);
}

 
void drm_connector_cleanup(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode, *t;

	 
	if (WARN_ON(connector->registration_state ==
		    DRM_CONNECTOR_REGISTERED))
		drm_connector_unregister(connector);

	if (connector->privacy_screen) {
		drm_privacy_screen_put(connector->privacy_screen);
		connector->privacy_screen = NULL;
	}

	if (connector->tile_group) {
		drm_mode_put_tile_group(dev, connector->tile_group);
		connector->tile_group = NULL;
	}

	list_for_each_entry_safe(mode, t, &connector->probed_modes, head)
		drm_mode_remove(connector, mode);

	list_for_each_entry_safe(mode, t, &connector->modes, head)
		drm_mode_remove(connector, mode);

	ida_free(&drm_connector_enum_list[connector->connector_type].ida,
			  connector->connector_type_id);

	ida_free(&dev->mode_config.connector_ida, connector->index);

	kfree(connector->display_info.bus_formats);
	kfree(connector->display_info.vics);
	drm_mode_object_unregister(dev, &connector->base);
	kfree(connector->name);
	connector->name = NULL;
	fwnode_handle_put(connector->fwnode);
	connector->fwnode = NULL;
	spin_lock_irq(&dev->mode_config.connector_list_lock);
	list_del(&connector->head);
	dev->mode_config.num_connector--;
	spin_unlock_irq(&dev->mode_config.connector_list_lock);

	WARN_ON(connector->state && !connector->funcs->atomic_destroy_state);
	if (connector->state && connector->funcs->atomic_destroy_state)
		connector->funcs->atomic_destroy_state(connector,
						       connector->state);

	mutex_destroy(&connector->mutex);

	memset(connector, 0, sizeof(*connector));

	if (dev->registered)
		drm_sysfs_hotplug_event(dev);
}
EXPORT_SYMBOL(drm_connector_cleanup);

 
int drm_connector_register(struct drm_connector *connector)
{
	int ret = 0;

	if (!connector->dev->registered)
		return 0;

	mutex_lock(&connector->mutex);
	if (connector->registration_state != DRM_CONNECTOR_INITIALIZING)
		goto unlock;

	ret = drm_sysfs_connector_add(connector);
	if (ret)
		goto unlock;

	drm_debugfs_connector_add(connector);

	if (connector->funcs->late_register) {
		ret = connector->funcs->late_register(connector);
		if (ret)
			goto err_debugfs;
	}

	drm_mode_object_register(connector->dev, &connector->base);

	connector->registration_state = DRM_CONNECTOR_REGISTERED;

	 
	drm_sysfs_connector_hotplug_event(connector);

	if (connector->privacy_screen)
		drm_privacy_screen_register_notifier(connector->privacy_screen,
					   &connector->privacy_screen_notifier);

	mutex_lock(&connector_list_lock);
	list_add_tail(&connector->global_connector_list_entry, &connector_list);
	mutex_unlock(&connector_list_lock);
	goto unlock;

err_debugfs:
	drm_debugfs_connector_remove(connector);
	drm_sysfs_connector_remove(connector);
unlock:
	mutex_unlock(&connector->mutex);
	return ret;
}
EXPORT_SYMBOL(drm_connector_register);

 
void drm_connector_unregister(struct drm_connector *connector)
{
	mutex_lock(&connector->mutex);
	if (connector->registration_state != DRM_CONNECTOR_REGISTERED) {
		mutex_unlock(&connector->mutex);
		return;
	}

	mutex_lock(&connector_list_lock);
	list_del_init(&connector->global_connector_list_entry);
	mutex_unlock(&connector_list_lock);

	if (connector->privacy_screen)
		drm_privacy_screen_unregister_notifier(
					connector->privacy_screen,
					&connector->privacy_screen_notifier);

	if (connector->funcs->early_unregister)
		connector->funcs->early_unregister(connector);

	drm_sysfs_connector_remove(connector);
	drm_debugfs_connector_remove(connector);

	connector->registration_state = DRM_CONNECTOR_UNREGISTERED;
	mutex_unlock(&connector->mutex);
}
EXPORT_SYMBOL(drm_connector_unregister);

void drm_connector_unregister_all(struct drm_device *dev)
{
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter)
		drm_connector_unregister(connector);
	drm_connector_list_iter_end(&conn_iter);
}

int drm_connector_register_all(struct drm_device *dev)
{
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	int ret = 0;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		ret = drm_connector_register(connector);
		if (ret)
			break;
	}
	drm_connector_list_iter_end(&conn_iter);

	if (ret)
		drm_connector_unregister_all(dev);
	return ret;
}

 
const char *drm_get_connector_status_name(enum drm_connector_status status)
{
	if (status == connector_status_connected)
		return "connected";
	else if (status == connector_status_disconnected)
		return "disconnected";
	else
		return "unknown";
}
EXPORT_SYMBOL(drm_get_connector_status_name);

 
const char *drm_get_connector_force_name(enum drm_connector_force force)
{
	switch (force) {
	case DRM_FORCE_UNSPECIFIED:
		return "unspecified";
	case DRM_FORCE_OFF:
		return "off";
	case DRM_FORCE_ON:
		return "on";
	case DRM_FORCE_ON_DIGITAL:
		return "digital";
	default:
		return "unknown";
	}
}

#ifdef CONFIG_LOCKDEP
static struct lockdep_map connector_list_iter_dep_map = {
	.name = "drm_connector_list_iter"
};
#endif

 
void drm_connector_list_iter_begin(struct drm_device *dev,
				   struct drm_connector_list_iter *iter)
{
	iter->dev = dev;
	iter->conn = NULL;
	lock_acquire_shared_recursive(&connector_list_iter_dep_map, 0, 1, NULL, _RET_IP_);
}
EXPORT_SYMBOL(drm_connector_list_iter_begin);

 
static void
__drm_connector_put_safe(struct drm_connector *conn)
{
	struct drm_mode_config *config = &conn->dev->mode_config;

	lockdep_assert_held(&config->connector_list_lock);

	if (!refcount_dec_and_test(&conn->base.refcount.refcount))
		return;

	llist_add(&conn->free_node, &config->connector_free_list);
	schedule_work(&config->connector_free_work);
}

 
struct drm_connector *
drm_connector_list_iter_next(struct drm_connector_list_iter *iter)
{
	struct drm_connector *old_conn = iter->conn;
	struct drm_mode_config *config = &iter->dev->mode_config;
	struct list_head *lhead;
	unsigned long flags;

	spin_lock_irqsave(&config->connector_list_lock, flags);
	lhead = old_conn ? &old_conn->head : &config->connector_list;

	do {
		if (lhead->next == &config->connector_list) {
			iter->conn = NULL;
			break;
		}

		lhead = lhead->next;
		iter->conn = list_entry(lhead, struct drm_connector, head);

		 
	} while (!kref_get_unless_zero(&iter->conn->base.refcount));

	if (old_conn)
		__drm_connector_put_safe(old_conn);
	spin_unlock_irqrestore(&config->connector_list_lock, flags);

	return iter->conn;
}
EXPORT_SYMBOL(drm_connector_list_iter_next);

 
void drm_connector_list_iter_end(struct drm_connector_list_iter *iter)
{
	struct drm_mode_config *config = &iter->dev->mode_config;
	unsigned long flags;

	iter->dev = NULL;
	if (iter->conn) {
		spin_lock_irqsave(&config->connector_list_lock, flags);
		__drm_connector_put_safe(iter->conn);
		spin_unlock_irqrestore(&config->connector_list_lock, flags);
	}
	lock_release(&connector_list_iter_dep_map, _RET_IP_);
}
EXPORT_SYMBOL(drm_connector_list_iter_end);

static const struct drm_prop_enum_list drm_subpixel_enum_list[] = {
	{ SubPixelUnknown, "Unknown" },
	{ SubPixelHorizontalRGB, "Horizontal RGB" },
	{ SubPixelHorizontalBGR, "Horizontal BGR" },
	{ SubPixelVerticalRGB, "Vertical RGB" },
	{ SubPixelVerticalBGR, "Vertical BGR" },
	{ SubPixelNone, "None" },
};

 
const char *drm_get_subpixel_order_name(enum subpixel_order order)
{
	return drm_subpixel_enum_list[order].name;
}
EXPORT_SYMBOL(drm_get_subpixel_order_name);

static const struct drm_prop_enum_list drm_dpms_enum_list[] = {
	{ DRM_MODE_DPMS_ON, "On" },
	{ DRM_MODE_DPMS_STANDBY, "Standby" },
	{ DRM_MODE_DPMS_SUSPEND, "Suspend" },
	{ DRM_MODE_DPMS_OFF, "Off" }
};
DRM_ENUM_NAME_FN(drm_get_dpms_name, drm_dpms_enum_list)

static const struct drm_prop_enum_list drm_link_status_enum_list[] = {
	{ DRM_MODE_LINK_STATUS_GOOD, "Good" },
	{ DRM_MODE_LINK_STATUS_BAD, "Bad" },
};

 
int drm_display_info_set_bus_formats(struct drm_display_info *info,
				     const u32 *formats,
				     unsigned int num_formats)
{
	u32 *fmts = NULL;

	if (!formats && num_formats)
		return -EINVAL;

	if (formats && num_formats) {
		fmts = kmemdup(formats, sizeof(*formats) * num_formats,
			       GFP_KERNEL);
		if (!fmts)
			return -ENOMEM;
	}

	kfree(info->bus_formats);
	info->bus_formats = fmts;
	info->num_bus_formats = num_formats;

	return 0;
}
EXPORT_SYMBOL(drm_display_info_set_bus_formats);

 
static const struct drm_prop_enum_list drm_scaling_mode_enum_list[] = {
	{ DRM_MODE_SCALE_NONE, "None" },
	{ DRM_MODE_SCALE_FULLSCREEN, "Full" },
	{ DRM_MODE_SCALE_CENTER, "Center" },
	{ DRM_MODE_SCALE_ASPECT, "Full aspect" },
};

static const struct drm_prop_enum_list drm_aspect_ratio_enum_list[] = {
	{ DRM_MODE_PICTURE_ASPECT_NONE, "Automatic" },
	{ DRM_MODE_PICTURE_ASPECT_4_3, "4:3" },
	{ DRM_MODE_PICTURE_ASPECT_16_9, "16:9" },
};

static const struct drm_prop_enum_list drm_content_type_enum_list[] = {
	{ DRM_MODE_CONTENT_TYPE_NO_DATA, "No Data" },
	{ DRM_MODE_CONTENT_TYPE_GRAPHICS, "Graphics" },
	{ DRM_MODE_CONTENT_TYPE_PHOTO, "Photo" },
	{ DRM_MODE_CONTENT_TYPE_CINEMA, "Cinema" },
	{ DRM_MODE_CONTENT_TYPE_GAME, "Game" },
};

static const struct drm_prop_enum_list drm_panel_orientation_enum_list[] = {
	{ DRM_MODE_PANEL_ORIENTATION_NORMAL,	"Normal"	},
	{ DRM_MODE_PANEL_ORIENTATION_BOTTOM_UP,	"Upside Down"	},
	{ DRM_MODE_PANEL_ORIENTATION_LEFT_UP,	"Left Side Up"	},
	{ DRM_MODE_PANEL_ORIENTATION_RIGHT_UP,	"Right Side Up"	},
};

static const struct drm_prop_enum_list drm_dvi_i_select_enum_list[] = {
	{ DRM_MODE_SUBCONNECTOR_Automatic, "Automatic" },  
	{ DRM_MODE_SUBCONNECTOR_DVID,      "DVI-D"     },  
	{ DRM_MODE_SUBCONNECTOR_DVIA,      "DVI-A"     },  
};
DRM_ENUM_NAME_FN(drm_get_dvi_i_select_name, drm_dvi_i_select_enum_list)

static const struct drm_prop_enum_list drm_dvi_i_subconnector_enum_list[] = {
	{ DRM_MODE_SUBCONNECTOR_Unknown,   "Unknown"   },  
	{ DRM_MODE_SUBCONNECTOR_DVID,      "DVI-D"     },  
	{ DRM_MODE_SUBCONNECTOR_DVIA,      "DVI-A"     },  
};
DRM_ENUM_NAME_FN(drm_get_dvi_i_subconnector_name,
		 drm_dvi_i_subconnector_enum_list)

static const struct drm_prop_enum_list drm_tv_mode_enum_list[] = {
	{ DRM_MODE_TV_MODE_NTSC, "NTSC" },
	{ DRM_MODE_TV_MODE_NTSC_443, "NTSC-443" },
	{ DRM_MODE_TV_MODE_NTSC_J, "NTSC-J" },
	{ DRM_MODE_TV_MODE_PAL, "PAL" },
	{ DRM_MODE_TV_MODE_PAL_M, "PAL-M" },
	{ DRM_MODE_TV_MODE_PAL_N, "PAL-N" },
	{ DRM_MODE_TV_MODE_SECAM, "SECAM" },
};
DRM_ENUM_NAME_FN(drm_get_tv_mode_name, drm_tv_mode_enum_list)

 
int drm_get_tv_mode_from_name(const char *name, size_t len)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(drm_tv_mode_enum_list); i++) {
		const struct drm_prop_enum_list *item = &drm_tv_mode_enum_list[i];

		if (strlen(item->name) == len && !strncmp(item->name, name, len))
			return item->type;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(drm_get_tv_mode_from_name);

static const struct drm_prop_enum_list drm_tv_select_enum_list[] = {
	{ DRM_MODE_SUBCONNECTOR_Automatic, "Automatic" },  
	{ DRM_MODE_SUBCONNECTOR_Composite, "Composite" },  
	{ DRM_MODE_SUBCONNECTOR_SVIDEO,    "SVIDEO"    },  
	{ DRM_MODE_SUBCONNECTOR_Component, "Component" },  
	{ DRM_MODE_SUBCONNECTOR_SCART,     "SCART"     },  
};
DRM_ENUM_NAME_FN(drm_get_tv_select_name, drm_tv_select_enum_list)

static const struct drm_prop_enum_list drm_tv_subconnector_enum_list[] = {
	{ DRM_MODE_SUBCONNECTOR_Unknown,   "Unknown"   },  
	{ DRM_MODE_SUBCONNECTOR_Composite, "Composite" },  
	{ DRM_MODE_SUBCONNECTOR_SVIDEO,    "SVIDEO"    },  
	{ DRM_MODE_SUBCONNECTOR_Component, "Component" },  
	{ DRM_MODE_SUBCONNECTOR_SCART,     "SCART"     },  
};
DRM_ENUM_NAME_FN(drm_get_tv_subconnector_name,
		 drm_tv_subconnector_enum_list)

static const struct drm_prop_enum_list drm_dp_subconnector_enum_list[] = {
	{ DRM_MODE_SUBCONNECTOR_Unknown,     "Unknown"   },  
	{ DRM_MODE_SUBCONNECTOR_VGA,	     "VGA"       },  
	{ DRM_MODE_SUBCONNECTOR_DVID,	     "DVI-D"     },  
	{ DRM_MODE_SUBCONNECTOR_HDMIA,	     "HDMI"      },  
	{ DRM_MODE_SUBCONNECTOR_DisplayPort, "DP"        },  
	{ DRM_MODE_SUBCONNECTOR_Wireless,    "Wireless"  },  
	{ DRM_MODE_SUBCONNECTOR_Native,	     "Native"    },  
};

DRM_ENUM_NAME_FN(drm_get_dp_subconnector_name,
		 drm_dp_subconnector_enum_list)


static const char * const colorspace_names[] = {
	 
	[DRM_MODE_COLORIMETRY_DEFAULT] = "Default",
	 
	[DRM_MODE_COLORIMETRY_SMPTE_170M_YCC] = "SMPTE_170M_YCC",
	[DRM_MODE_COLORIMETRY_BT709_YCC] = "BT709_YCC",
	 
	[DRM_MODE_COLORIMETRY_XVYCC_601] = "XVYCC_601",
	 
	[DRM_MODE_COLORIMETRY_XVYCC_709] = "XVYCC_709",
	 
	[DRM_MODE_COLORIMETRY_SYCC_601] = "SYCC_601",
	 
	[DRM_MODE_COLORIMETRY_OPYCC_601] = "opYCC_601",
	 
	[DRM_MODE_COLORIMETRY_OPRGB] = "opRGB",
	 
	[DRM_MODE_COLORIMETRY_BT2020_CYCC] = "BT2020_CYCC",
	 
	[DRM_MODE_COLORIMETRY_BT2020_RGB] = "BT2020_RGB",
	 
	[DRM_MODE_COLORIMETRY_BT2020_YCC] = "BT2020_YCC",
	 
	[DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65] = "DCI-P3_RGB_D65",
	[DRM_MODE_COLORIMETRY_DCI_P3_RGB_THEATER] = "DCI-P3_RGB_Theater",
	[DRM_MODE_COLORIMETRY_RGB_WIDE_FIXED] = "RGB_WIDE_FIXED",
	 
	[DRM_MODE_COLORIMETRY_RGB_WIDE_FLOAT] = "RGB_WIDE_FLOAT",
	[DRM_MODE_COLORIMETRY_BT601_YCC] = "BT601_YCC",
};

 
const char *drm_get_colorspace_name(enum drm_colorspace colorspace)
{
	if (colorspace < ARRAY_SIZE(colorspace_names) && colorspace_names[colorspace])
		return colorspace_names[colorspace];
	else
		return "(null)";
}

static const u32 hdmi_colorspaces =
	BIT(DRM_MODE_COLORIMETRY_SMPTE_170M_YCC) |
	BIT(DRM_MODE_COLORIMETRY_BT709_YCC) |
	BIT(DRM_MODE_COLORIMETRY_XVYCC_601) |
	BIT(DRM_MODE_COLORIMETRY_XVYCC_709) |
	BIT(DRM_MODE_COLORIMETRY_SYCC_601) |
	BIT(DRM_MODE_COLORIMETRY_OPYCC_601) |
	BIT(DRM_MODE_COLORIMETRY_OPRGB) |
	BIT(DRM_MODE_COLORIMETRY_BT2020_CYCC) |
	BIT(DRM_MODE_COLORIMETRY_BT2020_RGB) |
	BIT(DRM_MODE_COLORIMETRY_BT2020_YCC) |
	BIT(DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65) |
	BIT(DRM_MODE_COLORIMETRY_DCI_P3_RGB_THEATER);

 
static const u32 dp_colorspaces =
	BIT(DRM_MODE_COLORIMETRY_RGB_WIDE_FIXED) |
	BIT(DRM_MODE_COLORIMETRY_RGB_WIDE_FLOAT) |
	BIT(DRM_MODE_COLORIMETRY_OPRGB) |
	BIT(DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65) |
	BIT(DRM_MODE_COLORIMETRY_BT2020_RGB) |
	BIT(DRM_MODE_COLORIMETRY_BT601_YCC) |
	BIT(DRM_MODE_COLORIMETRY_BT709_YCC) |
	BIT(DRM_MODE_COLORIMETRY_XVYCC_601) |
	BIT(DRM_MODE_COLORIMETRY_XVYCC_709) |
	BIT(DRM_MODE_COLORIMETRY_SYCC_601) |
	BIT(DRM_MODE_COLORIMETRY_OPYCC_601) |
	BIT(DRM_MODE_COLORIMETRY_BT2020_CYCC) |
	BIT(DRM_MODE_COLORIMETRY_BT2020_YCC);

 

int drm_connector_create_standard_properties(struct drm_device *dev)
{
	struct drm_property *prop;

	prop = drm_property_create(dev, DRM_MODE_PROP_BLOB |
				   DRM_MODE_PROP_IMMUTABLE,
				   "EDID", 0);
	if (!prop)
		return -ENOMEM;
	dev->mode_config.edid_property = prop;

	prop = drm_property_create_enum(dev, 0,
				   "DPMS", drm_dpms_enum_list,
				   ARRAY_SIZE(drm_dpms_enum_list));
	if (!prop)
		return -ENOMEM;
	dev->mode_config.dpms_property = prop;

	prop = drm_property_create(dev,
				   DRM_MODE_PROP_BLOB |
				   DRM_MODE_PROP_IMMUTABLE,
				   "PATH", 0);
	if (!prop)
		return -ENOMEM;
	dev->mode_config.path_property = prop;

	prop = drm_property_create(dev,
				   DRM_MODE_PROP_BLOB |
				   DRM_MODE_PROP_IMMUTABLE,
				   "TILE", 0);
	if (!prop)
		return -ENOMEM;
	dev->mode_config.tile_property = prop;

	prop = drm_property_create_enum(dev, 0, "link-status",
					drm_link_status_enum_list,
					ARRAY_SIZE(drm_link_status_enum_list));
	if (!prop)
		return -ENOMEM;
	dev->mode_config.link_status_property = prop;

	prop = drm_property_create_bool(dev, DRM_MODE_PROP_IMMUTABLE, "non-desktop");
	if (!prop)
		return -ENOMEM;
	dev->mode_config.non_desktop_property = prop;

	prop = drm_property_create(dev, DRM_MODE_PROP_BLOB,
				   "HDR_OUTPUT_METADATA", 0);
	if (!prop)
		return -ENOMEM;
	dev->mode_config.hdr_output_metadata_property = prop;

	return 0;
}

 
int drm_mode_create_dvi_i_properties(struct drm_device *dev)
{
	struct drm_property *dvi_i_selector;
	struct drm_property *dvi_i_subconnector;

	if (dev->mode_config.dvi_i_select_subconnector_property)
		return 0;

	dvi_i_selector =
		drm_property_create_enum(dev, 0,
				    "select subconnector",
				    drm_dvi_i_select_enum_list,
				    ARRAY_SIZE(drm_dvi_i_select_enum_list));
	dev->mode_config.dvi_i_select_subconnector_property = dvi_i_selector;

	dvi_i_subconnector = drm_property_create_enum(dev, DRM_MODE_PROP_IMMUTABLE,
				    "subconnector",
				    drm_dvi_i_subconnector_enum_list,
				    ARRAY_SIZE(drm_dvi_i_subconnector_enum_list));
	dev->mode_config.dvi_i_subconnector_property = dvi_i_subconnector;

	return 0;
}
EXPORT_SYMBOL(drm_mode_create_dvi_i_properties);

 
void drm_connector_attach_dp_subconnector_property(struct drm_connector *connector)
{
	struct drm_mode_config *mode_config = &connector->dev->mode_config;

	if (!mode_config->dp_subconnector_property)
		mode_config->dp_subconnector_property =
			drm_property_create_enum(connector->dev,
				DRM_MODE_PROP_IMMUTABLE,
				"subconnector",
				drm_dp_subconnector_enum_list,
				ARRAY_SIZE(drm_dp_subconnector_enum_list));

	drm_object_attach_property(&connector->base,
				   mode_config->dp_subconnector_property,
				   DRM_MODE_SUBCONNECTOR_Unknown);
}
EXPORT_SYMBOL(drm_connector_attach_dp_subconnector_property);

 

 
 

 
int drm_connector_attach_content_type_property(struct drm_connector *connector)
{
	if (!drm_mode_create_content_type_property(connector->dev))
		drm_object_attach_property(&connector->base,
					   connector->dev->mode_config.content_type_property,
					   DRM_MODE_CONTENT_TYPE_NO_DATA);
	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_content_type_property);

 
void drm_connector_attach_tv_margin_properties(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;

	drm_object_attach_property(&connector->base,
				   dev->mode_config.tv_left_margin_property,
				   0);
	drm_object_attach_property(&connector->base,
				   dev->mode_config.tv_right_margin_property,
				   0);
	drm_object_attach_property(&connector->base,
				   dev->mode_config.tv_top_margin_property,
				   0);
	drm_object_attach_property(&connector->base,
				   dev->mode_config.tv_bottom_margin_property,
				   0);
}
EXPORT_SYMBOL(drm_connector_attach_tv_margin_properties);

 
int drm_mode_create_tv_margin_properties(struct drm_device *dev)
{
	if (dev->mode_config.tv_left_margin_property)
		return 0;

	dev->mode_config.tv_left_margin_property =
		drm_property_create_range(dev, 0, "left margin", 0, 100);
	if (!dev->mode_config.tv_left_margin_property)
		return -ENOMEM;

	dev->mode_config.tv_right_margin_property =
		drm_property_create_range(dev, 0, "right margin", 0, 100);
	if (!dev->mode_config.tv_right_margin_property)
		return -ENOMEM;

	dev->mode_config.tv_top_margin_property =
		drm_property_create_range(dev, 0, "top margin", 0, 100);
	if (!dev->mode_config.tv_top_margin_property)
		return -ENOMEM;

	dev->mode_config.tv_bottom_margin_property =
		drm_property_create_range(dev, 0, "bottom margin", 0, 100);
	if (!dev->mode_config.tv_bottom_margin_property)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL(drm_mode_create_tv_margin_properties);

 
int drm_mode_create_tv_properties_legacy(struct drm_device *dev,
					 unsigned int num_modes,
					 const char * const modes[])
{
	struct drm_property *tv_selector;
	struct drm_property *tv_subconnector;
	unsigned int i;

	if (dev->mode_config.tv_select_subconnector_property)
		return 0;

	 
	tv_selector = drm_property_create_enum(dev, 0,
					  "select subconnector",
					  drm_tv_select_enum_list,
					  ARRAY_SIZE(drm_tv_select_enum_list));
	if (!tv_selector)
		goto nomem;

	dev->mode_config.tv_select_subconnector_property = tv_selector;

	tv_subconnector =
		drm_property_create_enum(dev, DRM_MODE_PROP_IMMUTABLE,
				    "subconnector",
				    drm_tv_subconnector_enum_list,
				    ARRAY_SIZE(drm_tv_subconnector_enum_list));
	if (!tv_subconnector)
		goto nomem;
	dev->mode_config.tv_subconnector_property = tv_subconnector;

	 
	if (drm_mode_create_tv_margin_properties(dev))
		goto nomem;

	if (num_modes) {
		dev->mode_config.legacy_tv_mode_property =
			drm_property_create(dev, DRM_MODE_PROP_ENUM,
					    "mode", num_modes);
		if (!dev->mode_config.legacy_tv_mode_property)
			goto nomem;

		for (i = 0; i < num_modes; i++)
			drm_property_add_enum(dev->mode_config.legacy_tv_mode_property,
					      i, modes[i]);
	}

	dev->mode_config.tv_brightness_property =
		drm_property_create_range(dev, 0, "brightness", 0, 100);
	if (!dev->mode_config.tv_brightness_property)
		goto nomem;

	dev->mode_config.tv_contrast_property =
		drm_property_create_range(dev, 0, "contrast", 0, 100);
	if (!dev->mode_config.tv_contrast_property)
		goto nomem;

	dev->mode_config.tv_flicker_reduction_property =
		drm_property_create_range(dev, 0, "flicker reduction", 0, 100);
	if (!dev->mode_config.tv_flicker_reduction_property)
		goto nomem;

	dev->mode_config.tv_overscan_property =
		drm_property_create_range(dev, 0, "overscan", 0, 100);
	if (!dev->mode_config.tv_overscan_property)
		goto nomem;

	dev->mode_config.tv_saturation_property =
		drm_property_create_range(dev, 0, "saturation", 0, 100);
	if (!dev->mode_config.tv_saturation_property)
		goto nomem;

	dev->mode_config.tv_hue_property =
		drm_property_create_range(dev, 0, "hue", 0, 100);
	if (!dev->mode_config.tv_hue_property)
		goto nomem;

	return 0;
nomem:
	return -ENOMEM;
}
EXPORT_SYMBOL(drm_mode_create_tv_properties_legacy);

 
int drm_mode_create_tv_properties(struct drm_device *dev,
				  unsigned int supported_tv_modes)
{
	struct drm_prop_enum_list tv_mode_list[DRM_MODE_TV_MODE_MAX];
	struct drm_property *tv_mode;
	unsigned int i, len = 0;

	if (dev->mode_config.tv_mode_property)
		return 0;

	for (i = 0; i < DRM_MODE_TV_MODE_MAX; i++) {
		if (!(supported_tv_modes & BIT(i)))
			continue;

		tv_mode_list[len].type = i;
		tv_mode_list[len].name = drm_get_tv_mode_name(i);
		len++;
	}

	tv_mode = drm_property_create_enum(dev, 0, "TV mode",
					   tv_mode_list, len);
	if (!tv_mode)
		return -ENOMEM;

	dev->mode_config.tv_mode_property = tv_mode;

	return drm_mode_create_tv_properties_legacy(dev, 0, NULL);
}
EXPORT_SYMBOL(drm_mode_create_tv_properties);

 
int drm_mode_create_scaling_mode_property(struct drm_device *dev)
{
	struct drm_property *scaling_mode;

	if (dev->mode_config.scaling_mode_property)
		return 0;

	scaling_mode =
		drm_property_create_enum(dev, 0, "scaling mode",
				drm_scaling_mode_enum_list,
				    ARRAY_SIZE(drm_scaling_mode_enum_list));

	dev->mode_config.scaling_mode_property = scaling_mode;

	return 0;
}
EXPORT_SYMBOL(drm_mode_create_scaling_mode_property);

 

 
int drm_connector_attach_vrr_capable_property(
	struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_property *prop;

	if (!connector->vrr_capable_property) {
		prop = drm_property_create_bool(dev, DRM_MODE_PROP_IMMUTABLE,
			"vrr_capable");
		if (!prop)
			return -ENOMEM;

		connector->vrr_capable_property = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_vrr_capable_property);

 
int drm_connector_attach_scaling_mode_property(struct drm_connector *connector,
					       u32 scaling_mode_mask)
{
	struct drm_device *dev = connector->dev;
	struct drm_property *scaling_mode_property;
	int i;
	const unsigned valid_scaling_mode_mask =
		(1U << ARRAY_SIZE(drm_scaling_mode_enum_list)) - 1;

	if (WARN_ON(hweight32(scaling_mode_mask) < 2 ||
		    scaling_mode_mask & ~valid_scaling_mode_mask))
		return -EINVAL;

	scaling_mode_property =
		drm_property_create(dev, DRM_MODE_PROP_ENUM, "scaling mode",
				    hweight32(scaling_mode_mask));

	if (!scaling_mode_property)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(drm_scaling_mode_enum_list); i++) {
		int ret;

		if (!(BIT(i) & scaling_mode_mask))
			continue;

		ret = drm_property_add_enum(scaling_mode_property,
					    drm_scaling_mode_enum_list[i].type,
					    drm_scaling_mode_enum_list[i].name);

		if (ret) {
			drm_property_destroy(dev, scaling_mode_property);

			return ret;
		}
	}

	drm_object_attach_property(&connector->base,
				   scaling_mode_property, 0);

	connector->scaling_mode_property = scaling_mode_property;

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_scaling_mode_property);

 
int drm_mode_create_aspect_ratio_property(struct drm_device *dev)
{
	if (dev->mode_config.aspect_ratio_property)
		return 0;

	dev->mode_config.aspect_ratio_property =
		drm_property_create_enum(dev, 0, "aspect ratio",
				drm_aspect_ratio_enum_list,
				ARRAY_SIZE(drm_aspect_ratio_enum_list));

	if (dev->mode_config.aspect_ratio_property == NULL)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL(drm_mode_create_aspect_ratio_property);

 

static int drm_mode_create_colorspace_property(struct drm_connector *connector,
					u32 supported_colorspaces)
{
	struct drm_device *dev = connector->dev;
	u32 colorspaces = supported_colorspaces | BIT(DRM_MODE_COLORIMETRY_DEFAULT);
	struct drm_prop_enum_list enum_list[DRM_MODE_COLORIMETRY_COUNT];
	int i, len;

	if (connector->colorspace_property)
		return 0;

	if (!supported_colorspaces) {
		drm_err(dev, "No supported colorspaces provded on [CONNECTOR:%d:%s]\n",
			    connector->base.id, connector->name);
		return -EINVAL;
	}

	if ((supported_colorspaces & -BIT(DRM_MODE_COLORIMETRY_COUNT)) != 0) {
		drm_err(dev, "Unknown colorspace provded on [CONNECTOR:%d:%s]\n",
			    connector->base.id, connector->name);
		return -EINVAL;
	}

	len = 0;
	for (i = 0; i < DRM_MODE_COLORIMETRY_COUNT; i++) {
		if ((colorspaces & BIT(i)) == 0)
			continue;

		enum_list[len].type = i;
		enum_list[len].name = colorspace_names[i];
		len++;
	}

	connector->colorspace_property =
		drm_property_create_enum(dev, DRM_MODE_PROP_ENUM, "Colorspace",
					enum_list,
					len);

	if (!connector->colorspace_property)
		return -ENOMEM;

	return 0;
}

 
int drm_mode_create_hdmi_colorspace_property(struct drm_connector *connector,
					     u32 supported_colorspaces)
{
	u32 colorspaces;

	if (supported_colorspaces)
		colorspaces = supported_colorspaces & hdmi_colorspaces;
	else
		colorspaces = hdmi_colorspaces;

	return drm_mode_create_colorspace_property(connector, colorspaces);
}
EXPORT_SYMBOL(drm_mode_create_hdmi_colorspace_property);

 
int drm_mode_create_dp_colorspace_property(struct drm_connector *connector,
					   u32 supported_colorspaces)
{
	u32 colorspaces;

	if (supported_colorspaces)
		colorspaces = supported_colorspaces & dp_colorspaces;
	else
		colorspaces = dp_colorspaces;

	return drm_mode_create_colorspace_property(connector, colorspaces);
}
EXPORT_SYMBOL(drm_mode_create_dp_colorspace_property);

 
int drm_mode_create_content_type_property(struct drm_device *dev)
{
	if (dev->mode_config.content_type_property)
		return 0;

	dev->mode_config.content_type_property =
		drm_property_create_enum(dev, 0, "content type",
					 drm_content_type_enum_list,
					 ARRAY_SIZE(drm_content_type_enum_list));

	if (dev->mode_config.content_type_property == NULL)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL(drm_mode_create_content_type_property);

 
int drm_mode_create_suggested_offset_properties(struct drm_device *dev)
{
	if (dev->mode_config.suggested_x_property && dev->mode_config.suggested_y_property)
		return 0;

	dev->mode_config.suggested_x_property =
		drm_property_create_range(dev, DRM_MODE_PROP_IMMUTABLE, "suggested X", 0, 0xffffffff);

	dev->mode_config.suggested_y_property =
		drm_property_create_range(dev, DRM_MODE_PROP_IMMUTABLE, "suggested Y", 0, 0xffffffff);

	if (dev->mode_config.suggested_x_property == NULL ||
	    dev->mode_config.suggested_y_property == NULL)
		return -ENOMEM;
	return 0;
}
EXPORT_SYMBOL(drm_mode_create_suggested_offset_properties);

 
int drm_connector_set_path_property(struct drm_connector *connector,
				    const char *path)
{
	struct drm_device *dev = connector->dev;
	int ret;

	ret = drm_property_replace_global_blob(dev,
					       &connector->path_blob_ptr,
					       strlen(path) + 1,
					       path,
					       &connector->base,
					       dev->mode_config.path_property);
	return ret;
}
EXPORT_SYMBOL(drm_connector_set_path_property);

 
int drm_connector_set_tile_property(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	char tile[256];
	int ret;

	if (!connector->has_tile) {
		ret  = drm_property_replace_global_blob(dev,
							&connector->tile_blob_ptr,
							0,
							NULL,
							&connector->base,
							dev->mode_config.tile_property);
		return ret;
	}

	snprintf(tile, 256, "%d:%d:%d:%d:%d:%d:%d:%d",
		 connector->tile_group->id, connector->tile_is_single_monitor,
		 connector->num_h_tile, connector->num_v_tile,
		 connector->tile_h_loc, connector->tile_v_loc,
		 connector->tile_h_size, connector->tile_v_size);

	ret = drm_property_replace_global_blob(dev,
					       &connector->tile_blob_ptr,
					       strlen(tile) + 1,
					       tile,
					       &connector->base,
					       dev->mode_config.tile_property);
	return ret;
}
EXPORT_SYMBOL(drm_connector_set_tile_property);

 
void drm_connector_set_link_status_property(struct drm_connector *connector,
					    uint64_t link_status)
{
	struct drm_device *dev = connector->dev;

	drm_modeset_lock(&dev->mode_config.connection_mutex, NULL);
	connector->state->link_status = link_status;
	drm_modeset_unlock(&dev->mode_config.connection_mutex);
}
EXPORT_SYMBOL(drm_connector_set_link_status_property);

 
int drm_connector_attach_max_bpc_property(struct drm_connector *connector,
					  int min, int max)
{
	struct drm_device *dev = connector->dev;
	struct drm_property *prop;

	prop = connector->max_bpc_property;
	if (!prop) {
		prop = drm_property_create_range(dev, 0, "max bpc", min, max);
		if (!prop)
			return -ENOMEM;

		connector->max_bpc_property = prop;
	}

	drm_object_attach_property(&connector->base, prop, max);
	connector->state->max_requested_bpc = max;
	connector->state->max_bpc = max;

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_max_bpc_property);

 
int drm_connector_attach_hdr_output_metadata_property(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_property *prop = dev->mode_config.hdr_output_metadata_property;

	drm_object_attach_property(&connector->base, prop, 0);

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_hdr_output_metadata_property);

 
int drm_connector_attach_colorspace_property(struct drm_connector *connector)
{
	struct drm_property *prop = connector->colorspace_property;

	drm_object_attach_property(&connector->base, prop, DRM_MODE_COLORIMETRY_DEFAULT);

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_colorspace_property);

 
bool drm_connector_atomic_hdr_metadata_equal(struct drm_connector_state *old_state,
					     struct drm_connector_state *new_state)
{
	struct drm_property_blob *old_blob = old_state->hdr_output_metadata;
	struct drm_property_blob *new_blob = new_state->hdr_output_metadata;

	if (!old_blob || !new_blob)
		return old_blob == new_blob;

	if (old_blob->length != new_blob->length)
		return false;

	return !memcmp(old_blob->data, new_blob->data, old_blob->length);
}
EXPORT_SYMBOL(drm_connector_atomic_hdr_metadata_equal);

 
void drm_connector_set_vrr_capable_property(
		struct drm_connector *connector, bool capable)
{
	if (!connector->vrr_capable_property)
		return;

	drm_object_property_set_value(&connector->base,
				      connector->vrr_capable_property,
				      capable);
}
EXPORT_SYMBOL(drm_connector_set_vrr_capable_property);

 
int drm_connector_set_panel_orientation(
	struct drm_connector *connector,
	enum drm_panel_orientation panel_orientation)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_info *info = &connector->display_info;
	struct drm_property *prop;

	 
	if (info->panel_orientation != DRM_MODE_PANEL_ORIENTATION_UNKNOWN)
		return 0;

	 
	if (panel_orientation == DRM_MODE_PANEL_ORIENTATION_UNKNOWN)
		return 0;

	info->panel_orientation = panel_orientation;

	prop = dev->mode_config.panel_orientation_property;
	if (!prop) {
		prop = drm_property_create_enum(dev, DRM_MODE_PROP_IMMUTABLE,
				"panel orientation",
				drm_panel_orientation_enum_list,
				ARRAY_SIZE(drm_panel_orientation_enum_list));
		if (!prop)
			return -ENOMEM;

		dev->mode_config.panel_orientation_property = prop;
	}

	drm_object_attach_property(&connector->base, prop,
				   info->panel_orientation);
	return 0;
}
EXPORT_SYMBOL(drm_connector_set_panel_orientation);

 
int drm_connector_set_panel_orientation_with_quirk(
	struct drm_connector *connector,
	enum drm_panel_orientation panel_orientation,
	int width, int height)
{
	int orientation_quirk;

	orientation_quirk = drm_get_panel_orientation_quirk(width, height);
	if (orientation_quirk != DRM_MODE_PANEL_ORIENTATION_UNKNOWN)
		panel_orientation = orientation_quirk;

	return drm_connector_set_panel_orientation(connector,
						   panel_orientation);
}
EXPORT_SYMBOL(drm_connector_set_panel_orientation_with_quirk);

 
int drm_connector_set_orientation_from_panel(
	struct drm_connector *connector,
	struct drm_panel *panel)
{
	enum drm_panel_orientation orientation;

	if (panel && panel->funcs && panel->funcs->get_orientation)
		orientation = panel->funcs->get_orientation(panel);
	else
		orientation = DRM_MODE_PANEL_ORIENTATION_UNKNOWN;

	return drm_connector_set_panel_orientation(connector, orientation);
}
EXPORT_SYMBOL(drm_connector_set_orientation_from_panel);

static const struct drm_prop_enum_list privacy_screen_enum[] = {
	{ PRIVACY_SCREEN_DISABLED,		"Disabled" },
	{ PRIVACY_SCREEN_ENABLED,		"Enabled" },
	{ PRIVACY_SCREEN_DISABLED_LOCKED,	"Disabled-locked" },
	{ PRIVACY_SCREEN_ENABLED_LOCKED,	"Enabled-locked" },
};

 
void
drm_connector_create_privacy_screen_properties(struct drm_connector *connector)
{
	if (connector->privacy_screen_sw_state_property)
		return;

	 
	connector->privacy_screen_sw_state_property =
		drm_property_create_enum(connector->dev, DRM_MODE_PROP_ENUM,
				"privacy-screen sw-state",
				privacy_screen_enum, 2);

	connector->privacy_screen_hw_state_property =
		drm_property_create_enum(connector->dev,
				DRM_MODE_PROP_IMMUTABLE | DRM_MODE_PROP_ENUM,
				"privacy-screen hw-state",
				privacy_screen_enum,
				ARRAY_SIZE(privacy_screen_enum));
}
EXPORT_SYMBOL(drm_connector_create_privacy_screen_properties);

 
void
drm_connector_attach_privacy_screen_properties(struct drm_connector *connector)
{
	if (!connector->privacy_screen_sw_state_property)
		return;

	drm_object_attach_property(&connector->base,
				   connector->privacy_screen_sw_state_property,
				   PRIVACY_SCREEN_DISABLED);

	drm_object_attach_property(&connector->base,
				   connector->privacy_screen_hw_state_property,
				   PRIVACY_SCREEN_DISABLED);
}
EXPORT_SYMBOL(drm_connector_attach_privacy_screen_properties);

static void drm_connector_update_privacy_screen_properties(
	struct drm_connector *connector, bool set_sw_state)
{
	enum drm_privacy_screen_status sw_state, hw_state;

	drm_privacy_screen_get_state(connector->privacy_screen,
				     &sw_state, &hw_state);

	if (set_sw_state)
		connector->state->privacy_screen_sw_state = sw_state;
	drm_object_property_set_value(&connector->base,
			connector->privacy_screen_hw_state_property, hw_state);
}

static int drm_connector_privacy_screen_notifier(
	struct notifier_block *nb, unsigned long action, void *data)
{
	struct drm_connector *connector =
		container_of(nb, struct drm_connector, privacy_screen_notifier);
	struct drm_device *dev = connector->dev;

	drm_modeset_lock(&dev->mode_config.connection_mutex, NULL);
	drm_connector_update_privacy_screen_properties(connector, true);
	drm_modeset_unlock(&dev->mode_config.connection_mutex);

	drm_sysfs_connector_property_event(connector,
					   connector->privacy_screen_sw_state_property);
	drm_sysfs_connector_property_event(connector,
					   connector->privacy_screen_hw_state_property);

	return NOTIFY_DONE;
}

 
void drm_connector_attach_privacy_screen_provider(
	struct drm_connector *connector, struct drm_privacy_screen *priv)
{
	connector->privacy_screen = priv;
	connector->privacy_screen_notifier.notifier_call =
		drm_connector_privacy_screen_notifier;

	drm_connector_create_privacy_screen_properties(connector);
	drm_connector_update_privacy_screen_properties(connector, true);
	drm_connector_attach_privacy_screen_properties(connector);
}
EXPORT_SYMBOL(drm_connector_attach_privacy_screen_provider);

 
void drm_connector_update_privacy_screen(const struct drm_connector_state *connector_state)
{
	struct drm_connector *connector = connector_state->connector;
	int ret;

	if (!connector->privacy_screen)
		return;

	ret = drm_privacy_screen_set_sw_state(connector->privacy_screen,
					      connector_state->privacy_screen_sw_state);
	if (ret) {
		drm_err(connector->dev, "Error updating privacy-screen sw_state\n");
		return;
	}

	 
	drm_connector_update_privacy_screen_properties(connector, false);
}
EXPORT_SYMBOL(drm_connector_update_privacy_screen);

int drm_connector_set_obj_prop(struct drm_mode_object *obj,
				    struct drm_property *property,
				    uint64_t value)
{
	int ret = -EINVAL;
	struct drm_connector *connector = obj_to_connector(obj);

	 
	if (property == connector->dev->mode_config.dpms_property) {
		ret = (*connector->funcs->dpms)(connector, (int)value);
	} else if (connector->funcs->set_property)
		ret = connector->funcs->set_property(connector, property, value);

	if (!ret)
		drm_object_property_set_value(&connector->base, property, value);
	return ret;
}

int drm_connector_property_set_ioctl(struct drm_device *dev,
				     void *data, struct drm_file *file_priv)
{
	struct drm_mode_connector_set_property *conn_set_prop = data;
	struct drm_mode_obj_set_property obj_set_prop = {
		.value = conn_set_prop->value,
		.prop_id = conn_set_prop->prop_id,
		.obj_id = conn_set_prop->connector_id,
		.obj_type = DRM_MODE_OBJECT_CONNECTOR
	};

	 
	return drm_mode_obj_set_property_ioctl(dev, &obj_set_prop, file_priv);
}

static struct drm_encoder *drm_connector_get_encoder(struct drm_connector *connector)
{
	 
	if (connector->state)
		return connector->state->best_encoder;
	return connector->encoder;
}

static bool
drm_mode_expose_to_userspace(const struct drm_display_mode *mode,
			     const struct list_head *modes,
			     const struct drm_file *file_priv)
{
	 
	if (!file_priv->stereo_allowed && drm_mode_is_stereo(mode))
		return false;
	 
	if (!file_priv->aspect_ratio_allowed) {
		const struct drm_display_mode *mode_itr;

		list_for_each_entry(mode_itr, modes, head) {
			if (mode_itr->expose_to_userspace &&
			    drm_mode_match(mode_itr, mode,
					   DRM_MODE_MATCH_TIMINGS |
					   DRM_MODE_MATCH_CLOCK |
					   DRM_MODE_MATCH_FLAGS |
					   DRM_MODE_MATCH_3D_FLAGS))
				return false;
		}
	}

	return true;
}

int drm_mode_getconnector(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_mode_get_connector *out_resp = data;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct drm_display_mode *mode;
	int mode_count = 0;
	int encoders_count = 0;
	int ret = 0;
	int copied = 0;
	struct drm_mode_modeinfo u_mode;
	struct drm_mode_modeinfo __user *mode_ptr;
	uint32_t __user *encoder_ptr;
	bool is_current_master;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EOPNOTSUPP;

	memset(&u_mode, 0, sizeof(struct drm_mode_modeinfo));

	connector = drm_connector_lookup(dev, file_priv, out_resp->connector_id);
	if (!connector)
		return -ENOENT;

	encoders_count = hweight32(connector->possible_encoders);

	if ((out_resp->count_encoders >= encoders_count) && encoders_count) {
		copied = 0;
		encoder_ptr = (uint32_t __user *)(unsigned long)(out_resp->encoders_ptr);

		drm_connector_for_each_possible_encoder(connector, encoder) {
			if (put_user(encoder->base.id, encoder_ptr + copied)) {
				ret = -EFAULT;
				goto out;
			}
			copied++;
		}
	}
	out_resp->count_encoders = encoders_count;

	out_resp->connector_id = connector->base.id;
	out_resp->connector_type = connector->connector_type;
	out_resp->connector_type_id = connector->connector_type_id;

	is_current_master = drm_is_current_master(file_priv);

	mutex_lock(&dev->mode_config.mutex);
	if (out_resp->count_modes == 0) {
		if (is_current_master)
			connector->funcs->fill_modes(connector,
						     dev->mode_config.max_width,
						     dev->mode_config.max_height);
		else
			drm_dbg_kms(dev, "User-space requested a forced probe on [CONNECTOR:%d:%s] but is not the DRM master, demoting to read-only probe",
				    connector->base.id, connector->name);
	}

	out_resp->mm_width = connector->display_info.width_mm;
	out_resp->mm_height = connector->display_info.height_mm;
	out_resp->subpixel = connector->display_info.subpixel_order;
	out_resp->connection = connector->status;

	 
	list_for_each_entry(mode, &connector->modes, head) {
		WARN_ON(mode->expose_to_userspace);

		if (drm_mode_expose_to_userspace(mode, &connector->modes,
						 file_priv)) {
			mode->expose_to_userspace = true;
			mode_count++;
		}
	}

	 
	if ((out_resp->count_modes >= mode_count) && mode_count) {
		copied = 0;
		mode_ptr = (struct drm_mode_modeinfo __user *)(unsigned long)out_resp->modes_ptr;
		list_for_each_entry(mode, &connector->modes, head) {
			if (!mode->expose_to_userspace)
				continue;

			 
			mode->expose_to_userspace = false;

			drm_mode_convert_to_umode(&u_mode, mode);
			 
			if (!file_priv->aspect_ratio_allowed)
				u_mode.flags &= ~DRM_MODE_FLAG_PIC_AR_MASK;
			if (copy_to_user(mode_ptr + copied,
					 &u_mode, sizeof(u_mode))) {
				ret = -EFAULT;

				 
				list_for_each_entry_continue(mode, &connector->modes, head)
					mode->expose_to_userspace = false;

				mutex_unlock(&dev->mode_config.mutex);

				goto out;
			}
			copied++;
		}
	} else {
		 
		list_for_each_entry(mode, &connector->modes, head)
			mode->expose_to_userspace = false;
	}

	out_resp->count_modes = mode_count;
	mutex_unlock(&dev->mode_config.mutex);

	drm_modeset_lock(&dev->mode_config.connection_mutex, NULL);
	encoder = drm_connector_get_encoder(connector);
	if (encoder)
		out_resp->encoder_id = encoder->base.id;
	else
		out_resp->encoder_id = 0;

	 
	ret = drm_mode_object_get_properties(&connector->base, file_priv->atomic,
			(uint32_t __user *)(unsigned long)(out_resp->props_ptr),
			(uint64_t __user *)(unsigned long)(out_resp->prop_values_ptr),
			&out_resp->count_props);
	drm_modeset_unlock(&dev->mode_config.connection_mutex);

out:
	drm_connector_put(connector);

	return ret;
}

 
struct drm_connector *drm_connector_find_by_fwnode(struct fwnode_handle *fwnode)
{
	struct drm_connector *connector, *found = ERR_PTR(-ENODEV);

	if (!fwnode)
		return ERR_PTR(-ENODEV);

	mutex_lock(&connector_list_lock);

	list_for_each_entry(connector, &connector_list, global_connector_list_entry) {
		if (connector->fwnode == fwnode ||
		    (connector->fwnode && connector->fwnode->secondary == fwnode)) {
			drm_connector_get(connector);
			found = connector;
			break;
		}
	}

	mutex_unlock(&connector_list_lock);

	return found;
}

 
void drm_connector_oob_hotplug_event(struct fwnode_handle *connector_fwnode)
{
	struct drm_connector *connector;

	connector = drm_connector_find_by_fwnode(connector_fwnode);
	if (IS_ERR(connector))
		return;

	if (connector->funcs->oob_hotplug_event)
		connector->funcs->oob_hotplug_event(connector);

	drm_connector_put(connector);
}
EXPORT_SYMBOL(drm_connector_oob_hotplug_event);


 

static void drm_tile_group_free(struct kref *kref)
{
	struct drm_tile_group *tg = container_of(kref, struct drm_tile_group, refcount);
	struct drm_device *dev = tg->dev;

	mutex_lock(&dev->mode_config.idr_mutex);
	idr_remove(&dev->mode_config.tile_idr, tg->id);
	mutex_unlock(&dev->mode_config.idr_mutex);
	kfree(tg);
}

 
void drm_mode_put_tile_group(struct drm_device *dev,
			     struct drm_tile_group *tg)
{
	kref_put(&tg->refcount, drm_tile_group_free);
}
EXPORT_SYMBOL(drm_mode_put_tile_group);

 
struct drm_tile_group *drm_mode_get_tile_group(struct drm_device *dev,
					       const char topology[8])
{
	struct drm_tile_group *tg;
	int id;

	mutex_lock(&dev->mode_config.idr_mutex);
	idr_for_each_entry(&dev->mode_config.tile_idr, tg, id) {
		if (!memcmp(tg->group_data, topology, 8)) {
			if (!kref_get_unless_zero(&tg->refcount))
				tg = NULL;
			mutex_unlock(&dev->mode_config.idr_mutex);
			return tg;
		}
	}
	mutex_unlock(&dev->mode_config.idr_mutex);
	return NULL;
}
EXPORT_SYMBOL(drm_mode_get_tile_group);

 
struct drm_tile_group *drm_mode_create_tile_group(struct drm_device *dev,
						  const char topology[8])
{
	struct drm_tile_group *tg;
	int ret;

	tg = kzalloc(sizeof(*tg), GFP_KERNEL);
	if (!tg)
		return NULL;

	kref_init(&tg->refcount);
	memcpy(tg->group_data, topology, 8);
	tg->dev = dev;

	mutex_lock(&dev->mode_config.idr_mutex);
	ret = idr_alloc(&dev->mode_config.tile_idr, tg, 1, 0, GFP_KERNEL);
	if (ret >= 0) {
		tg->id = ret;
	} else {
		kfree(tg);
		tg = NULL;
	}

	mutex_unlock(&dev->mode_config.idr_mutex);
	return tg;
}
EXPORT_SYMBOL(drm_mode_create_tile_group);
