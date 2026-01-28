#ifndef _CORESIGHT_TRACE_ID_H
#define _CORESIGHT_TRACE_ID_H
#include <linux/bitops.h>
#include <linux/types.h>
#define CORESIGHT_TRACE_IDS_MAX 128
#define CORESIGHT_TRACE_ID_RES_0 0
#define CORESIGHT_TRACE_ID_RES_TOP 0x70
#define IS_VALID_CS_TRACE_ID(id)	\
	((id > CORESIGHT_TRACE_ID_RES_0) && (id < CORESIGHT_TRACE_ID_RES_TOP))
struct coresight_trace_id_map {
	DECLARE_BITMAP(used_ids, CORESIGHT_TRACE_IDS_MAX);
	DECLARE_BITMAP(pend_rel_ids, CORESIGHT_TRACE_IDS_MAX);
};
int coresight_trace_id_get_cpu_id(int cpu);
void coresight_trace_id_put_cpu_id(int cpu);
int coresight_trace_id_read_cpu_id(int cpu);
int coresight_trace_id_get_system_id(void);
void coresight_trace_id_put_system_id(int id);
void coresight_trace_id_perf_start(void);
void coresight_trace_id_perf_stop(void);
#endif  
