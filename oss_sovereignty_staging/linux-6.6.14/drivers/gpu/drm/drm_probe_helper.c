 

#include <linux/export.h>
#include <linux/moduleparam.h>

#include <drm/drm_bridge.h>
#include <drm/drm_client.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_modeset_helper_vtables.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_sysfs.h>

#include "drm_crtc_helper_internal.h"

 

static bool drm_kms_helper_poll = true;
module_param_named(poll, drm_kms_helper_poll, bool, 0600);

static enum drm_mode_status
drm_mode_validate_flag(const struct drm_display_mode *mode,
		       int flags)
{
	if ((mode->flags & DRM_MODE_FLAG_INTERLACE) &&
	    !(flags & DRM_MODE_FLAG_INTERLACE))
		return MODE_NO_INTERLACE;

	if ((mode->flags & DRM_MODE_FLAG_DBLSCAN) &&
	    !(flags & DRM_MODE_FLAG_DBLSCAN))
		return MODE_NO_DBLESCAN;

	if ((mode->flags & DRM_MODE_FLAG_3D_MASK) &&
	    !(flags & DRM_MODE_FLAG_3D_MASK))
		return MODE_NO_STEREO;

	return MODE_OK;
}

static int
drm_mode_validate_pipeline(struct drm_display_mode *mode,
			   struct drm_connector *connector,
			   struct drm_modeset_acquire_ctx *ctx,
			   enum drm_mode_status *status)
{
	struct drm_device *dev = connector->dev;
	struct drm_encoder *encoder;
	int ret;

	 
	ret = drm_connector_mode_valid(connector, mode, ctx, status);
	if (ret || *status != MODE_OK)
		return ret;

	 
	drm_connector_for_each_possible_encoder(connector, encoder) {
		struct drm_bridge *bridge;
		struct drm_crtc *crtc;

		*status = drm_encoder_mode_valid(encoder, mode);
		if (*status != MODE_OK) {
			 
			continue;
		}

		bridge = drm_bridge_chain_get_first_bridge(encoder);
		*status = drm_bridge_chain_mode_valid(bridge,
						      &connector->display_info,
						      mode);
		if (*status != MODE_OK) {
			 
			continue;
		}

		drm_for_each_crtc(crtc, dev) {
			if (!drm_encoder_crtc_ok(encoder, crtc))
				continue;

			*status = drm_crtc_mode_valid(crtc, mode);
			if (*status == MODE_OK) {
				 
				return 0;
			}
		}
	}

	return 0;
}

static int drm_helper_probe_add_cmdline_mode(struct drm_connector *connector)
{
	struct drm_cmdline_mode *cmdline_mode;
	struct drm_display_mode *mode;

	cmdline_mode = &connector->cmdline_mode;
	if (!cmdline_mode->specified)
		return 0;

	 
	list_for_each_entry(mode, &connector->probed_modes, head) {
		if (mode->hdisplay != cmdline_mode->xres ||
		    mode->vdisplay != cmdline_mode->yres)
			continue;

		if (cmdline_mode->refresh_specified) {
			 
			if (drm_mode_vrefresh(mode) != cmdline_mode->refresh)
				continue;
		}

		 
		mode->type |= DRM_MODE_TYPE_USERDEF;
		return 0;
	}

	mode = drm_mode_create_from_cmdline_mode(connector->dev,
						 cmdline_mode);
	if (mode == NULL)
		return 0;

	drm_mode_probed_add(connector, mode);
	return 1;
}

enum drm_mode_status drm_crtc_mode_valid(struct drm_crtc *crtc,
					 const struct drm_display_mode *mode)
{
	const struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;

	if (!crtc_funcs || !crtc_funcs->mode_valid)
		return MODE_OK;

	return crtc_funcs->mode_valid(crtc, mode);
}

enum drm_mode_status drm_encoder_mode_valid(struct drm_encoder *encoder,
					    const struct drm_display_mode *mode)
{
	const struct drm_encoder_helper_funcs *encoder_funcs =
		encoder->helper_private;

	if (!encoder_funcs || !encoder_funcs->mode_valid)
		return MODE_OK;

	return encoder_funcs->mode_valid(encoder, mode);
}

int
drm_connector_mode_valid(struct drm_connector *connector,
			 struct drm_display_mode *mode,
			 struct drm_modeset_acquire_ctx *ctx,
			 enum drm_mode_status *status)
{
	const struct drm_connector_helper_funcs *connector_funcs =
		connector->helper_private;
	int ret = 0;

