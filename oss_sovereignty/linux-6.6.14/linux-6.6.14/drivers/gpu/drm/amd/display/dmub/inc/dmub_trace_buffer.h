#ifndef _DMUB_TRACE_BUFFER_H_
#define _DMUB_TRACE_BUFFER_H_
#include "dmub_cmd.h"
#define LOAD_DMCU_FW	1
#define LOAD_PHY_FW	2
enum dmucb_trace_code {
	DMCUB__UNKNOWN,
	DMCUB__MAIN_BEGIN,
	DMCUB__PHY_INIT_BEGIN,
	DMCUB__PHY_FW_SRAM_LOAD_BEGIN,
	DMCUB__PHY_FW_SRAM_LOAD_END,
	DMCUB__PHY_INIT_POLL_DONE,
	DMCUB__PHY_INIT_END,
	DMCUB__DMCU_ERAM_LOAD_BEGIN,
	DMCUB__DMCU_ERAM_LOAD_END,
	DMCUB__DMCU_ISR_LOAD_BEGIN,
	DMCUB__DMCU_ISR_LOAD_END,
	DMCUB__MAIN_IDLE,
	DMCUB__PERF_TRACE,
	DMCUB__PG_DONE,
};
struct dmcub_trace_buf_entry {
	enum dmucb_trace_code trace_code;
	uint32_t tick_count;
	uint32_t param0;
	uint32_t param1;
};
#define TRACE_BUF_SIZE (1024)  
#define PERF_TRACE_MAX_ENTRY ((TRACE_BUF_SIZE - 8)/sizeof(struct dmcub_trace_buf_entry))
struct dmcub_trace_buf {
	uint32_t entry_count;
	uint32_t clk_freq;
	struct dmcub_trace_buf_entry entries[PERF_TRACE_MAX_ENTRY];
};
#endif  
