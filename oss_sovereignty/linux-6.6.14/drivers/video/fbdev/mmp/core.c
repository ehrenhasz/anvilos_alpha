
 

#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/export.h>
#include <linux/module.h>
#include <video/mmp_disp.h>

static struct mmp_overlay *path_get_overlay(struct mmp_path *path,
		int overlay_id)
{
	if (path && overlay_id < path->overlay_num)
		return &path->overlays[overlay_id];
	return NULL;
}

static int path_check_status(struct mmp_path *path)
{
	int i;
	for (i = 0; i < path->overlay_num; i++)
		if (path->overlays[i].status)
			return 1;

	return 0;
}

 
static int path_get_modelist(struct mmp_path *path,
		struct mmp_mode **modelist)
{
	BUG_ON(!path || !modelist);

	if (path->panel && path->panel->get_modelist)
		return path->panel->get_modelist(path->panel, modelist);

	return 0;
}

 
static LIST_HEAD(panel_list);
static LIST_HEAD(path_list);
static DEFINE_MUTEX(disp_lock);

 
void mmp_register_panel(struct mmp_panel *panel)
{
	struct mmp_path *path;

	mutex_lock(&disp_lock);

	 
	list_add_tail(&panel->node, &panel_list);

	 
	list_for_each_entry(path, &path_list, node) {
		if (!strcmp(panel->plat_path_name, path->name)) {
			dev_info(panel->dev, "connect to path %s\n",
				path->name);
			path->panel = panel;
			break;
		}
	}

	mutex_unlock(&disp_lock);
}
EXPORT_SYMBOL_GPL(mmp_register_panel);

 
void mmp_unregister_panel(struct mmp_panel *panel)
{
	struct mmp_path *path;

	mutex_lock(&disp_lock);
	list_del(&panel->node);

	list_for_each_entry(path, &path_list, node) {
		if (path->panel && path->panel == panel) {
			dev_info(panel->dev, "disconnect from path %s\n",
				path->name);
			path->panel = NULL;
			break;
		}
	}
	mutex_unlock(&disp_lock);
}
EXPORT_SYMBOL_GPL(mmp_unregister_panel);

 
struct mmp_path *mmp_get_path(const char *name)
{
	struct mmp_path *path = NULL, *iter;

	mutex_lock(&disp_lock);
	list_for_each_entry(iter, &path_list, node) {
		if (!strcmp(name, iter->name)) {
			path = iter;
			break;
		}
	}
	mutex_unlock(&disp_lock);

	return path;
}
EXPORT_SYMBOL_GPL(mmp_get_path);

 
struct mmp_path *mmp_register_path(struct mmp_path_info *info)
{
	int i;
	struct mmp_path *path = NULL;
	struct mmp_panel *panel;

	path = kzalloc(struct_size(path, overlays, info->overlay_num),
		       GFP_KERNEL);
	if (!path)
		return NULL;

	 
	mutex_init(&path->access_ok);
	path->dev = info->dev;
	path->id = info->id;
	path->name = info->name;
	path->output_type = info->output_type;
	path->overlay_num = info->overlay_num;
	path->plat_data = info->plat_data;
	path->ops.set_mode = info->set_mode;

	mutex_lock(&disp_lock);
	 
	list_for_each_entry(panel, &panel_list, node) {
		if (!strcmp(info->name, panel->plat_path_name)) {
			dev_info(path->dev, "get panel %s\n", panel->name);
			path->panel = panel;
			break;
		}
	}

	dev_info(path->dev, "register %s, overlay_num %d\n",
			path->name, path->overlay_num);

	 
	if (!path->ops.check_status)
		path->ops.check_status = path_check_status;
	if (!path->ops.get_overlay)
		path->ops.get_overlay = path_get_overlay;
	if (!path->ops.get_modelist)
		path->ops.get_modelist = path_get_modelist;

	 
	for (i = 0; i < path->overlay_num; i++) {
		path->overlays[i].path = path;
		path->overlays[i].id = i;
		mutex_init(&path->overlays[i].access_ok);
		path->overlays[i].ops = info->overlay_ops;
	}

	 
	list_add_tail(&path->node, &path_list);

	mutex_unlock(&disp_lock);
	return path;
}
EXPORT_SYMBOL_GPL(mmp_register_path);

 
void mmp_unregister_path(struct mmp_path *path)
{
	int i;

	if (!path)
		return;

	mutex_lock(&disp_lock);
	 
	list_del(&path->node);

	 
	for (i = 0; i < path->overlay_num; i++)
		mutex_destroy(&path->overlays[i].access_ok);

	mutex_destroy(&path->access_ok);

	kfree(path);
	mutex_unlock(&disp_lock);
}
EXPORT_SYMBOL_GPL(mmp_unregister_path);

MODULE_AUTHOR("Zhou Zhu <zzhu3@marvell.com>");
MODULE_DESCRIPTION("Marvell MMP display framework");
MODULE_LICENSE("GPL");
