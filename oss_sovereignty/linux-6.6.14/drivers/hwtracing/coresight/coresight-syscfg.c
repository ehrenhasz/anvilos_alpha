
 

#include <linux/platform_device.h>
#include <linux/slab.h>

#include "coresight-config.h"
#include "coresight-etm-perf.h"
#include "coresight-syscfg.h"
#include "coresight-syscfg-configfs.h"

 

 
static DEFINE_MUTEX(cscfg_mutex);

 
static struct cscfg_manager *cscfg_mgr;

 

 
static struct cscfg_feature_csdev *
cscfg_get_feat_csdev(struct coresight_device *csdev, const char *name)
{
	struct cscfg_feature_csdev *feat_csdev = NULL;

	list_for_each_entry(feat_csdev, &csdev->feature_csdev_list, node) {
		if (strcmp(feat_csdev->feat_desc->name, name) == 0)
			return feat_csdev;
	}
	return NULL;
}

 
static struct cscfg_config_csdev *
cscfg_alloc_csdev_cfg(struct coresight_device *csdev, int nr_feats)
{
	struct cscfg_config_csdev *config_csdev = NULL;
	struct device *dev = csdev->dev.parent;

	 
	config_csdev = devm_kzalloc(dev,
				    offsetof(struct cscfg_config_csdev, feats_csdev[nr_feats]),
				    GFP_KERNEL);
	if (!config_csdev)
		return NULL;

	config_csdev->csdev = csdev;
	return config_csdev;
}

 
static int cscfg_add_csdev_cfg(struct coresight_device *csdev,
			       struct cscfg_config_desc *config_desc)
{
	struct cscfg_config_csdev *config_csdev = NULL;
	struct cscfg_feature_csdev *feat_csdev;
	unsigned long flags;
	int i;

	 
	for (i = 0; i < config_desc->nr_feat_refs; i++) {
		 
		feat_csdev = cscfg_get_feat_csdev(csdev, config_desc->feat_ref_names[i]);
		if (feat_csdev) {
			 
			if (!config_csdev) {
				config_csdev = cscfg_alloc_csdev_cfg(csdev,
								     config_desc->nr_feat_refs);
				if (!config_csdev)
					return -ENOMEM;
				config_csdev->config_desc = config_desc;
			}
			config_csdev->feats_csdev[config_csdev->nr_feat++] = feat_csdev;
		}
	}
	 
	if (config_csdev) {
		spin_lock_irqsave(&csdev->cscfg_csdev_lock, flags);
		list_add(&config_csdev->node, &csdev->config_csdev_list);
		spin_unlock_irqrestore(&csdev->cscfg_csdev_lock, flags);
	}

	return 0;
}

 
static int cscfg_add_cfg_to_csdevs(struct cscfg_config_desc *config_desc)
{
	struct cscfg_registered_csdev *csdev_item;
	int err;

	list_for_each_entry(csdev_item, &cscfg_mgr->csdev_desc_list, item) {
		err = cscfg_add_csdev_cfg(csdev_item->csdev, config_desc);
		if (err)
			return err;
	}
	return 0;
}

 
static struct cscfg_feature_csdev *
cscfg_alloc_csdev_feat(struct coresight_device *csdev, struct cscfg_feature_desc *feat_desc)
{
	struct cscfg_feature_csdev *feat_csdev = NULL;
	struct device *dev = csdev->dev.parent;
	int i;

	feat_csdev = devm_kzalloc(dev, sizeof(struct cscfg_feature_csdev), GFP_KERNEL);
	if (!feat_csdev)
		return NULL;

	 
	feat_csdev->nr_params = feat_desc->nr_params;

	 
	if (feat_csdev->nr_params) {
		feat_csdev->params_csdev = devm_kcalloc(dev, feat_csdev->nr_params,
							sizeof(struct cscfg_parameter_csdev),
							GFP_KERNEL);
		if (!feat_csdev->params_csdev)
			return NULL;

		 
		for (i = 0; i < feat_csdev->nr_params; i++)
			feat_csdev->params_csdev[i].feat_csdev = feat_csdev;
	}

	 
	feat_csdev->nr_regs = feat_desc->nr_regs;
	feat_csdev->regs_csdev = devm_kcalloc(dev, feat_csdev->nr_regs,
					      sizeof(struct cscfg_regval_csdev),
					      GFP_KERNEL);
	if (!feat_csdev->regs_csdev)
		return NULL;

	 
	feat_csdev->feat_desc = feat_desc;
	feat_csdev->csdev = csdev;

	return feat_csdev;
}

 
static int cscfg_load_feat_csdev(struct coresight_device *csdev,
				 struct cscfg_feature_desc *feat_desc,
				 struct cscfg_csdev_feat_ops *ops)
{
	struct cscfg_feature_csdev *feat_csdev;
	unsigned long flags;
	int err;

