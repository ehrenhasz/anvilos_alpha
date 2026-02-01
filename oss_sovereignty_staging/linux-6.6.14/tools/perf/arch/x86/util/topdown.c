
#include "api/fs/fs.h"
#include "util/evsel.h"
#include "util/pmu.h"
#include "util/pmus.h"
#include "util/topdown.h"
#include "topdown.h"
#include "evsel.h"

 
bool topdown_sys_has_perf_metrics(void)
{
	static bool has_perf_metrics;
	static bool cached;
	struct perf_pmu *pmu;

	if (cached)
		return has_perf_metrics;

	 
	pmu = perf_pmus__find_by_type(PERF_TYPE_RAW);
	if (pmu && perf_pmu__have_event(pmu, "slots"))
		has_perf_metrics = true;

	cached = true;
	return has_perf_metrics;
}

#define TOPDOWN_SLOTS		0x0400

 
bool arch_topdown_sample_read(struct evsel *leader)
{
	if (!evsel__sys_has_perf_metrics(leader))
		return false;

	if (leader->core.attr.config == TOPDOWN_SLOTS)
		return true;

	return false;
}