	if (!connector_funcs)
		*status = MODE_OK;
	else if (connector_funcs->mode_valid_ctx)
		ret = connector_funcs->mode_valid_ctx(connector, mode, ctx,
						      status);
	else if (connector_funcs->mode_valid)
		*status = connector_funcs->mode_valid(connector, mode);
	else
		*status = MODE_OK;

	return ret;
}

static void drm_kms_helper_disable_hpd(struct drm_device *dev)
{
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		const struct drm_connector_helper_funcs *funcs =
			connector->helper_private;

		if (funcs && funcs->disable_hpd)
			funcs->disable_hpd(connector);
	}
	drm_connector_list_iter_end(&conn_iter);
}

static bool drm_kms_helper_enable_hpd(struct drm_device *dev)
{
	bool poll = false;
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		const struct drm_connector_helper_funcs *funcs =
			connector->helper_private;

		if (funcs && funcs->enable_hpd)
			funcs->enable_hpd(connector);

		if (connector->polled & (DRM_CONNECTOR_POLL_CONNECT |
					 DRM_CONNECTOR_POLL_DISCONNECT))
			poll = true;
	}
	drm_connector_list_iter_end(&conn_iter);

	return poll;
}

#define DRM_OUTPUT_POLL_PERIOD (10*HZ)
static void reschedule_output_poll_work(struct drm_device *dev)
{
	unsigned long delay = DRM_OUTPUT_POLL_PERIOD;

	if (dev->mode_config.delayed_event)
		 
		delay = HZ;

	schedule_delayed_work(&dev->mode_config.output_poll_work, delay);
}

 
void drm_kms_helper_poll_enable(struct drm_device *dev)
{
	if (!dev->mode_config.poll_enabled || !drm_kms_helper_poll ||
	    dev->mode_config.poll_running)
		return;

	if (drm_kms_helper_enable_hpd(dev) ||
	    dev->mode_config.delayed_event)
		reschedule_output_poll_work(dev);

	dev->mode_config.poll_running = true;
}
EXPORT_SYMBOL(drm_kms_helper_poll_enable);

 
void drm_kms_helper_poll_reschedule(struct drm_device *dev)
{
	if (dev->mode_config.poll_running)
		reschedule_output_poll_work(dev);
}
EXPORT_SYMBOL(drm_kms_helper_poll_reschedule);

static enum drm_connector_status
drm_helper_probe_detect_ctx(struct drm_connector *connector, bool force)
{
	const struct drm_connector_helper_funcs *funcs = connector->helper_private;
	struct drm_modeset_acquire_ctx ctx;
	int ret;

	drm_modeset_acquire_init(&ctx, 0);

retry:
	ret = drm_modeset_lock(&connector->dev->mode_config.connection_mutex, &ctx);
	if (!ret) {
		if (funcs->detect_ctx)
			ret = funcs->detect_ctx(connector, &ctx, force);
		else if (connector->funcs->detect)
			ret = connector->funcs->detect(connector, force);
		else
			ret = connector_status_connected;
	}

	if (ret == -EDEADLK) {
		drm_modeset_backoff(&ctx);
		goto retry;
	}

	if (WARN_ON(ret < 0))
		ret = connector_status_unknown;

	if (ret != connector->status)
		connector->epoch_counter += 1;

	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	return ret;
}

 
int
drm_helper_probe_detect(struct drm_connector *connector,
			struct drm_modeset_acquire_ctx *ctx,
			bool force)
{
	const struct drm_connector_helper_funcs *funcs = connector->helper_private;
	struct drm_device *dev = connector->dev;
	int ret;

	if (!ctx)
		return drm_helper_probe_detect_ctx(connector, force);

	ret = drm_modeset_lock(&dev->mode_config.connection_mutex, ctx);
	if (ret)
		return ret;

	if (funcs->detect_ctx)
		ret = funcs->detect_ctx(connector, ctx, force);
	else if (connector->funcs->detect)
		ret = connector->funcs->detect(connector, force);
	else
		ret = connector_status_connected;

	if (ret != connector->status)
		connector->epoch_counter += 1;

	return ret;
}
EXPORT_SYMBOL(drm_helper_probe_detect);

static int drm_helper_probe_get_modes(struct drm_connector *connector)
{
	const struct drm_connector_helper_funcs *connector_funcs =
		connector->helper_private;
	int count;

	count = connector_funcs->get_modes(connector);

	 
	if (count == 0 && connector->status == connector_status_connected)
		count = drm_edid_override_connector_update(connector);

	return count;
}

