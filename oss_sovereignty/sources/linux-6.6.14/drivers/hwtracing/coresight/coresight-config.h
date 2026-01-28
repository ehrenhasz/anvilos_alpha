


#ifndef _CORESIGHT_CORESIGHT_CONFIG_H
#define _CORESIGHT_CORESIGHT_CONFIG_H

#include <linux/coresight.h>
#include <linux/types.h>




#define CS_CFG_REG_TYPE_STD		0x80	
#define CS_CFG_REG_TYPE_RESOURCE	0x40	
#define CS_CFG_REG_TYPE_VAL_PARAM	0x08	
#define CS_CFG_REG_TYPE_VAL_MASK	0x04	
#define CS_CFG_REG_TYPE_VAL_64BIT	0x02	
#define CS_CFG_REG_TYPE_VAL_SAVE	0x01	


#define CS_CFG_MATCH_CLASS_SRC_ALL	0x0001	
#define CS_CFG_MATCH_CLASS_SRC_ETM4	0x0002	


#define CS_CFG_MATCH_INST_ANY		0x80000000 


#define CS_CFG_CONFIG_PRESET_MAX 15


struct cscfg_parameter_desc {
	const char *name;
	u64 value;
};


struct cscfg_regval_desc {
	struct {
		u32 type:8;
		u32 offset:12;
		u32 hw_info:12;
	};
	union {
		u64 val64;
		struct {
			u32 val32;
			u32 mask32;
		};
		u32 param_idx;
	};
};


struct cscfg_feature_desc {
	const char *name;
	const char *description;
	struct list_head item;
	u32 match_flags;
	int nr_params;
	struct cscfg_parameter_desc *params_desc;
	int nr_regs;
	struct cscfg_regval_desc *regs_desc;
	void *load_owner;
	struct config_group *fs_group;
};


struct cscfg_config_desc {
	const char *name;
	const char *description;
	struct list_head item;
	int nr_feat_refs;
	const char **feat_ref_names;
	int nr_presets;
	int nr_total_params;
	const u64 *presets; 
	struct dev_ext_attribute *event_ea;
	atomic_t active_cnt;
	void *load_owner;
	struct config_group *fs_group;
	bool available;
};


struct cscfg_regval_csdev {
	struct cscfg_regval_desc reg_desc;
	void *driver_regval;
};


struct cscfg_parameter_csdev {
	struct cscfg_feature_csdev *feat_csdev;
	struct cscfg_regval_csdev *reg_csdev;
	u64 current_value;
	bool val64;
};


struct cscfg_feature_csdev {
	const struct cscfg_feature_desc *feat_desc;
	struct coresight_device *csdev;
	struct list_head node;
	spinlock_t *drv_spinlock;
	int nr_params;
	struct cscfg_parameter_csdev *params_csdev;
	int nr_regs;
	struct cscfg_regval_csdev *regs_csdev;
};


struct cscfg_config_csdev {
	const struct cscfg_config_desc *config_desc;
	struct coresight_device *csdev;
	bool enabled;
	struct list_head node;
	int nr_feat;
	struct cscfg_feature_csdev *feats_csdev[];
};


struct cscfg_csdev_feat_ops {
	int (*load_feat)(struct coresight_device *csdev,
			 struct cscfg_feature_csdev *feat_csdev);
};




int cscfg_csdev_enable_config(struct cscfg_config_csdev *config_csdev, int preset);
void cscfg_csdev_disable_config(struct cscfg_config_csdev *config_csdev);


void cscfg_reset_feat(struct cscfg_feature_csdev *feat_csdev);

#endif 
