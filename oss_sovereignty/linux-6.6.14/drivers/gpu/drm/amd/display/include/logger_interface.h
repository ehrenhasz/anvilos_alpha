 

#ifndef __DAL_LOGGER_INTERFACE_H__
#define __DAL_LOGGER_INTERFACE_H__

#include "logger_types.h"

struct dc_context;
struct dc_link;
struct dc_surface_update;
struct resource_context;
struct dc_state;

 

void pre_surface_trace(
		struct dc *dc,
		const struct dc_plane_state *const *plane_states,
		int surface_count);

void update_surface_trace(
		struct dc *dc,
		const struct dc_surface_update *updates,
		int surface_count);

void post_surface_trace(struct dc *dc);

void context_timing_trace(
		struct dc *dc,
		struct resource_context *res_ctx);

void context_clock_trace(
		struct dc *dc,
		struct dc_state *context);

 

#define DAL_LOGGER_NOT_IMPL(fmt, ...) \
	do { \
		static bool print_not_impl = true; \
		if (print_not_impl == true) { \
			print_not_impl = false; \
			DRM_WARN("DAL_NOT_IMPL: " fmt, ##__VA_ARGS__); \
		} \
	} while (0)

 

#define DC_ERROR(...) \
		do { \
			(void)(dc_ctx); \
			DC_LOG_ERROR(__VA_ARGS__); \
		} while (0)

#define DC_SYNC_INFO(...) \
		do { \
			(void)(dc_ctx); \
			DC_LOG_SYNC(__VA_ARGS__); \
		} while (0)

 

#define CONN_DATA_DETECT(link, hex_data, hex_len, ...) \
		do { \
			(void)(link); \
			DC_LOG_EVENT_DETECTION(__VA_ARGS__); \
		} while (0)

#define CONN_DATA_LINK_LOSS(link, hex_data, hex_len, ...) \
		do { \
			(void)(link); \
			DC_LOG_EVENT_LINK_LOSS(__VA_ARGS__); \
		} while (0)

#define CONN_MSG_LT(link, ...) \
		do { \
			(void)(link); \
			DC_LOG_EVENT_LINK_TRAINING(__VA_ARGS__); \
		} while (0)

#define CONN_MSG_MODE(link, ...) \
		do { \
			(void)(link); \
			DC_LOG_EVENT_MODE_SET(__VA_ARGS__); \
		} while (0)

 
#define DTN_INFO_BEGIN() \
	dm_dtn_log_begin(dc_ctx, log_ctx)

#define DTN_INFO(msg, ...) \
	dm_dtn_log_append_v(dc_ctx, log_ctx, msg, ##__VA_ARGS__)

#define DTN_INFO_END() \
	dm_dtn_log_end(dc_ctx, log_ctx)

#define PERFORMANCE_TRACE_START() \
	unsigned long long perf_trc_start_stmp = dm_get_timestamp(dc->ctx)

#define PERFORMANCE_TRACE_END() \
	do { \
		unsigned long long perf_trc_end_stmp = dm_get_timestamp(dc->ctx); \
		if (dc->debug.performance_trace) { \
			DC_LOG_PERF_TRACE("%s duration: %lld ticks\n", __func__, \
				perf_trc_end_stmp - perf_trc_start_stmp); \
		} \
	} while (0)

#define DISPLAY_STATS_BEGIN(entry) (void)(entry)

#define DISPLAY_STATS(msg, ...) DC_LOG_PERF_TRACE(msg, __VA_ARGS__)

#define DISPLAY_STATS_END(entry) (void)(entry)

#define LOG_GAMMA_WRITE(msg, ...)

#endif  