static int __drm_helper_update_and_validate(struct drm_connector *connector,
					    uint32_t maxX, uint32_t maxY,
					    struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode;
	int mode_flags = 0;
	int ret;

	drm_connector_list_update(connector);

	if (connector->interlace_allowed)
		mode_flags |= DRM_MODE_FLAG_INTERLACE;
	if (connector->doublescan_allowed)
		mode_flags |= DRM_MODE_FLAG_DBLSCAN;
	if (connector->stereo_allowed)
		mode_flags |= DRM_MODE_FLAG_3D_MASK;

	list_for_each_entry(mode, &connector->modes, head) {
		if (mode->status != MODE_OK)
			continue;

		mode->status = drm_mode_validate_driver(dev, mode);
		if (mode->status != MODE_OK)
			continue;

		mode->status = drm_mode_validate_size(mode, maxX, maxY);
		if (mode->status != MODE_OK)
			continue;

		mode->status = drm_mode_validate_flag(mode, mode_flags);
		if (mode->status != MODE_OK)
			continue;

		ret = drm_mode_validate_pipeline(mode, connector, ctx,
						 &mode->status);
		if (ret) {
			drm_dbg_kms(dev,
				    "drm_mode_validate_pipeline failed: %d\n",
				    ret);

			if (drm_WARN_ON_ONCE(dev, ret != -EDEADLK))
				mode->status = MODE_ERROR;
			else
				return -EDEADLK;
		}

		if (mode->status != MODE_OK)
			continue;
		mode->status = drm_mode_validate_ycbcr420(mode, connector);
	}

	return 0;
}

 
int drm_helper_probe_single_connector_modes(struct drm_connector *connector,
					    uint32_t maxX, uint32_t maxY)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode;
	int count = 0, ret;
	enum drm_connector_status old_status;
	struct drm_modeset_acquire_ctx ctx;

	WARN_ON(!mutex_is_locked(&dev->mode_config.mutex));

	drm_modeset_acquire_init(&ctx, 0);

	DRM_DEBUG_KMS("[CONNECTOR:%d:%s]\n", connector->base.id,
			connector->name);

retry:
	ret = drm_modeset_lock(&dev->mode_config.connection_mutex, &ctx);
	if (ret == -EDEADLK) {
		drm_modeset_backoff(&ctx);
		goto retry;
	} else
		WARN_ON(ret < 0);

	 
	list_for_each_entry(mode, &connector->modes, head)
		mode->status = MODE_STALE;

	old_status = connector->status;

	if (connector->force) {
		if (connector->force == DRM_FORCE_ON ||
		    connector->force == DRM_FORCE_ON_DIGITAL)
			connector->status = connector_status_connected;
		else
			connector->status = connector_status_disconnected;
		if (connector->funcs->force)
			connector->funcs->force(connector);
	} else {
		ret = drm_helper_probe_detect(connector, &ctx, true);

		if (ret == -EDEADLK) {
			drm_modeset_backoff(&ctx);
			goto retry;
		} else if (WARN(ret < 0, "Invalid return value %i for connector detection\n", ret))
			ret = connector_status_unknown;

		connector->status = ret;
	}

	 
	if (old_status != connector->status) {
		DRM_DEBUG_KMS("[CONNECTOR:%d:%s] status updated from %s to %s\n",
			      connector->base.id,
			      connector->name,
			      drm_get_connector_status_name(old_status),
			      drm_get_connector_status_name(connector->status));

		 
		dev->mode_config.delayed_event = true;
		if (dev->mode_config.poll_enabled)
			mod_delayed_work(system_wq,
					 &dev->mode_config.output_poll_work,
					 0);
	}

	 
	drm_kms_helper_poll_enable(dev);

	if (connector->status == connector_status_disconnected) {
		DRM_DEBUG_KMS("[CONNECTOR:%d:%s] disconnected\n",
			connector->base.id, connector->name);
		drm_connector_update_edid_property(connector, NULL);
		drm_mode_prune_invalid(dev, &connector->modes, false);
		goto exit;
	}

	count = drm_helper_probe_get_modes(connector);

	if (count == 0 && (connector->status == connector_status_connected ||
			   connector->status == connector_status_unknown)) {
		count = drm_add_modes_noedid(connector, 1024, 768);

		 
		if (connector->connector_type == DRM_MODE_CONNECTOR_DisplayPort)
			drm_set_preferred_mode(connector, 640, 480);
	}
	count += drm_helper_probe_add_cmdline_mode(connector);
	if (count != 0) {
		ret = __drm_helper_update_and_validate(connector, maxX, maxY, &ctx);
		if (ret == -EDEADLK) {
			drm_modeset_backoff(&ctx);
			goto retry;
		}
	}

	drm_mode_prune_invalid(dev, &connector->modes, true);

	 
	if (list_empty(&connector->modes) &&
	    connector->connector_type == DRM_MODE_CONNECTOR_DisplayPort) {
		count = drm_add_modes_noedid(connector, 640, 480);
		ret = __drm_helper_update_and_validate(connector, maxX, maxY, &ctx);
		if (ret == -EDEADLK) {
			drm_modeset_backoff(&ctx);
			goto retry;
		}
		drm_mode_prune_invalid(dev, &connector->modes, true);
	}