	if (!ops->load_feat)
		return -EINVAL;

	feat_csdev = cscfg_alloc_csdev_feat(csdev, feat_desc);
	if (!feat_csdev)
		return -ENOMEM;

	 
	err = ops->load_feat(csdev, feat_csdev);
	if (err)
		return err;

	 
	cscfg_reset_feat(feat_csdev);
	spin_lock_irqsave(&csdev->cscfg_csdev_lock, flags);
	list_add(&feat_csdev->node, &csdev->feature_csdev_list);
	spin_unlock_irqrestore(&csdev->cscfg_csdev_lock, flags);

	return 0;
}

 
static int cscfg_add_feat_to_csdevs(struct cscfg_feature_desc *feat_desc)
{
	struct cscfg_registered_csdev *csdev_item;
	int err;

	list_for_each_entry(csdev_item, &cscfg_mgr->csdev_desc_list, item) {
		if (csdev_item->match_flags & feat_desc->match_flags) {
			err = cscfg_load_feat_csdev(csdev_item->csdev, feat_desc, &csdev_item->ops);
			if (err)
				return err;
		}
	}
	return 0;
}

 
static bool cscfg_match_list_feat(const char *name)
{
	struct cscfg_feature_desc *feat_desc;

	list_for_each_entry(feat_desc, &cscfg_mgr->feat_desc_list, item) {
		if (strcmp(feat_desc->name, name) == 0)
			return true;
	}
	return false;
}

 
static int cscfg_check_feat_for_cfg(struct cscfg_config_desc *config_desc)
{
	int i;

	for (i = 0; i < config_desc->nr_feat_refs; i++)
		if (!cscfg_match_list_feat(config_desc->feat_ref_names[i]))
			return -EINVAL;
	return 0;
}

 
static int cscfg_load_feat(struct cscfg_feature_desc *feat_desc)
{
	int err;
	struct cscfg_feature_desc *feat_desc_exist;

	 
	list_for_each_entry(feat_desc_exist, &cscfg_mgr->feat_desc_list, item) {
		if (!strcmp(feat_desc_exist->name, feat_desc->name))
			return -EEXIST;
	}

	 
	err = cscfg_add_feat_to_csdevs(feat_desc);
	if (err)
		return err;

	list_add(&feat_desc->item, &cscfg_mgr->feat_desc_list);
	return 0;
}

 
static int cscfg_load_config(struct cscfg_config_desc *config_desc)
{
	int err;
	struct cscfg_config_desc *config_desc_exist;

	 
	list_for_each_entry(config_desc_exist, &cscfg_mgr->config_desc_list, item) {
		if (!strcmp(config_desc_exist->name, config_desc->name))
			return -EEXIST;
	}

	 
	err = cscfg_check_feat_for_cfg(config_desc);
	if (err)
		return err;

	 
	err = cscfg_add_cfg_to_csdevs(config_desc);
	if (err)
		return err;

	 
	err = etm_perf_add_symlink_cscfg(cscfg_device(), config_desc);
	if (err)
		return err;

	list_add(&config_desc->item, &cscfg_mgr->config_desc_list);
	atomic_set(&config_desc->active_cnt, 0);
	return 0;
}

 
const struct cscfg_feature_desc *cscfg_get_named_feat_desc(const char *name)
{
	const struct cscfg_feature_desc *feat_desc = NULL, *feat_desc_item;

	mutex_lock(&cscfg_mutex);

	list_for_each_entry(feat_desc_item, &cscfg_mgr->feat_desc_list, item) {
		if (strcmp(feat_desc_item->name, name) == 0) {
			feat_desc = feat_desc_item;
			break;
		}
	}

	mutex_unlock(&cscfg_mutex);
	return feat_desc;
}

 
static struct cscfg_feature_csdev *
cscfg_csdev_get_feat_from_desc(struct coresight_device *csdev,
			       struct cscfg_feature_desc *feat_desc)
{
	struct cscfg_feature_csdev *feat_csdev;

