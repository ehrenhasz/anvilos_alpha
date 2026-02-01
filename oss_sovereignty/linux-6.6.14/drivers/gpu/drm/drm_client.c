
 

#include <linux/iosys-map.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include <drm/drm_client.h>
#include <drm/drm_debugfs.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_mode.h>
#include <drm/drm_print.h>

#include "drm_crtc_internal.h"
#include "drm_internal.h"

 

static int drm_client_open(struct drm_client_dev *client)
{
	struct drm_device *dev = client->dev;
	struct drm_file *file;

	file = drm_file_alloc(dev->primary);
	if (IS_ERR(file))
		return PTR_ERR(file);

	mutex_lock(&dev->filelist_mutex);
	list_add(&file->lhead, &dev->filelist_internal);
	mutex_unlock(&dev->filelist_mutex);

	client->file = file;

	return 0;
}

static void drm_client_close(struct drm_client_dev *client)
{
	struct drm_device *dev = client->dev;

	mutex_lock(&dev->filelist_mutex);
	list_del(&client->file->lhead);
	mutex_unlock(&dev->filelist_mutex);

	drm_file_free(client->file);
}

 
int drm_client_init(struct drm_device *dev, struct drm_client_dev *client,
		    const char *name, const struct drm_client_funcs *funcs)
{
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_MODESET) || !dev->driver->dumb_create)
		return -EOPNOTSUPP;

	if (funcs && !try_module_get(funcs->owner))
		return -ENODEV;

	client->dev = dev;
	client->name = name;
	client->funcs = funcs;

	ret = drm_client_modeset_create(client);
	if (ret)
		goto err_put_module;

	ret = drm_client_open(client);
	if (ret)
		goto err_free;

	drm_dev_get(dev);

	return 0;

err_free:
	drm_client_modeset_free(client);
err_put_module:
	if (funcs)
		module_put(funcs->owner);

	return ret;
}
EXPORT_SYMBOL(drm_client_init);

 
void drm_client_register(struct drm_client_dev *client)
{
	struct drm_device *dev = client->dev;
	int ret;

	mutex_lock(&dev->clientlist_mutex);
	list_add(&client->list, &dev->clientlist);

	if (client->funcs && client->funcs->hotplug) {
		 
		ret = client->funcs->hotplug(client);
		if (ret)
			drm_dbg_kms(dev, "client hotplug ret=%d\n", ret);
	}
	mutex_unlock(&dev->clientlist_mutex);
}
EXPORT_SYMBOL(drm_client_register);

 
void drm_client_release(struct drm_client_dev *client)
{
	struct drm_device *dev = client->dev;

	drm_dbg_kms(dev, "%s\n", client->name);

	drm_client_modeset_free(client);
	drm_client_close(client);
	drm_dev_put(dev);
	if (client->funcs)
		module_put(client->funcs->owner);
}
EXPORT_SYMBOL(drm_client_release);

void drm_client_dev_unregister(struct drm_device *dev)
{
	struct drm_client_dev *client, *tmp;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return;

	mutex_lock(&dev->clientlist_mutex);
	list_for_each_entry_safe(client, tmp, &dev->clientlist, list) {
		list_del(&client->list);
		if (client->funcs && client->funcs->unregister) {
			client->funcs->unregister(client);
		} else {
			drm_client_release(client);
			kfree(client);
		}
	}
	mutex_unlock(&dev->clientlist_mutex);
}

 
void drm_client_dev_hotplug(struct drm_device *dev)
{
	struct drm_client_dev *client;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return;

	if (!dev->mode_config.num_connector) {
		drm_dbg_kms(dev, "No connectors found, will not send hotplug events!\n");
		return;
	}

	mutex_lock(&dev->clientlist_mutex);
	list_for_each_entry(client, &dev->clientlist, list) {
		if (!client->funcs || !client->funcs->hotplug)
			continue;

		if (client->hotplug_failed)
			continue;

		ret = client->funcs->hotplug(client);
		drm_dbg_kms(dev, "%s: ret=%d\n", client->name, ret);
		if (ret)
			client->hotplug_failed = true;
	}
	mutex_unlock(&dev->clientlist_mutex);
}
EXPORT_SYMBOL(drm_client_dev_hotplug);

void drm_client_dev_restore(struct drm_device *dev)
{
	struct drm_client_dev *client;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return;

	mutex_lock(&dev->clientlist_mutex);
	list_for_each_entry(client, &dev->clientlist, list) {
		if (!client->funcs || !client->funcs->restore)
			continue;

		ret = client->funcs->restore(client);
		drm_dbg_kms(dev, "%s: ret=%d\n", client->name, ret);
		if (!ret)  
			break;
	}
	mutex_unlock(&dev->clientlist_mutex);
}

static void drm_client_buffer_delete(struct drm_client_buffer *buffer)
{
	if (buffer->gem) {
		drm_gem_vunmap_unlocked(buffer->gem, &buffer->map);
		drm_gem_object_put(buffer->gem);
	}

	kfree(buffer);
}