exit:
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	if (list_empty(&connector->modes))
		return 0;

	drm_mode_sort(&connector->modes);

	DRM_DEBUG_KMS("[CONNECTOR:%d:%s] probed modes :\n", connector->base.id,
			connector->name);
	list_for_each_entry(mode, &connector->modes, head) {
		drm_mode_set_crtcinfo(mode, CRTC_INTERLACE_HALVE_V);
		drm_mode_debug_printmodeline(mode);
	}

	return count;
}
EXPORT_SYMBOL(drm_helper_probe_single_connector_modes);

 
void drm_kms_helper_hotplug_event(struct drm_device *dev)
{
	 
	drm_sysfs_hotplug_event(dev);
	if (dev->mode_config.funcs->output_poll_changed)
		dev->mode_config.funcs->output_poll_changed(dev);

	drm_client_dev_hotplug(dev);
}
EXPORT_SYMBOL(drm_kms_helper_hotplug_event);

 
void drm_kms_helper_connector_hotplug_event(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;

	 
	drm_sysfs_connector_hotplug_event(connector);
	if (dev->mode_config.funcs->output_poll_changed)
		dev->mode_config.funcs->output_poll_changed(dev);

	drm_client_dev_hotplug(dev);
}
EXPORT_SYMBOL(drm_kms_helper_connector_hotplug_event);