	list_for_each_entry(feat_csdev, &csdev->feature_csdev_list, node) {
		if (feat_csdev->feat_desc == feat_desc)
			return feat_csdev;
	}
	return NULL;
}

int cscfg_update_feat_param_val(struct cscfg_feature_desc *feat_desc,
				int param_idx, u64 value)
{
	int err = 0;
	struct cscfg_feature_csdev *feat_csdev;
	struct cscfg_registered_csdev *csdev_item;

	mutex_lock(&cscfg_mutex);

	 
	if (atomic_read(&cscfg_mgr->sys_active_cnt)) {
		err = -EBUSY;
		goto unlock_exit;
	}

	 
	if ((param_idx < 0) || (param_idx >= feat_desc->nr_params)) {
		err = -EINVAL;
		goto unlock_exit;
	}
	feat_desc->params_desc[param_idx].value = value;

	 
	list_for_each_entry(csdev_item, &cscfg_mgr->csdev_desc_list, item) {
		feat_csdev = cscfg_csdev_get_feat_from_desc(csdev_item->csdev, feat_desc);
		if (feat_csdev)
			feat_csdev->params_csdev[param_idx].current_value = value;
	}

unlock_exit:
	mutex_unlock(&cscfg_mutex);
	return err;
}

 
static int cscfg_owner_get(struct cscfg_load_owner_info *owner_info)
{
	if ((owner_info->type == CSCFG_OWNER_MODULE) &&
	    (!try_module_get(owner_info->owner_handle)))
		return -EINVAL;
	return 0;
}

 
static void cscfg_owner_put(struct cscfg_load_owner_info *owner_info)
{
	if (owner_info->type == CSCFG_OWNER_MODULE)
		module_put(owner_info->owner_handle);
}

static void cscfg_remove_owned_csdev_configs(struct coresight_device *csdev, void *load_owner)
{
	struct cscfg_config_csdev *config_csdev, *tmp;

	if (list_empty(&csdev->config_csdev_list))
		return;

	list_for_each_entry_safe(config_csdev, tmp, &csdev->config_csdev_list, node) {
		if (config_csdev->config_desc->load_owner == load_owner)
			list_del(&config_csdev->node);
	}
}