static struct drm_client_buffer *
drm_client_buffer_create(struct drm_client_dev *client, u32 width, u32 height,
			 u32 format, u32 *handle)
{
	const struct drm_format_info *info = drm_format_info(format);
	struct drm_mode_create_dumb dumb_args = { };
	struct drm_device *dev = client->dev;
	struct drm_client_buffer *buffer;
	struct drm_gem_object *obj;
	int ret;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->client = client;

	dumb_args.width = width;
	dumb_args.height = height;
	dumb_args.bpp = drm_format_info_bpp(info, 0);
	ret = drm_mode_create_dumb(dev, &dumb_args, client->file);
	if (ret)
		goto err_delete;

	obj = drm_gem_object_lookup(client->file, dumb_args.handle);
	if (!obj)  {
		ret = -ENOENT;
		goto err_delete;
	}

	buffer->pitch = dumb_args.pitch;
	buffer->gem = obj;
	*handle = dumb_args.handle;

	return buffer;

err_delete:
	drm_client_buffer_delete(buffer);

	return ERR_PTR(ret);
}

 
int
drm_client_buffer_vmap(struct drm_client_buffer *buffer,
		       struct iosys_map *map_copy)
{
	struct iosys_map *map = &buffer->map;
	int ret;

	 
	ret = drm_gem_vmap_unlocked(buffer->gem, map);
	if (ret)
		return ret;

	*map_copy = *map;

	return 0;
}
EXPORT_SYMBOL(drm_client_buffer_vmap);

 
void drm_client_buffer_vunmap(struct drm_client_buffer *buffer)
{
	struct iosys_map *map = &buffer->map;

	drm_gem_vunmap_unlocked(buffer->gem, map);
}
EXPORT_SYMBOL(drm_client_buffer_vunmap);

static void drm_client_buffer_rmfb(struct drm_client_buffer *buffer)
{
	int ret;

	if (!buffer->fb)
		return;

	ret = drm_mode_rmfb(buffer->client->dev, buffer->fb->base.id, buffer->client->file);
	if (ret)
		drm_err(buffer->client->dev,
			"Error removing FB:%u (%d)\n", buffer->fb->base.id, ret);

	buffer->fb = NULL;
}

static int drm_client_buffer_addfb(struct drm_client_buffer *buffer,
				   u32 width, u32 height, u32 format,
				   u32 handle)
{
	struct drm_client_dev *client = buffer->client;
	struct drm_mode_fb_cmd fb_req = { };
	const struct drm_format_info *info;
	int ret;

	info = drm_format_info(format);
	fb_req.bpp = drm_format_info_bpp(info, 0);
	fb_req.depth = info->depth;
	fb_req.width = width;
	fb_req.height = height;
	fb_req.handle = handle;
	fb_req.pitch = buffer->pitch;

	ret = drm_mode_addfb(client->dev, &fb_req, client->file);
	if (ret)
		return ret;

	buffer->fb = drm_framebuffer_lookup(client->dev, buffer->client->file, fb_req.fb_id);
	if (WARN_ON(!buffer->fb))
		return -ENOENT;

	 
	drm_framebuffer_put(buffer->fb);

	strscpy(buffer->fb->comm, client->name, TASK_COMM_LEN);

	return 0;
}

 
struct drm_client_buffer *
drm_client_framebuffer_create(struct drm_client_dev *client, u32 width, u32 height, u32 format)
{
	struct drm_client_buffer *buffer;
	u32 handle;
	int ret;

	buffer = drm_client_buffer_create(client, width, height, format,
					  &handle);
	if (IS_ERR(buffer))
		return buffer;

	ret = drm_client_buffer_addfb(buffer, width, height, format, handle);

	 
	drm_mode_destroy_dumb(client->dev, handle, client->file);

	if (ret) {
		drm_client_buffer_delete(buffer);
		return ERR_PTR(ret);
	}

	return buffer;
}
EXPORT_SYMBOL(drm_client_framebuffer_create);

 
void drm_client_framebuffer_delete(struct drm_client_buffer *buffer)
{
	if (!buffer)
		return;

	drm_client_buffer_rmfb(buffer);
	drm_client_buffer_delete(buffer);
}
EXPORT_SYMBOL(drm_client_framebuffer_delete);

 
int drm_client_framebuffer_flush(struct drm_client_buffer *buffer, struct drm_rect *rect)
{
	if (!buffer || !buffer->fb || !buffer->fb->funcs->dirty)
		return 0;

	if (rect) {
		struct drm_clip_rect clip = {
			.x1 = rect->x1,
			.y1 = rect->y1,
			.x2 = rect->x2,
			.y2 = rect->y2,
		};

		return buffer->fb->funcs->dirty(buffer->fb, buffer->client->file,
						0, 0, &clip, 1);
	}

	return buffer->fb->funcs->dirty(buffer->fb, buffer->client->file,
					0, 0, NULL, 0);
}
EXPORT_SYMBOL(drm_client_framebuffer_flush);

#ifdef CONFIG_DEBUG_FS
static int drm_client_debugfs_internal_clients(struct seq_file *m, void *data)
{
	struct drm_debugfs_entry *entry = m->private;
	struct drm_device *dev = entry->dev;
	struct drm_printer p = drm_seq_file_printer(m);
	struct drm_client_dev *client;

	mutex_lock(&dev->clientlist_mutex);
	list_for_each_entry(client, &dev->clientlist, list)
		drm_printf(&p, "%s\n", client->name);
	mutex_unlock(&dev->clientlist_mutex);

	return 0;
}

static const struct drm_debugfs_info drm_client_debugfs_list[] = {
	{ "internal_clients", drm_client_debugfs_internal_clients, 0 },
};

void drm_client_debugfs_init(struct drm_minor *minor)
{
	drm_debugfs_add_files(minor->dev, drm_client_debugfs_list,
			      ARRAY_SIZE(drm_client_debugfs_list));
}
#endif
