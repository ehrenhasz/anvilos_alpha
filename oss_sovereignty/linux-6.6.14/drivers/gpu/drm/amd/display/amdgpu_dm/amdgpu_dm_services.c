 

#include <linux/string.h>
#include <linux/acpi.h>

#include <drm/drm_probe_helper.h>
#include <drm/amdgpu_drm.h>
#include "dm_services.h"
#include "amdgpu.h"
#include "amdgpu_dm.h"
#include "amdgpu_dm_irq.h"
#include "amdgpu_pm.h"
#include "amdgpu_dm_trace.h"

	unsigned long long
	dm_get_elapse_time_in_ns(struct dc_context *ctx,
				 unsigned long long current_time_stamp,
				 unsigned long long last_time_stamp)
{
	return current_time_stamp - last_time_stamp;
}

void dm_perf_trace_timestamp(const char *func_name, unsigned int line, struct dc_context *ctx)
{
	trace_amdgpu_dc_performance(ctx->perf_trace->read_count,
				    ctx->perf_trace->write_count,
				    &ctx->perf_trace->last_entry_read,
				    &ctx->perf_trace->last_entry_write,
				    func_name, line);
}

 