static void cscfg_remove_owned_csdev_features(struct coresight_device *csdev, void *load_owner)
{
	struct cscfg_feature_csdev *feat_csdev, *tmp;

	if (list_empty(&csdev->feature_csdev_list))
		return;

	list_for_each_entry_safe(feat_csdev, tmp, &csdev->feature_csdev_list, node) {
		if (feat_csdev->feat_desc->load_owner == load_owner)
			list_del(&feat_csdev->node);
	}
}

 
static void cscfg_fs_unregister_cfgs_feats(void *load_owner)
{
	struct cscfg_config_desc *config_desc;
	struct cscfg_feature_desc *feat_desc;

	list_for_each_entry(config_desc, &cscfg_mgr->config_desc_list, item) {
		if (config_desc->load_owner == load_owner)
			cscfg_configfs_del_config(config_desc);
	}
	list_for_each_entry(feat_desc, &cscfg_mgr->feat_desc_list, item) {
		if (feat_desc->load_owner == load_owner)
			cscfg_configfs_del_feature(feat_desc);
	}
}

 
static void cscfg_unload_owned_cfgs_feats(void *load_owner)
{
	struct cscfg_config_desc *config_desc, *cfg_tmp;
	struct cscfg_feature_desc *feat_desc, *feat_tmp;
	struct cscfg_registered_csdev *csdev_item;

	lockdep_assert_held(&cscfg_mutex);

	 
	list_for_each_entry(csdev_item, &cscfg_mgr->csdev_desc_list, item) {
		 
		cscfg_remove_owned_csdev_configs(csdev_item->csdev, load_owner);
		cscfg_remove_owned_csdev_features(csdev_item->csdev, load_owner);
	}

	 
	list_for_each_entry_safe(config_desc, cfg_tmp, &cscfg_mgr->config_desc_list, item) {
		if (config_desc->load_owner == load_owner) {
			etm_perf_del_symlink_cscfg(config_desc);
			list_del(&config_desc->item);
		}
	}

	 
	list_for_each_entry_safe(feat_desc, feat_tmp, &cscfg_mgr->feat_desc_list, item) {
		if (feat_desc->load_owner == load_owner) {
			list_del(&feat_desc->item);
		}
	}
}

 
static int cscfg_load_owned_cfgs_feats(struct cscfg_config_desc **config_descs,
				       struct cscfg_feature_desc **feat_descs,
				       struct cscfg_load_owner_info *owner_info)
{
	int i, err;

	lockdep_assert_held(&cscfg_mutex);

	 
	if (feat_descs) {
		for (i = 0; feat_descs[i]; i++) {
			err = cscfg_load_feat(feat_descs[i]);
			if (err) {
				pr_err("coresight-syscfg: Failed to load feature %s\n",
				       feat_descs[i]->name);
				return err;
			}
			feat_descs[i]->load_owner = owner_info;
		}
	}

	 
	if (config_descs) {
		for (i = 0; config_descs[i]; i++) {
			err = cscfg_load_config(config_descs[i]);
			if (err) {
				pr_err("coresight-syscfg: Failed to load configuration %s\n",
				       config_descs[i]->name);
				return err;
			}
			config_descs[i]->load_owner = owner_info;
			config_descs[i]->available = false;
		}
	}
	return 0;
}

 
static void cscfg_set_configs_available(struct cscfg_config_desc **config_descs)
{
	int i;

	lockdep_assert_held(&cscfg_mutex);

	if (config_descs) {
		for (i = 0; config_descs[i]; i++)
			config_descs[i]->available = true;
	}
}

 
static int cscfg_fs_register_cfgs_feats(struct cscfg_config_desc **config_descs,
					struct cscfg_feature_desc **feat_descs)
{
	int i, err;

	if (feat_descs) {
		for (i = 0; feat_descs[i]; i++) {
			err = cscfg_configfs_add_feature(feat_descs[i]);
			if (err)
				return err;
		}
	}
	if (config_descs) {
		for (i = 0; config_descs[i]; i++) {
			err = cscfg_configfs_add_config(config_descs[i]);
			if (err)
				return err;
		}
	}
	return 0;
}

 
int cscfg_load_config_sets(struct cscfg_config_desc **config_descs,
			   struct cscfg_feature_desc **feat_descs,
			   struct cscfg_load_owner_info *owner_info)
{
	int err = 0;

	mutex_lock(&cscfg_mutex);
	if (cscfg_mgr->load_state != CSCFG_NONE) {
		mutex_unlock(&cscfg_mutex);
		return -EBUSY;
	}
	cscfg_mgr->load_state = CSCFG_LOAD;

	 
	err = cscfg_load_owned_cfgs_feats(config_descs, feat_descs, owner_info);
	if (err)
		goto err_clean_load;

	 
	list_add_tail(&owner_info->item, &cscfg_mgr->load_order_list);
	if (!list_is_singular(&cscfg_mgr->load_order_list)) {
		 
		err = cscfg_owner_get(list_prev_entry(owner_info, item));
		if (err)
			goto err_clean_owner_list;
	}

	 
	mutex_unlock(&cscfg_mutex);

	 
	err = cscfg_fs_register_cfgs_feats(config_descs, feat_descs);
	mutex_lock(&cscfg_mutex);

