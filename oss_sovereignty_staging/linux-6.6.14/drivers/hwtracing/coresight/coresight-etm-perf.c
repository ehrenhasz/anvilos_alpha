
 

#include <linux/bitfield.h>
#include <linux/coresight.h>
#include <linux/coresight-pmu.h>
#include <linux/cpumask.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/perf_event.h>
#include <linux/percpu-defs.h>
#include <linux/slab.h>
#include <linux/stringhash.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#include "coresight-config.h"
#include "coresight-etm-perf.h"
#include "coresight-priv.h"
#include "coresight-syscfg.h"
#include "coresight-trace-id.h"

static struct pmu etm_pmu;
static bool etm_perf_up;

 
struct etm_ctxt {
	struct perf_output_handle handle;
	struct etm_event_data *event_data;
};

static DEFINE_PER_CPU(struct etm_ctxt, etm_ctxt);
static DEFINE_PER_CPU(struct coresight_device *, csdev_src);

 
PMU_FORMAT_ATTR(branch_broadcast, "config:"__stringify(ETM_OPT_BRANCH_BROADCAST));
PMU_FORMAT_ATTR(cycacc,		"config:" __stringify(ETM_OPT_CYCACC));
 
PMU_FORMAT_ATTR(contextid1,	"config:" __stringify(ETM_OPT_CTXTID));
 
PMU_FORMAT_ATTR(contextid2,	"config:" __stringify(ETM_OPT_CTXTID2));
PMU_FORMAT_ATTR(timestamp,	"config:" __stringify(ETM_OPT_TS));
PMU_FORMAT_ATTR(retstack,	"config:" __stringify(ETM_OPT_RETSTK));
 
PMU_FORMAT_ATTR(preset,		"config:0-3");
 
PMU_FORMAT_ATTR(sinkid,		"config2:0-31");
 
PMU_FORMAT_ATTR(configid,	"config2:32-63");


 
static ssize_t format_attr_contextid_show(struct device *dev,
					  struct device_attribute *attr,
					  char *page)
{
	int pid_fmt = ETM_OPT_CTXTID;

#if IS_ENABLED(CONFIG_CORESIGHT_SOURCE_ETM4X)
	pid_fmt = is_kernel_in_hyp_mode() ? ETM_OPT_CTXTID2 : ETM_OPT_CTXTID;
#endif
	return sprintf(page, "config:%d\n", pid_fmt);
}

static struct device_attribute format_attr_contextid =
	__ATTR(contextid, 0444, format_attr_contextid_show, NULL);

static struct attribute *etm_config_formats_attr[] = {
	&format_attr_cycacc.attr,
	&format_attr_contextid.attr,
	&format_attr_contextid1.attr,
	&format_attr_contextid2.attr,
	&format_attr_timestamp.attr,
	&format_attr_retstack.attr,
	&format_attr_sinkid.attr,
	&format_attr_preset.attr,
	&format_attr_configid.attr,
	&format_attr_branch_broadcast.attr,
	NULL,
};

static const struct attribute_group etm_pmu_format_group = {
	.name   = "format",
	.attrs  = etm_config_formats_attr,
};

static struct attribute *etm_config_sinks_attr[] = {
	NULL,
};

static const struct attribute_group etm_pmu_sinks_group = {
	.name   = "sinks",
	.attrs  = etm_config_sinks_attr,
};

static struct attribute *etm_config_events_attr[] = {
	NULL,
};

static const struct attribute_group etm_pmu_events_group = {
	.name   = "events",
	.attrs  = etm_config_events_attr,
};

static const struct attribute_group *etm_pmu_attr_groups[] = {
	&etm_pmu_format_group,
	&etm_pmu_sinks_group,
	&etm_pmu_events_group,
	NULL,
};

static inline struct list_head **
etm_event_cpu_path_ptr(struct etm_event_data *data, int cpu)
{
	return per_cpu_ptr(data->path, cpu);
}

static inline struct list_head *
etm_event_cpu_path(struct etm_event_data *data, int cpu)
{
	return *etm_event_cpu_path_ptr(data, cpu);
}

static void etm_event_read(struct perf_event *event) {}

static int etm_addr_filters_alloc(struct perf_event *event)
{
	struct etm_filters *filters;
	int node = event->cpu == -1 ? -1 : cpu_to_node(event->cpu);

	filters = kzalloc_node(sizeof(struct etm_filters), GFP_KERNEL, node);
	if (!filters)
		return -ENOMEM;

	if (event->parent)
		memcpy(filters, event->parent->hw.addr_filters,
		       sizeof(*filters));

	event->hw.addr_filters = filters;

	return 0;
}

