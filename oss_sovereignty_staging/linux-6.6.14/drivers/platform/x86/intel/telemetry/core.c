
 
#include <linux/device.h>
#include <linux/module.h>

#include <asm/intel_telemetry.h>

#define DRIVER_NAME "intel_telemetry_core"

struct telemetry_core_config {
	struct telemetry_plt_config *plt_config;
	const struct telemetry_core_ops *telem_ops;
};

static struct telemetry_core_config telm_core_conf;

static int telemetry_def_update_events(struct telemetry_evtconfig pss_evtconfig,
				      struct telemetry_evtconfig ioss_evtconfig)
{
	return 0;
}

static int telemetry_def_set_sampling_period(u8 pss_period, u8 ioss_period)
{
	return 0;
}

static int telemetry_def_get_sampling_period(u8 *pss_min_period,
					     u8 *pss_max_period,
					     u8 *ioss_min_period,
					     u8 *ioss_max_period)
{
	return 0;
}

static int telemetry_def_get_eventconfig(
			struct telemetry_evtconfig *pss_evtconfig,
			struct telemetry_evtconfig *ioss_evtconfig,
			int pss_len, int ioss_len)
{
	return 0;
}

static int telemetry_def_get_trace_verbosity(enum telemetry_unit telem_unit,
					     u32 *verbosity)
{
	return 0;
}


static int telemetry_def_set_trace_verbosity(enum telemetry_unit telem_unit,
					     u32 verbosity)
{
	return 0;
}

static int telemetry_def_raw_read_eventlog(enum telemetry_unit telem_unit,
					   struct telemetry_evtlog *evtlog,
					   int len, int log_all_evts)
{
	return 0;
}

static int telemetry_def_read_eventlog(enum telemetry_unit telem_unit,
				       struct telemetry_evtlog *evtlog,
				       int len, int log_all_evts)
{
	return 0;
}

static int telemetry_def_add_events(u8 num_pss_evts, u8 num_ioss_evts,
				    u32 *pss_evtmap, u32 *ioss_evtmap)
{
	return 0;
}

static int telemetry_def_reset_events(void)
{
	return 0;
}

static const struct telemetry_core_ops telm_defpltops = {
	.set_sampling_period = telemetry_def_set_sampling_period,
	.get_sampling_period = telemetry_def_get_sampling_period,
	.get_trace_verbosity = telemetry_def_get_trace_verbosity,
	.set_trace_verbosity = telemetry_def_set_trace_verbosity,
	.raw_read_eventlog = telemetry_def_raw_read_eventlog,
	.get_eventconfig = telemetry_def_get_eventconfig,
	.read_eventlog = telemetry_def_read_eventlog,
	.update_events = telemetry_def_update_events,
	.reset_events = telemetry_def_reset_events,
	.add_events = telemetry_def_add_events,
};

 
int telemetry_update_events(struct telemetry_evtconfig pss_evtconfig,
			    struct telemetry_evtconfig ioss_evtconfig)
{
	return telm_core_conf.telem_ops->update_events(pss_evtconfig,
						       ioss_evtconfig);
}
EXPORT_SYMBOL_GPL(telemetry_update_events);


 
int telemetry_set_sampling_period(u8 pss_period, u8 ioss_period)
{
	return telm_core_conf.telem_ops->set_sampling_period(pss_period,
							     ioss_period);
}
EXPORT_SYMBOL_GPL(telemetry_set_sampling_period);

 
int telemetry_get_sampling_period(u8 *pss_min_period, u8 *pss_max_period,
				  u8 *ioss_min_period, u8 *ioss_max_period)
{
	return telm_core_conf.telem_ops->get_sampling_period(pss_min_period,
							     pss_max_period,
							     ioss_min_period,
							     ioss_max_period);
}
EXPORT_SYMBOL_GPL(telemetry_get_sampling_period);


 
int telemetry_reset_events(void)
{
	return telm_core_conf.telem_ops->reset_events();
}
EXPORT_SYMBOL_GPL(telemetry_reset_events);

 
int telemetry_get_eventconfig(struct telemetry_evtconfig *pss_evtconfig,
			      struct telemetry_evtconfig *ioss_evtconfig,
			      int pss_len, int ioss_len)
{
	return telm_core_conf.telem_ops->get_eventconfig(pss_evtconfig,
							 ioss_evtconfig,
							 pss_len, ioss_len);
}
EXPORT_SYMBOL_GPL(telemetry_get_eventconfig);

 
int telemetry_add_events(u8 num_pss_evts, u8 num_ioss_evts,
			 u32 *pss_evtmap, u32 *ioss_evtmap)
{
	return telm_core_conf.telem_ops->add_events(num_pss_evts,
						    num_ioss_evts, pss_evtmap,
						    ioss_evtmap);
}
EXPORT_SYMBOL_GPL(telemetry_add_events);

 
int telemetry_read_events(enum telemetry_unit telem_unit,
			  struct telemetry_evtlog *evtlog, int len)
{
	return telm_core_conf.telem_ops->read_eventlog(telem_unit, evtlog,
						       len, 0);
}
EXPORT_SYMBOL_GPL(telemetry_read_events);

 
int telemetry_raw_read_events(enum telemetry_unit telem_unit,
			      struct telemetry_evtlog *evtlog, int len)
{
	return telm_core_conf.telem_ops->raw_read_eventlog(telem_unit, evtlog,
							   len, 0);
}
EXPORT_SYMBOL_GPL(telemetry_raw_read_events);

 
int telemetry_read_eventlog(enum telemetry_unit telem_unit,
			    struct telemetry_evtlog *evtlog, int len)
{
	return telm_core_conf.telem_ops->read_eventlog(telem_unit, evtlog,
						       len, 1);
}
EXPORT_SYMBOL_GPL(telemetry_read_eventlog);

 
int telemetry_raw_read_eventlog(enum telemetry_unit telem_unit,
				struct telemetry_evtlog *evtlog, int len)
{
	return telm_core_conf.telem_ops->raw_read_eventlog(telem_unit, evtlog,
							   len, 1);
}
EXPORT_SYMBOL_GPL(telemetry_raw_read_eventlog);


 
int telemetry_get_trace_verbosity(enum telemetry_unit telem_unit,
				  u32 *verbosity)
{
	return telm_core_conf.telem_ops->get_trace_verbosity(telem_unit,
							     verbosity);
}
EXPORT_SYMBOL_GPL(telemetry_get_trace_verbosity);


 
int telemetry_set_trace_verbosity(enum telemetry_unit telem_unit, u32 verbosity)
{
	return telm_core_conf.telem_ops->set_trace_verbosity(telem_unit,
							     verbosity);
}
EXPORT_SYMBOL_GPL(telemetry_set_trace_verbosity);

 
int telemetry_set_pltdata(const struct telemetry_core_ops *ops,
			  struct telemetry_plt_config *pltconfig)
{
	if (ops)
		telm_core_conf.telem_ops = ops;