	if (err)
		goto err_clean_cfs;

	 
	cscfg_set_configs_available(config_descs);
	goto exit_unlock;

err_clean_cfs:
	 
	cscfg_fs_unregister_cfgs_feats(owner_info);

	if (!list_is_singular(&cscfg_mgr->load_order_list))
		cscfg_owner_put(list_prev_entry(owner_info, item));

err_clean_owner_list:
	list_del(&owner_info->item);

err_clean_load:
	cscfg_unload_owned_cfgs_feats(owner_info);

exit_unlock:
	cscfg_mgr->load_state = CSCFG_NONE;
	mutex_unlock(&cscfg_mutex);
	return err;
}
EXPORT_SYMBOL_GPL(cscfg_load_config_sets);

 
int cscfg_unload_config_sets(struct cscfg_load_owner_info *owner_info)
{
	int err = 0;
	struct cscfg_load_owner_info *load_list_item = NULL;

	mutex_lock(&cscfg_mutex);
	if (cscfg_mgr->load_state != CSCFG_NONE) {
		mutex_unlock(&cscfg_mutex);
		return -EBUSY;
	}

	 
	cscfg_mgr->load_state = CSCFG_UNLOAD;

	 
	if (atomic_read(&cscfg_mgr->sys_active_cnt)) {
		err = -EBUSY;
		goto exit_unlock;
	}

	 
	if (!list_empty(&cscfg_mgr->load_order_list)) {
		load_list_item = list_last_entry(&cscfg_mgr->load_order_list,
						 struct cscfg_load_owner_info, item);
		if (load_list_item != owner_info)
			load_list_item = NULL;
	}

	if (!load_list_item) {
		err = -EINVAL;
		goto exit_unlock;
	}

	 
	mutex_unlock(&cscfg_mutex);
	cscfg_fs_unregister_cfgs_feats(owner_info);
	mutex_lock(&cscfg_mutex);

	 
	cscfg_unload_owned_cfgs_feats(owner_info);

	 
	if (!list_is_singular(&cscfg_mgr->load_order_list)) {
		 
		cscfg_owner_put(list_prev_entry(owner_info, item));
	}
	list_del(&owner_info->item);

exit_unlock:
	cscfg_mgr->load_state = CSCFG_NONE;
	mutex_unlock(&cscfg_mutex);
	return err;
}
EXPORT_SYMBOL_GPL(cscfg_unload_config_sets);

 

 
static int cscfg_add_cfgs_csdev(struct coresight_device *csdev)
{
	struct cscfg_config_desc *config_desc;
	int err = 0;

	list_for_each_entry(config_desc, &cscfg_mgr->config_desc_list, item) {
		err = cscfg_add_csdev_cfg(csdev, config_desc);
		if (err)
			break;
	}
	return err;
}

 
static int cscfg_add_feats_csdev(struct coresight_device *csdev,
				 u32 match_flags,
				 struct cscfg_csdev_feat_ops *ops)
{
	struct cscfg_feature_desc *feat_desc;
	int err = 0;

	if (!ops->load_feat)
		return -EINVAL;

	list_for_each_entry(feat_desc, &cscfg_mgr->feat_desc_list, item) {
		if (feat_desc->match_flags & match_flags) {
			err = cscfg_load_feat_csdev(csdev, feat_desc, ops);
			if (err)
				break;
		}
	}
	return err;
}

 
static int cscfg_list_add_csdev(struct coresight_device *csdev,
				u32 match_flags,
				struct cscfg_csdev_feat_ops *ops)
{
	struct cscfg_registered_csdev *csdev_item;

	 
	csdev_item = kzalloc(sizeof(struct cscfg_registered_csdev), GFP_KERNEL);
	if (!csdev_item)
		return -ENOMEM;

	csdev_item->csdev = csdev;
	csdev_item->match_flags = match_flags;
	csdev_item->ops.load_feat = ops->load_feat;
	list_add(&csdev_item->item, &cscfg_mgr->csdev_desc_list);