static void etm_event_destroy(struct perf_event *event)
{
	kfree(event->hw.addr_filters);
	event->hw.addr_filters = NULL;
}

static int etm_event_init(struct perf_event *event)
{
	int ret = 0;

	if (event->attr.type != etm_pmu.type) {
		ret = -ENOENT;
		goto out;
	}

	ret = etm_addr_filters_alloc(event);
	if (ret)
		goto out;

	event->destroy = etm_event_destroy;
out:
	return ret;
}

static void free_sink_buffer(struct etm_event_data *event_data)
{
	int cpu;
	cpumask_t *mask = &event_data->mask;
	struct coresight_device *sink;

	if (!event_data->snk_config)
		return;

	if (WARN_ON(cpumask_empty(mask)))
		return;

	cpu = cpumask_first(mask);
	sink = coresight_get_sink(etm_event_cpu_path(event_data, cpu));
	sink_ops(sink)->free_buffer(event_data->snk_config);
}

static void free_event_data(struct work_struct *work)
{
	int cpu;
	cpumask_t *mask;
	struct etm_event_data *event_data;

	event_data = container_of(work, struct etm_event_data, work);
	mask = &event_data->mask;

	 
	free_sink_buffer(event_data);

	 
	if (event_data->cfg_hash)
		cscfg_deactivate_config(event_data->cfg_hash);

	for_each_cpu(cpu, mask) {
		struct list_head **ppath;

		ppath = etm_event_cpu_path_ptr(event_data, cpu);
		if (!(IS_ERR_OR_NULL(*ppath)))
			coresight_release_path(*ppath);
		*ppath = NULL;
		coresight_trace_id_put_cpu_id(cpu);
	}

	 
	coresight_trace_id_perf_stop();

	free_percpu(event_data->path);
	kfree(event_data);
}

static void *alloc_event_data(int cpu)
{
	cpumask_t *mask;
	struct etm_event_data *event_data;

	 
	event_data = kzalloc(sizeof(struct etm_event_data), GFP_KERNEL);
	if (!event_data)
		return NULL;


	mask = &event_data->mask;
	if (cpu != -1)
		cpumask_set_cpu(cpu, mask);
	else
		cpumask_copy(mask, cpu_present_mask);

	 
	event_data->path = alloc_percpu(struct list_head *);

	if (!event_data->path) {
		kfree(event_data);
		return NULL;
	}

	return event_data;
}

static void etm_free_aux(void *data)
{
	struct etm_event_data *event_data = data;

	schedule_work(&event_data->work);
}

 
static bool sinks_compatible(struct coresight_device *a,
			     struct coresight_device *b)
{
	if (!a || !b)
		return false;
	 
	return (a->subtype.sink_subtype == b->subtype.sink_subtype) &&
	       (sink_ops(a) == sink_ops(b));
}

static void *etm_setup_aux(struct perf_event *event, void **pages,
			   int nr_pages, bool overwrite)
{
	u32 id, cfg_hash;
	int cpu = event->cpu;
	int trace_id;
	cpumask_t *mask;
	struct coresight_device *sink = NULL;
	struct coresight_device *user_sink = NULL, *last_sink = NULL;
	struct etm_event_data *event_data = NULL;

	event_data = alloc_event_data(cpu);
	if (!event_data)
		return NULL;
	INIT_WORK(&event_data->work, free_event_data);

	 
	if (event->attr.config2 & GENMASK_ULL(31, 0)) {
		id = (u32)event->attr.config2;
		sink = user_sink = coresight_get_sink_by_id(id);
	}

	 
	coresight_trace_id_perf_start();

	 
	cfg_hash = (u32)((event->attr.config2 & GENMASK_ULL(63, 32)) >> 32);
	if (cfg_hash) {
		if (cscfg_activate_config(cfg_hash))
			goto err;
		event_data->cfg_hash = cfg_hash;
	}

	mask = &event_data->mask;

	 
	for_each_cpu(cpu, mask) {
		struct list_head *path;
		struct coresight_device *csdev;

		csdev = per_cpu(csdev_src, cpu);
		 
		if (!csdev) {
			cpumask_clear_cpu(cpu, mask);
			continue;
		}

		 
		if (!user_sink) {
			 
			sink = coresight_find_default_sink(csdev);
			if (!sink) {
				cpumask_clear_cpu(cpu, mask);
				continue;
			}

			 
			if (last_sink && !sinks_compatible(last_sink, sink)) {
				cpumask_clear_cpu(cpu, mask);
				continue;
			}
			last_sink = sink;
		}

		 
		path = coresight_build_path(csdev, sink);
		if (IS_ERR(path)) {
			cpumask_clear_cpu(cpu, mask);
			continue;
		}

		 
		trace_id = coresight_trace_id_get_cpu_id(cpu);
		if (!IS_VALID_CS_TRACE_ID(trace_id)) {
			cpumask_clear_cpu(cpu, mask);
			coresight_release_path(path);
			continue;
		}

		*etm_event_cpu_path_ptr(event_data, cpu) = path;
	}

	 
	if (!sink)
		goto err;

	 
	cpu = cpumask_first(mask);
	if (cpu >= nr_cpu_ids)
		goto err;

	if (!sink_ops(sink)->alloc_buffer || !sink_ops(sink)->free_buffer)
		goto err;

	 
	event_data->snk_config =
			sink_ops(sink)->alloc_buffer(sink, event, pages,
						     nr_pages, overwrite);
	if (!event_data->snk_config)
		goto err;

out:
	return event_data;

err:
	etm_free_aux(event_data);
	event_data = NULL;
	goto out;
}

