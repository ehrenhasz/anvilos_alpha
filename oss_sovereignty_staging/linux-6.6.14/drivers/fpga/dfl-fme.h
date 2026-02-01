 
 

#ifndef __DFL_FME_H
#define __DFL_FME_H

 
struct dfl_fme {
	struct platform_device *mgr;
	struct list_head region_list;
	struct list_head bridge_list;
	struct dfl_feature_platform_data *pdata;
};

extern const struct dfl_feature_ops fme_pr_mgmt_ops;
extern const struct dfl_feature_id fme_pr_mgmt_id_table[];
extern const struct dfl_feature_ops fme_global_err_ops;
extern const struct dfl_feature_id fme_global_err_id_table[];
extern const struct attribute_group fme_global_err_group;
extern const struct dfl_feature_ops fme_perf_ops;
extern const struct dfl_feature_id fme_perf_id_table[];

#endif  