	INIT_LIST_HEAD(&csdev->feature_csdev_list);
	INIT_LIST_HEAD(&csdev->config_csdev_list);
	spin_lock_init(&csdev->cscfg_csdev_lock);

	return 0;
}

 
static void cscfg_list_remove_csdev(struct coresight_device *csdev)
{
	struct cscfg_registered_csdev *csdev_item, *tmp;

	list_for_each_entry_safe(csdev_item, tmp, &cscfg_mgr->csdev_desc_list, item) {
		if (csdev_item->csdev == csdev) {
			list_del(&csdev_item->item);
			kfree(csdev_item);
			break;
		}
	}
}

 
int cscfg_register_csdev(struct coresight_device *csdev,
			 u32 match_flags,
			 struct cscfg_csdev_feat_ops *ops)
{
	int ret = 0;

	mutex_lock(&cscfg_mutex);

	 
	ret = cscfg_list_add_csdev(csdev, match_flags, ops);
	if (ret)
		goto reg_csdev_unlock;

	 
	ret = cscfg_add_feats_csdev(csdev, match_flags, ops);
	if (ret) {
		cscfg_list_remove_csdev(csdev);
		goto reg_csdev_unlock;
	}

	ret = cscfg_add_cfgs_csdev(csdev);
	if (ret) {
		cscfg_list_remove_csdev(csdev);
		goto reg_csdev_unlock;
	}

