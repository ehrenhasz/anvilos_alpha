


#ifndef _CORESIGHT_ETM_PERF_H
#define _CORESIGHT_ETM_PERF_H

#include <linux/percpu-defs.h>
#include "coresight-priv.h"

struct coresight_device;
struct cscfg_config_desc;


#define ETM_ADDR_CMP_MAX	8


struct etm_filter {
	unsigned long start_addr;
	unsigned long stop_addr;
	enum etm_addr_type type;
};


struct etm_filters {
	struct etm_filter	etm_filter[ETM_ADDR_CMP_MAX];
	unsigned int		nr_filters;
	bool			ssstatus;
};


struct etm_event_data {
	struct work_struct work;
	cpumask_t mask;
	cpumask_t aux_hwid_done;
	void *snk_config;
	u32 cfg_hash;
	struct list_head * __percpu *path;
};

#if IS_ENABLED(CONFIG_CORESIGHT)
int etm_perf_symlink(struct coresight_device *csdev, bool link);
int etm_perf_add_symlink_sink(struct coresight_device *csdev);
void etm_perf_del_symlink_sink(struct coresight_device *csdev);
static inline void *etm_perf_sink_config(struct perf_output_handle *handle)
{
	struct etm_event_data *data = perf_get_aux(handle);

	if (data)
		return data->snk_config;
	return NULL;
}
int etm_perf_add_symlink_cscfg(struct device *dev,
			       struct cscfg_config_desc *config_desc);
void etm_perf_del_symlink_cscfg(struct cscfg_config_desc *config_desc);
#else
static inline int etm_perf_symlink(struct coresight_device *csdev, bool link)
{ return -EINVAL; }
int etm_perf_add_symlink_sink(struct coresight_device *csdev)
{ return -EINVAL; }
void etm_perf_del_symlink_sink(struct coresight_device *csdev) {}
static inline void *etm_perf_sink_config(struct perf_output_handle *handle)
{
	return NULL;
}
int etm_perf_add_symlink_cscfg(struct device *dev,
			       struct cscfg_config_desc *config_desc)
{ return -EINVAL; }
void etm_perf_del_symlink_cscfg(struct cscfg_config_desc *config_desc) {}

#endif 

int __init etm_perf_init(void);
void etm_perf_exit(void);

#endif