static void etm_event_start(struct perf_event *event, int flags)
{
	int cpu = smp_processor_id();
	struct etm_event_data *event_data;
	struct etm_ctxt *ctxt = this_cpu_ptr(&etm_ctxt);
	struct perf_output_handle *handle = &ctxt->handle;
	struct coresight_device *sink, *csdev = per_cpu(csdev_src, cpu);
	struct list_head *path;
	u64 hw_id;

	if (!csdev)
		goto fail;

	 
	if (WARN_ON(ctxt->event_data))
		goto fail;

	 
	event_data = perf_aux_output_begin(handle, event);
	if (!event_data)
		goto fail;

	 
	if (!cpumask_test_cpu(cpu, &event_data->mask))
		goto out;

	path = etm_event_cpu_path(event_data, cpu);
	 
	sink = coresight_get_sink(path);
	if (WARN_ON_ONCE(!sink))
		goto fail_end_stop;

	 
	if (coresight_enable_path(path, CS_MODE_PERF, handle))
		goto fail_end_stop;

	 
	if (source_ops(csdev)->enable(csdev, event, CS_MODE_PERF))
		goto fail_disable_path;

	 
	if (!cpumask_test_cpu(cpu, &event_data->aux_hwid_done)) {
		cpumask_set_cpu(cpu, &event_data->aux_hwid_done);
		hw_id = FIELD_PREP(CS_AUX_HW_ID_VERSION_MASK,
				   CS_AUX_HW_ID_CURR_VERSION);
		hw_id |= FIELD_PREP(CS_AUX_HW_ID_TRACE_ID_MASK,
				    coresight_trace_id_read_cpu_id(cpu));
		perf_report_aux_output_id(event, hw_id);
	}

out:
	 
	event->hw.state = 0;
	 
	ctxt->event_data = event_data;
	return;

fail_disable_path:
	coresight_disable_path(path);
fail_end_stop:
	 
	if (READ_ONCE(handle->event)) {
		perf_aux_output_flag(handle, PERF_AUX_FLAG_TRUNCATED);
		perf_aux_output_end(handle, 0);
	}
fail:
	event->hw.state = PERF_HES_STOPPED;
	return;
}

static void etm_event_stop(struct perf_event *event, int mode)
{
	int cpu = smp_processor_id();
	unsigned long size;
	struct coresight_device *sink, *csdev = per_cpu(csdev_src, cpu);
	struct etm_ctxt *ctxt = this_cpu_ptr(&etm_ctxt);
	struct perf_output_handle *handle = &ctxt->handle;
	struct etm_event_data *event_data;
	struct list_head *path;

	 
	if (handle->event &&
	    WARN_ON(perf_get_aux(handle) != ctxt->event_data))
		return;

	event_data = ctxt->event_data;
	 
	ctxt->event_data = NULL;

	if (event->hw.state == PERF_HES_STOPPED)
		return;

	 
	if (WARN_ON(!event_data))
		return;

	 
	if (handle->event && (mode & PERF_EF_UPDATE) &&
	    !cpumask_test_cpu(cpu, &event_data->mask)) {
		event->hw.state = PERF_HES_STOPPED;
		perf_aux_output_end(handle, 0);
		return;
	}

	if (!csdev)
		return;

	path = etm_event_cpu_path(event_data, cpu);
	if (!path)
		return;

	sink = coresight_get_sink(path);
	if (!sink)
		return;

	 
	source_ops(csdev)->disable(csdev, event);

	 
	event->hw.state = PERF_HES_STOPPED;

	 
	if (handle->event && (mode & PERF_EF_UPDATE)) {
		if (WARN_ON_ONCE(handle->event != event))
			return;

		 
		if (!sink_ops(sink)->update_buffer)
			return;

		size = sink_ops(sink)->update_buffer(sink, handle,
					      event_data->snk_config);
		 
		if (READ_ONCE(handle->event))
			perf_aux_output_end(handle, size);
		else
			WARN_ON(size);
	}

	 
	coresight_disable_path(path);
}

