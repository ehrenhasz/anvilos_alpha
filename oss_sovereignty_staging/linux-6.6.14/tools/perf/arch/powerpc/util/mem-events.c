
#include "map_symbol.h"
#include "mem-events.h"

 
const char *perf_mem_events__name(int i, const char *pmu_name __maybe_unused)
{
	if (i == PERF_MEM_EVENTS__LOAD)
		return "cpu/mem-loads/";

	return "cpu/mem-stores/";
}