static void output_poll_execute(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct drm_device *dev = container_of(delayed_work, struct drm_device, mode_config.output_poll_work);
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	enum drm_connector_status old_status;
	bool repoll = false, changed;
	u64 old_epoch_counter;

	if (!dev->mode_config.poll_enabled)
		return;

	 
	changed = dev->mode_config.delayed_event;
	dev->mode_config.delayed_event = false;

	if (!drm_kms_helper_poll && dev->mode_config.poll_running) {
		drm_kms_helper_disable_hpd(dev);
		dev->mode_config.poll_running = false;
		goto out;
	}

	if (!mutex_trylock(&dev->mode_config.mutex)) {
		repoll = true;
		goto out;
	}

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		 
		if (connector->force)
			continue;

		 
		if (!connector->polled || connector->polled == DRM_CONNECTOR_POLL_HPD)
			continue;

		old_status = connector->status;
		 
		if (old_status == connector_status_connected &&
		    !(connector->polled & DRM_CONNECTOR_POLL_DISCONNECT))
			continue;

		repoll = true;

		old_epoch_counter = connector->epoch_counter;
		connector->status = drm_helper_probe_detect(connector, NULL, false);
		if (old_epoch_counter != connector->epoch_counter) {
			const char *old, *new;

			 
			if (connector->status == connector_status_unknown) {
				connector->status = old_status;
				continue;
			}

			old = drm_get_connector_status_name(old_status);
			new = drm_get_connector_status_name(connector->status);

			DRM_DEBUG_KMS("[CONNECTOR:%d:%s] "
				      "status updated from %s to %s\n",
				      connector->base.id,
				      connector->name,
				      old, new);
			DRM_DEBUG_KMS("[CONNECTOR:%d:%s] epoch counter %llu -> %llu\n",
				      connector->base.id, connector->name,
				      old_epoch_counter, connector->epoch_counter);

			changed = true;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	mutex_unlock(&dev->mode_config.mutex);

out:
	if (changed)
		drm_kms_helper_hotplug_event(dev);

	if (repoll)
		schedule_delayed_work(delayed_work, DRM_OUTPUT_POLL_PERIOD);
}

 
bool drm_kms_helper_is_poll_worker(void)
{
	struct work_struct *work = current_work();

	return work && work->func == output_poll_execute;
}
EXPORT_SYMBOL(drm_kms_helper_is_poll_worker);

 
void drm_kms_helper_poll_disable(struct drm_device *dev)
{
	if (dev->mode_config.poll_running)
		drm_kms_helper_disable_hpd(dev);

	cancel_delayed_work_sync(&dev->mode_config.output_poll_work);

	dev->mode_config.poll_running = false;
}
EXPORT_SYMBOL(drm_kms_helper_poll_disable);

 
void drm_kms_helper_poll_init(struct drm_device *dev)
{
	INIT_DELAYED_WORK(&dev->mode_config.output_poll_work, output_poll_execute);
	dev->mode_config.poll_enabled = true;

	drm_kms_helper_poll_enable(dev);
}
EXPORT_SYMBOL(drm_kms_helper_poll_init);

 
void drm_kms_helper_poll_fini(struct drm_device *dev)
{
	if (!dev->mode_config.poll_enabled)
		return;

	drm_kms_helper_poll_disable(dev);

	dev->mode_config.poll_enabled = false;
}
EXPORT_SYMBOL(drm_kms_helper_poll_fini);

static bool check_connector_changed(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	enum drm_connector_status old_status;
	u64 old_epoch_counter;

	 
	drm_WARN_ON(dev, !(connector->polled & DRM_CONNECTOR_POLL_HPD));

	drm_WARN_ON(dev, !mutex_is_locked(&dev->mode_config.mutex));

	old_status = connector->status;
	old_epoch_counter = connector->epoch_counter;
	connector->status = drm_helper_probe_detect(connector, NULL, false);

	if (old_epoch_counter == connector->epoch_counter) {
		drm_dbg_kms(dev, "[CONNECTOR:%d:%s] Same epoch counter %llu\n",
			    connector->base.id,
			    connector->name,
			    connector->epoch_counter);

		return false;
	}

	drm_dbg_kms(dev, "[CONNECTOR:%d:%s] status updated from %s to %s\n",
		    connector->base.id,
		    connector->name,
		    drm_get_connector_status_name(old_status),
		    drm_get_connector_status_name(connector->status));

	drm_dbg_kms(dev, "[CONNECTOR:%d:%s] Changed epoch counter %llu => %llu\n",
		    connector->base.id,
		    connector->name,
		    old_epoch_counter,
		    connector->epoch_counter);

	return true;
}

 
bool drm_connector_helper_hpd_irq_event(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	bool changed;

	mutex_lock(&dev->mode_config.mutex);
	changed = check_connector_changed(connector);
	mutex_unlock(&dev->mode_config.mutex);

	if (changed) {
		drm_kms_helper_connector_hotplug_event(connector);
		drm_dbg_kms(dev, "[CONNECTOR:%d:%s] Sent hotplug event\n",
			    connector->base.id,
			    connector->name);
	}

	return changed;
}
EXPORT_SYMBOL(drm_connector_helper_hpd_irq_event);

 
bool drm_helper_hpd_irq_event(struct drm_device *dev)
{
	struct drm_connector *connector, *first_changed_connector = NULL;
	struct drm_connector_list_iter conn_iter;
	int changed = 0;

	if (!dev->mode_config.poll_enabled)
		return false;

	mutex_lock(&dev->mode_config.mutex);
	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		 
		if (!(connector->polled & DRM_CONNECTOR_POLL_HPD))
			continue;

		if (check_connector_changed(connector)) {
			if (!first_changed_connector) {
				drm_connector_get(connector);
				first_changed_connector = connector;
			}

			changed++;
		}
	}
	drm_connector_list_iter_end(&conn_iter);
	mutex_unlock(&dev->mode_config.mutex);

	if (changed == 1)
		drm_kms_helper_connector_hotplug_event(first_changed_connector);
	else if (changed > 0)
		drm_kms_helper_hotplug_event(dev);

	if (first_changed_connector)
		drm_connector_put(first_changed_connector);

	return changed;
}
EXPORT_SYMBOL(drm_helper_hpd_irq_event);

 
enum drm_mode_status drm_crtc_helper_mode_valid_fixed(struct drm_crtc *crtc,
						      const struct drm_display_mode *mode,
						      const struct drm_display_mode *fixed_mode)
{
	if (mode->hdisplay != fixed_mode->hdisplay && mode->vdisplay != fixed_mode->vdisplay)
		return MODE_ONE_SIZE;
	else if (mode->hdisplay != fixed_mode->hdisplay)
		return MODE_ONE_WIDTH;
	else if (mode->vdisplay != fixed_mode->vdisplay)
		return MODE_ONE_HEIGHT;

	return MODE_OK;
}
EXPORT_SYMBOL(drm_crtc_helper_mode_valid_fixed);

 
int drm_connector_helper_get_modes_from_ddc(struct drm_connector *connector)
{
	struct edid *edid;
	int count = 0;

	if (!connector->ddc)
		return 0;

	edid = drm_get_edid(connector, connector->ddc);

	 
	drm_connector_update_edid_property(connector, edid);

	if (edid) {
		count = drm_add_edid_modes(connector, edid);
		kfree(edid);
	}

	return count;
}
EXPORT_SYMBOL(drm_connector_helper_get_modes_from_ddc);

 
int drm_connector_helper_get_modes_fixed(struct drm_connector *connector,
					 const struct drm_display_mode *fixed_mode)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(dev, fixed_mode);
	if (!mode) {
		drm_err(dev, "Failed to duplicate mode " DRM_MODE_FMT "\n",
			DRM_MODE_ARG(fixed_mode));
		return 0;
	}

	if (mode->name[0] == '\0')
		drm_mode_set_name(mode);

	mode->type |= DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	if (mode->width_mm)
		connector->display_info.width_mm = mode->width_mm;
	if (mode->height_mm)
		connector->display_info.height_mm = mode->height_mm;

	return 1;
}
EXPORT_SYMBOL(drm_connector_helper_get_modes_fixed);

 
int drm_connector_helper_get_modes(struct drm_connector *connector)
{
	const struct drm_edid *drm_edid;
	int count;

	drm_edid = drm_edid_read(connector);

	 
	drm_edid_connector_update(connector, drm_edid);

	count = drm_edid_connector_add_modes(connector);

	drm_edid_free(drm_edid);

	return count;
}
EXPORT_SYMBOL(drm_connector_helper_get_modes);

 
int drm_connector_helper_tv_get_modes(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_property *tv_mode_property =
		dev->mode_config.tv_mode_property;
	struct drm_cmdline_mode *cmdline = &connector->cmdline_mode;
	unsigned int ntsc_modes = BIT(DRM_MODE_TV_MODE_NTSC) |
		BIT(DRM_MODE_TV_MODE_NTSC_443) |
		BIT(DRM_MODE_TV_MODE_NTSC_J) |
		BIT(DRM_MODE_TV_MODE_PAL_M);
	unsigned int pal_modes = BIT(DRM_MODE_TV_MODE_PAL) |
		BIT(DRM_MODE_TV_MODE_PAL_N) |
		BIT(DRM_MODE_TV_MODE_SECAM);
	unsigned int tv_modes[2] = { UINT_MAX, UINT_MAX };
	unsigned int i, supported_tv_modes = 0;

	if (!tv_mode_property)
		return 0;

	for (i = 0; i < tv_mode_property->num_values; i++)
		supported_tv_modes |= BIT(tv_mode_property->values[i]);

	if ((supported_tv_modes & ntsc_modes) &&
	    (supported_tv_modes & pal_modes)) {
		uint64_t default_mode;

		if (drm_object_property_get_default_value(&connector->base,
							  tv_mode_property,
							  &default_mode))
			return 0;

		if (cmdline->tv_mode_specified)
			default_mode = cmdline->tv_mode;

		if (BIT(default_mode) & ntsc_modes) {
			tv_modes[0] = DRM_MODE_TV_MODE_NTSC;
			tv_modes[1] = DRM_MODE_TV_MODE_PAL;
		} else {
			tv_modes[0] = DRM_MODE_TV_MODE_PAL;
			tv_modes[1] = DRM_MODE_TV_MODE_NTSC;
		}
	} else if (supported_tv_modes & ntsc_modes) {
		tv_modes[0] = DRM_MODE_TV_MODE_NTSC;
	} else if (supported_tv_modes & pal_modes) {
		tv_modes[0] = DRM_MODE_TV_MODE_PAL;
	} else {
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(tv_modes); i++) {
		struct drm_display_mode *mode;

		if (tv_modes[i] == DRM_MODE_TV_MODE_NTSC)
			mode = drm_mode_analog_ntsc_480i(dev);
		else if (tv_modes[i] == DRM_MODE_TV_MODE_PAL)
			mode = drm_mode_analog_pal_576i(dev);
		else
			break;
		if (!mode)
			return i;
		if (!i)
			mode->type |= DRM_MODE_TYPE_PREFERRED;
		drm_mode_probed_add(connector, mode);
	}

	return i;
}
EXPORT_SYMBOL(drm_connector_helper_tv_get_modes);