static int etm_event_add(struct perf_event *event, int mode)
{
	int ret = 0;
	struct hw_perf_event *hwc = &event->hw;

	if (mode & PERF_EF_START) {
		etm_event_start(event, 0);
		if (hwc->state & PERF_HES_STOPPED)
			ret = -EINVAL;
	} else {
		hwc->state = PERF_HES_STOPPED;
	}

	return ret;
}

static void etm_event_del(struct perf_event *event, int mode)
{
	etm_event_stop(event, PERF_EF_UPDATE);
}

static int etm_addr_filters_validate(struct list_head *filters)
{
	bool range = false, address = false;
	int index = 0;
	struct perf_addr_filter *filter;

	list_for_each_entry(filter, filters, entry) {
		 
		if (++index > ETM_ADDR_CMP_MAX)
			return -EOPNOTSUPP;

		 
		if (filter->size) {
			 
			if (filter->action == PERF_ADDR_FILTER_ACTION_START ||
			    filter->action == PERF_ADDR_FILTER_ACTION_STOP)
				return -EOPNOTSUPP;

			range = true;
		} else
			address = true;

		 
		if (range && address)
			return -EOPNOTSUPP;
	}

	return 0;
}

static void etm_addr_filters_sync(struct perf_event *event)
{
	struct perf_addr_filters_head *head = perf_event_addr_filters(event);
	unsigned long start, stop;
	struct perf_addr_filter_range *fr = event->addr_filter_ranges;
	struct etm_filters *filters = event->hw.addr_filters;
	struct etm_filter *etm_filter;
	struct perf_addr_filter *filter;
	int i = 0;

	list_for_each_entry(filter, &head->list, entry) {
		start = fr[i].start;
		stop = start + fr[i].size;
		etm_filter = &filters->etm_filter[i];

		switch (filter->action) {
		case PERF_ADDR_FILTER_ACTION_FILTER:
			etm_filter->start_addr = start;
			etm_filter->stop_addr = stop;
			etm_filter->type = ETM_ADDR_TYPE_RANGE;
			break;
		case PERF_ADDR_FILTER_ACTION_START:
			etm_filter->start_addr = start;
			etm_filter->type = ETM_ADDR_TYPE_START;
			break;
		case PERF_ADDR_FILTER_ACTION_STOP:
			etm_filter->stop_addr = stop;
			etm_filter->type = ETM_ADDR_TYPE_STOP;
			break;
		}
		i++;
	}

	filters->nr_filters = i;
}