	pr_info("CSCFG registered %s", dev_name(&csdev->dev));

reg_csdev_unlock:
	mutex_unlock(&cscfg_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(cscfg_register_csdev);

 
void cscfg_unregister_csdev(struct coresight_device *csdev)
{
	mutex_lock(&cscfg_mutex);
	cscfg_list_remove_csdev(csdev);
	mutex_unlock(&cscfg_mutex);
}
EXPORT_SYMBOL_GPL(cscfg_unregister_csdev);

 
void cscfg_csdev_reset_feats(struct coresight_device *csdev)
{
	struct cscfg_feature_csdev *feat_csdev;
	unsigned long flags;

	spin_lock_irqsave(&csdev->cscfg_csdev_lock, flags);
	if (list_empty(&csdev->feature_csdev_list))
		goto unlock_exit;

	list_for_each_entry(feat_csdev, &csdev->feature_csdev_list, node)
		cscfg_reset_feat(feat_csdev);

unlock_exit:
	spin_unlock_irqrestore(&csdev->cscfg_csdev_lock, flags);
}
EXPORT_SYMBOL_GPL(cscfg_csdev_reset_feats);

 
static int _cscfg_activate_config(unsigned long cfg_hash)
{
	struct cscfg_config_desc *config_desc;
	int err = -EINVAL;

	if (cscfg_mgr->load_state == CSCFG_UNLOAD)
		return -EBUSY;

	list_for_each_entry(config_desc, &cscfg_mgr->config_desc_list, item) {
		if ((unsigned long)config_desc->event_ea->var == cfg_hash) {
			 
			if (config_desc->available == false)
				return -EBUSY;

			 
			err = cscfg_owner_get(config_desc->load_owner);
			if (err)
				break;
			 
			atomic_inc(&cscfg_mgr->sys_active_cnt);

			 
			atomic_inc(&config_desc->active_cnt);

			err = 0;
			dev_dbg(cscfg_device(), "Activate config %s.\n", config_desc->name);
			break;
		}
	}
	return err;
}

static void _cscfg_deactivate_config(unsigned long cfg_hash)
{
	struct cscfg_config_desc *config_desc;

	list_for_each_entry(config_desc, &cscfg_mgr->config_desc_list, item) {
		if ((unsigned long)config_desc->event_ea->var == cfg_hash) {
			atomic_dec(&config_desc->active_cnt);
			atomic_dec(&cscfg_mgr->sys_active_cnt);
			cscfg_owner_put(config_desc->load_owner);
			dev_dbg(cscfg_device(), "Deactivate config %s.\n", config_desc->name);
			break;
		}
	}
}

 
int cscfg_config_sysfs_activate(struct cscfg_config_desc *config_desc, bool activate)
{
	unsigned long cfg_hash;
	int err = 0;

	mutex_lock(&cscfg_mutex);

	cfg_hash = (unsigned long)config_desc->event_ea->var;

	if (activate) {
		 
		if (cscfg_mgr->sysfs_active_config) {
			err = -EBUSY;
			goto exit_unlock;
		}
		err = _cscfg_activate_config(cfg_hash);
		if (!err)
			cscfg_mgr->sysfs_active_config = cfg_hash;
	} else {
		 
		if (cscfg_mgr->sysfs_active_config == cfg_hash) {
			_cscfg_deactivate_config(cfg_hash);
			cscfg_mgr->sysfs_active_config = 0;
		} else
			err = -EINVAL;
	}

exit_unlock:
	mutex_unlock(&cscfg_mutex);
	return err;
}

 
void cscfg_config_sysfs_set_preset(int preset)
{
	mutex_lock(&cscfg_mutex);
	cscfg_mgr->sysfs_active_preset = preset;
	mutex_unlock(&cscfg_mutex);
}

 
void cscfg_config_sysfs_get_active_cfg(unsigned long *cfg_hash, int *preset)
{
	mutex_lock(&cscfg_mutex);
	*preset = cscfg_mgr->sysfs_active_preset;
	*cfg_hash = cscfg_mgr->sysfs_active_config;
	mutex_unlock(&cscfg_mutex);
}
EXPORT_SYMBOL_GPL(cscfg_config_sysfs_get_active_cfg);

 
int cscfg_activate_config(unsigned long cfg_hash)
{
	int err = 0;

	mutex_lock(&cscfg_mutex);
	err = _cscfg_activate_config(cfg_hash);
	mutex_unlock(&cscfg_mutex);

	return err;
}
EXPORT_SYMBOL_GPL(cscfg_activate_config);

 
void cscfg_deactivate_config(unsigned long cfg_hash)
{
	mutex_lock(&cscfg_mutex);
	_cscfg_deactivate_config(cfg_hash);
	mutex_unlock(&cscfg_mutex);
}
EXPORT_SYMBOL_GPL(cscfg_deactivate_config);

 
int cscfg_csdev_enable_active_config(struct coresight_device *csdev,
				     unsigned long cfg_hash, int preset)
{
	struct cscfg_config_csdev *config_csdev_active = NULL, *config_csdev_item;
	const struct cscfg_config_desc *config_desc;
	unsigned long flags;
	int err = 0;

	 
	if (!atomic_read(&cscfg_mgr->sys_active_cnt))
		return 0;

	 
	spin_lock_irqsave(&csdev->cscfg_csdev_lock, flags);
	list_for_each_entry(config_csdev_item, &csdev->config_csdev_list, node) {
		config_desc = config_csdev_item->config_desc;
		if ((atomic_read(&config_desc->active_cnt)) &&
		    ((unsigned long)config_desc->event_ea->var == cfg_hash)) {
			config_csdev_active = config_csdev_item;
			csdev->active_cscfg_ctxt = (void *)config_csdev_active;
			break;
		}
	}
	spin_unlock_irqrestore(&csdev->cscfg_csdev_lock, flags);

	 
	if (config_csdev_active) {
		 
		err = cscfg_csdev_enable_config(config_csdev_active, preset);
		if (!err) {
			 
			spin_lock_irqsave(&csdev->cscfg_csdev_lock, flags);
			if (csdev->active_cscfg_ctxt)
				config_csdev_active->enabled = true;
			else
				err = -EBUSY;
			spin_unlock_irqrestore(&csdev->cscfg_csdev_lock, flags);
		}
	}
	return err;
}
EXPORT_SYMBOL_GPL(cscfg_csdev_enable_active_config);

 
void cscfg_csdev_disable_active_config(struct coresight_device *csdev)
{
	struct cscfg_config_csdev *config_csdev;
	unsigned long flags;

	 
	spin_lock_irqsave(&csdev->cscfg_csdev_lock, flags);
	config_csdev = (struct cscfg_config_csdev *)csdev->active_cscfg_ctxt;
	if (config_csdev) {
		if (!config_csdev->enabled)
			config_csdev = NULL;
		else
			config_csdev->enabled = false;
	}
	csdev->active_cscfg_ctxt = NULL;
	spin_unlock_irqrestore(&csdev->cscfg_csdev_lock, flags);

	 
	if (config_csdev)
		cscfg_csdev_disable_config(config_csdev);
}
EXPORT_SYMBOL_GPL(cscfg_csdev_disable_active_config);

 

struct device *cscfg_device(void)
{
	return cscfg_mgr ? &cscfg_mgr->dev : NULL;
}

 
static void cscfg_dev_release(struct device *dev)
{
	mutex_lock(&cscfg_mutex);
	kfree(cscfg_mgr);
	cscfg_mgr = NULL;
	mutex_unlock(&cscfg_mutex);
}

 
static int cscfg_create_device(void)
{
	struct device *dev;
	int err = -ENOMEM;

	mutex_lock(&cscfg_mutex);
	if (cscfg_mgr) {
		err = -EINVAL;
		goto create_dev_exit_unlock;
	}

	cscfg_mgr = kzalloc(sizeof(struct cscfg_manager), GFP_KERNEL);
	if (!cscfg_mgr)
		goto create_dev_exit_unlock;

	 
	INIT_LIST_HEAD(&cscfg_mgr->csdev_desc_list);
	INIT_LIST_HEAD(&cscfg_mgr->feat_desc_list);
	INIT_LIST_HEAD(&cscfg_mgr->config_desc_list);
	INIT_LIST_HEAD(&cscfg_mgr->load_order_list);
	atomic_set(&cscfg_mgr->sys_active_cnt, 0);
	cscfg_mgr->load_state = CSCFG_NONE;

	 
	dev = cscfg_device();
	dev->release = cscfg_dev_release;
	dev->init_name = "cs_system_cfg";

	err = device_register(dev);
	if (err)
		put_device(dev);

create_dev_exit_unlock:
	mutex_unlock(&cscfg_mutex);
	return err;
}

 
static void cscfg_unload_cfgs_on_exit(void)
{
	struct cscfg_load_owner_info *owner_info = NULL;

	 
	mutex_lock(&cscfg_mutex);
	while (!list_empty(&cscfg_mgr->load_order_list)) {

		 
		owner_info = list_last_entry(&cscfg_mgr->load_order_list,
					     struct cscfg_load_owner_info, item);

		 
		switch (owner_info->type) {
		case CSCFG_OWNER_PRELOAD:
			 
			pr_info("cscfg: unloading preloaded configurations\n");
			break;

		case  CSCFG_OWNER_MODULE:
			 
			pr_err("cscfg: ERROR: prior module failed to unload configuration\n");
			goto list_remove;
		}

		 
		mutex_unlock(&cscfg_mutex);
		cscfg_fs_unregister_cfgs_feats(owner_info);
		mutex_lock(&cscfg_mutex);

		 
		cscfg_unload_owned_cfgs_feats(owner_info);

list_remove:
		 
		list_del(&owner_info->item);
	}
	mutex_unlock(&cscfg_mutex);
}

static void cscfg_clear_device(void)
{
	cscfg_unload_cfgs_on_exit();
	cscfg_configfs_release(cscfg_mgr);
	device_unregister(cscfg_device());
}

 
int __init cscfg_init(void)
{
	int err = 0;

	 
	err = cscfg_create_device();
	if (err)
		return err;

	 
	err = cscfg_configfs_init(cscfg_mgr);
	if (err)
		goto exit_err;

	 
	err = cscfg_preload(THIS_MODULE);
	if (err)
		goto exit_err;

	dev_info(cscfg_device(), "CoreSight Configuration manager initialised");
	return 0;

exit_err:
	cscfg_clear_device();
	return err;
}

void cscfg_exit(void)
{
	cscfg_clear_device();
}
