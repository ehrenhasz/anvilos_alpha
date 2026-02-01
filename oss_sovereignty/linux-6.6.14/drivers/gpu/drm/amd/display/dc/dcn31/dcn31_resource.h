 

#ifndef _DCN31_RESOURCE_H_
#define _DCN31_RESOURCE_H_

#include "core_types.h"

#define TO_DCN31_RES_POOL(pool)\
	container_of(pool, struct dcn31_resource_pool, base)

extern struct _vcs_dpi_ip_params_st dcn3_1_ip;

struct dcn31_resource_pool {
	struct resource_pool base;
};

bool dcn31_validate_bandwidth(struct dc *dc,
		struct dc_state *context,
		bool fast_validate);
void dcn31_calculate_wm_and_dlg(
		struct dc *dc, struct dc_state *context,
		display_e2e_pipe_params_st *pipes,
		int pipe_cnt,
		int vlevel);
int dcn31_populate_dml_pipes_from_context(
	struct dc *dc, struct dc_state *context,
	display_e2e_pipe_params_st *pipes,
	bool fast_validate);
void
dcn31_populate_dml_writeback_from_context(struct dc *dc,
					  struct resource_context *res_ctx,
					  display_e2e_pipe_params_st *pipes);
void
dcn31_set_mcif_arb_params(struct dc *dc,
			  struct dc_state *context,
			  display_e2e_pipe_params_st *pipes,
			  int pipe_cnt);

struct resource_pool *dcn31_create_resource_pool(
		const struct dc_init_data *init_data,
		struct dc *dc);

 
#ifndef regPHYPLLF_PIXCLK_RESYNC_CNTL
#define regPHYPLLF_PIXCLK_RESYNC_CNTL 0x007e
#define regPHYPLLF_PIXCLK_RESYNC_CNTL_BASE_IDX 1
#define regPHYPLLG_PIXCLK_RESYNC_CNTL 0x005f
#define regPHYPLLG_PIXCLK_RESYNC_CNTL_BASE_IDX 1


#define PHYPLLF_PIXCLK_RESYNC_CNTL__PHYPLLF_PIXCLK_RESYNC_ENABLE__SHIFT 0x0
#define PHYPLLF_PIXCLK_RESYNC_CNTL__PHYPLLF_DEEP_COLOR_DTO_ENABLE_STATUS__SHIFT 0x1
#define PHYPLLF_PIXCLK_RESYNC_CNTL__PHYPLLF_DCCG_DEEP_COLOR_CNTL__SHIFT 0x4
#define PHYPLLF_PIXCLK_RESYNC_CNTL__PHYPLLF_PIXCLK_ENABLE__SHIFT 0x8
#define PHYPLLF_PIXCLK_RESYNC_CNTL__PHYPLLF_PIXCLK_DOUBLE_RATE_ENABLE__SHIFT 0x9
#define PHYPLLF_PIXCLK_RESYNC_CNTL__PHYPLLF_PIXCLK_RESYNC_ENABLE_MASK 0x00000001L
#define PHYPLLF_PIXCLK_RESYNC_CNTL__PHYPLLF_DEEP_COLOR_DTO_ENABLE_STATUS_MASK 0x00000002L
#define PHYPLLF_PIXCLK_RESYNC_CNTL__PHYPLLF_DCCG_DEEP_COLOR_CNTL_MASK 0x00000030L
#define PHYPLLF_PIXCLK_RESYNC_CNTL__PHYPLLF_PIXCLK_ENABLE_MASK 0x00000100L
#define PHYPLLF_PIXCLK_RESYNC_CNTL__PHYPLLF_PIXCLK_DOUBLE_RATE_ENABLE_MASK 0x00000200L


#define PHYPLLG_PIXCLK_RESYNC_CNTL__PHYPLLG_PIXCLK_RESYNC_ENABLE__SHIFT 0x0
#define PHYPLLG_PIXCLK_RESYNC_CNTL__PHYPLLG_DEEP_COLOR_DTO_ENABLE_STATUS__SHIFT 0x1
#define PHYPLLG_PIXCLK_RESYNC_CNTL__PHYPLLG_DCCG_DEEP_COLOR_CNTL__SHIFT 0x4
#define PHYPLLG_PIXCLK_RESYNC_CNTL__PHYPLLG_PIXCLK_ENABLE__SHIFT 0x8
#define PHYPLLG_PIXCLK_RESYNC_CNTL__PHYPLLG_PIXCLK_DOUBLE_RATE_ENABLE__SHIFT 0x9
#define PHYPLLG_PIXCLK_RESYNC_CNTL__PHYPLLG_PIXCLK_RESYNC_ENABLE_MASK 0x00000001L
#define PHYPLLG_PIXCLK_RESYNC_CNTL__PHYPLLG_DEEP_COLOR_DTO_ENABLE_STATUS_MASK 0x00000002L
#define PHYPLLG_PIXCLK_RESYNC_CNTL__PHYPLLG_DCCG_DEEP_COLOR_CNTL_MASK 0x00000030L
#define PHYPLLG_PIXCLK_RESYNC_CNTL__PHYPLLG_PIXCLK_ENABLE_MASK 0x00000100L
#define PHYPLLG_PIXCLK_RESYNC_CNTL__PHYPLLG_PIXCLK_DOUBLE_RATE_ENABLE_MASK 0x00000200L
#endif
#endif  