	if (pltconfig)
		telm_core_conf.plt_config = pltconfig;

	return 0;
}
EXPORT_SYMBOL_GPL(telemetry_set_pltdata);

 
int telemetry_clear_pltdata(void)
{
	telm_core_conf.telem_ops = &telm_defpltops;
	telm_core_conf.plt_config = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(telemetry_clear_pltdata);

 
struct telemetry_plt_config *telemetry_get_pltdata(void)
{
	return telm_core_conf.plt_config;
}
EXPORT_SYMBOL_GPL(telemetry_get_pltdata);

static inline int telemetry_get_pssevtname(enum telemetry_unit telem_unit,
					   const char **name, int len)
{
	struct telemetry_unit_config psscfg;
	int i;

	if (!telm_core_conf.plt_config)
		return -EINVAL;

	psscfg = telm_core_conf.plt_config->pss_config;

	if (len > psscfg.ssram_evts_used)
		len = psscfg.ssram_evts_used;

	for (i = 0; i < len; i++)
		name[i] = psscfg.telem_evts[i].name;

	return 0;
}

static inline int telemetry_get_iossevtname(enum telemetry_unit telem_unit,
					    const char **name, int len)
{
	struct telemetry_unit_config iosscfg;
	int i;

	if (!(telm_core_conf.plt_config))
		return -EINVAL;

	iosscfg = telm_core_conf.plt_config->ioss_config;

	if (len > iosscfg.ssram_evts_used)
		len = iosscfg.ssram_evts_used;

	for (i = 0; i < len; i++)
		name[i] = iosscfg.telem_evts[i].name;

	return 0;

}

 
int telemetry_get_evtname(enum telemetry_unit telem_unit,
			  const char **name, int len)
{
	int ret = -EINVAL;

	if (telem_unit == TELEM_PSS)
		ret = telemetry_get_pssevtname(telem_unit, name, len);

	else if (telem_unit == TELEM_IOSS)
		ret = telemetry_get_iossevtname(telem_unit, name, len);

	return ret;
}
EXPORT_SYMBOL_GPL(telemetry_get_evtname);

static int __init telemetry_module_init(void)
{
	pr_info(pr_fmt(DRIVER_NAME) " Init\n");

	telm_core_conf.telem_ops = &telm_defpltops;
	return 0;
}

static void __exit telemetry_module_exit(void)
{
}

module_init(telemetry_module_init);
module_exit(telemetry_module_exit);

MODULE_AUTHOR("Souvik Kumar Chakravarty <souvik.k.chakravarty@intel.com>");
MODULE_DESCRIPTION("Intel SoC Telemetry Interface");
MODULE_LICENSE("GPL v2");