int etm_perf_symlink(struct coresight_device *csdev, bool link)
{
	char entry[sizeof("cpu9999999")];
	int ret = 0, cpu = source_ops(csdev)->cpu_id(csdev);
	struct device *pmu_dev = etm_pmu.dev;
	struct device *cs_dev = &csdev->dev;

	sprintf(entry, "cpu%d", cpu);

	if (!etm_perf_up)
		return -EPROBE_DEFER;

	if (link) {
		ret = sysfs_create_link(&pmu_dev->kobj, &cs_dev->kobj, entry);
		if (ret)
			return ret;
		per_cpu(csdev_src, cpu) = csdev;
	} else {
		sysfs_remove_link(&pmu_dev->kobj, entry);
		per_cpu(csdev_src, cpu) = NULL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(etm_perf_symlink);

static ssize_t etm_perf_sink_name_show(struct device *dev,
				       struct device_attribute *dattr,
				       char *buf)
{
	struct dev_ext_attribute *ea;

	ea = container_of(dattr, struct dev_ext_attribute, attr);
	return scnprintf(buf, PAGE_SIZE, "0x%lx\n", (unsigned long)(ea->var));
}

static struct dev_ext_attribute *
etm_perf_add_symlink_group(struct device *dev, const char *name, const char *group_name)
{
	struct dev_ext_attribute *ea;
	unsigned long hash;
	int ret;
	struct device *pmu_dev = etm_pmu.dev;

	if (!etm_perf_up)
		return ERR_PTR(-EPROBE_DEFER);

	ea = devm_kzalloc(dev, sizeof(*ea), GFP_KERNEL);
	if (!ea)
		return ERR_PTR(-ENOMEM);

	 
	hash = hashlen_hash(hashlen_string(NULL, name));

	sysfs_attr_init(&ea->attr.attr);
	ea->attr.attr.name = devm_kstrdup(dev, name, GFP_KERNEL);
	if (!ea->attr.attr.name)
		return ERR_PTR(-ENOMEM);

	ea->attr.attr.mode = 0444;
	ea->var = (unsigned long *)hash;

	ret = sysfs_add_file_to_group(&pmu_dev->kobj,
				      &ea->attr.attr, group_name);

	return ret ? ERR_PTR(ret) : ea;
}

int etm_perf_add_symlink_sink(struct coresight_device *csdev)
{
	const char *name;
	struct device *dev = &csdev->dev;
	int err = 0;

	if (csdev->type != CORESIGHT_DEV_TYPE_SINK &&
	    csdev->type != CORESIGHT_DEV_TYPE_LINKSINK)
		return -EINVAL;

	if (csdev->ea != NULL)
		return -EINVAL;

	name = dev_name(dev);
	csdev->ea = etm_perf_add_symlink_group(dev, name, "sinks");
	if (IS_ERR(csdev->ea)) {
		err = PTR_ERR(csdev->ea);
		csdev->ea = NULL;
	} else
		csdev->ea->attr.show = etm_perf_sink_name_show;

	return err;
}

static void etm_perf_del_symlink_group(struct dev_ext_attribute *ea, const char *group_name)
{
	struct device *pmu_dev = etm_pmu.dev;

	sysfs_remove_file_from_group(&pmu_dev->kobj,
				     &ea->attr.attr, group_name);
}

void etm_perf_del_symlink_sink(struct coresight_device *csdev)
{
	if (csdev->type != CORESIGHT_DEV_TYPE_SINK &&
	    csdev->type != CORESIGHT_DEV_TYPE_LINKSINK)
		return;

	if (!csdev->ea)
		return;

	etm_perf_del_symlink_group(csdev->ea, "sinks");
	csdev->ea = NULL;
}

static ssize_t etm_perf_cscfg_event_show(struct device *dev,
					 struct device_attribute *dattr,
					 char *buf)
{
	struct dev_ext_attribute *ea;

	ea = container_of(dattr, struct dev_ext_attribute, attr);
	return scnprintf(buf, PAGE_SIZE, "configid=0x%lx\n", (unsigned long)(ea->var));
}

int etm_perf_add_symlink_cscfg(struct device *dev, struct cscfg_config_desc *config_desc)
{
	int err = 0;

	if (config_desc->event_ea != NULL)
		return 0;

	config_desc->event_ea = etm_perf_add_symlink_group(dev, config_desc->name, "events");

	 
	if (!IS_ERR(config_desc->event_ea))
		config_desc->event_ea->attr.show = etm_perf_cscfg_event_show;
	else {
		err = PTR_ERR(config_desc->event_ea);
		config_desc->event_ea = NULL;
	}

	return err;
}

void etm_perf_del_symlink_cscfg(struct cscfg_config_desc *config_desc)
{
	if (!config_desc->event_ea)
		return;

	etm_perf_del_symlink_group(config_desc->event_ea, "events");
	config_desc->event_ea = NULL;
}

int __init etm_perf_init(void)
{
	int ret;

	etm_pmu.capabilities		= (PERF_PMU_CAP_EXCLUSIVE |
					   PERF_PMU_CAP_ITRACE);

	etm_pmu.attr_groups		= etm_pmu_attr_groups;
	etm_pmu.task_ctx_nr		= perf_sw_context;
	etm_pmu.read			= etm_event_read;
	etm_pmu.event_init		= etm_event_init;
	etm_pmu.setup_aux		= etm_setup_aux;
	etm_pmu.free_aux		= etm_free_aux;
	etm_pmu.start			= etm_event_start;
	etm_pmu.stop			= etm_event_stop;
	etm_pmu.add			= etm_event_add;
	etm_pmu.del			= etm_event_del;
	etm_pmu.addr_filters_sync	= etm_addr_filters_sync;
	etm_pmu.addr_filters_validate	= etm_addr_filters_validate;
	etm_pmu.nr_addr_filters		= ETM_ADDR_CMP_MAX;
	etm_pmu.module			= THIS_MODULE;

	ret = perf_pmu_register(&etm_pmu, CORESIGHT_ETM_PMU_NAME, -1);
	if (ret == 0)
		etm_perf_up = true;

	return ret;
}

void etm_perf_exit(void)
{
	perf_pmu_unregister(&etm_pmu);
}
