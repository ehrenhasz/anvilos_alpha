#ifndef __DAL_DCCG_H__
#define __DAL_DCCG_H__
#include "dc_types.h"
#include "hw_shared.h"
enum phyd32clk_clock_source {
	PHYD32CLKA,
	PHYD32CLKB,
	PHYD32CLKC,
	PHYD32CLKD,
	PHYD32CLKE,
	PHYD32CLKF,
	PHYD32CLKG,
};
enum physymclk_clock_source {
	PHYSYMCLK_FORCE_SRC_SYMCLK,     
	PHYSYMCLK_FORCE_SRC_PHYD18CLK,  
	PHYSYMCLK_FORCE_SRC_PHYD32CLK,  
};
enum streamclk_source {
	REFCLK,                    
	DTBCLK0,                   
	DPREFCLK,                  
};
enum dentist_dispclk_change_mode {
	DISPCLK_CHANGE_MODE_IMMEDIATE,
	DISPCLK_CHANGE_MODE_RAMPING,
};
enum pixel_rate_div {
   PIXEL_RATE_DIV_BY_1 = 0,
   PIXEL_RATE_DIV_BY_2 = 1,
   PIXEL_RATE_DIV_BY_4 = 3,
   PIXEL_RATE_DIV_NA = 0xF
};
struct dccg {
	struct dc_context *ctx;
	const struct dccg_funcs *funcs;
	int pipe_dppclk_khz[MAX_PIPES];
	int ref_dppclk;
	bool dpp_clock_gated[MAX_PIPES];
};
struct dtbclk_dto_params {
	const struct dc_crtc_timing *timing;
	int otg_inst;
	int pixclk_khz;
	int req_audio_dtbclk_khz;
	int num_odm_segments;
	int ref_dtbclk_khz;
	bool is_hdmi;
};
struct dccg_funcs {
	void (*update_dpp_dto)(struct dccg *dccg,
			int dpp_inst,
			int req_dppclk);
	void (*get_dccg_ref_freq)(struct dccg *dccg,
			unsigned int xtalin_freq_inKhz,
			unsigned int *dccg_ref_freq_inKhz);
	void (*set_fifo_errdet_ovr_en)(struct dccg *dccg,
			bool en);
	void (*otg_add_pixel)(struct dccg *dccg,
			uint32_t otg_inst);
	void (*otg_drop_pixel)(struct dccg *dccg,
			uint32_t otg_inst);
	void (*dccg_init)(struct dccg *dccg);
	void (*set_dpstreamclk)(
			struct dccg *dccg,
			enum streamclk_source src,
			int otg_inst,
			int dp_hpo_inst);
	void (*enable_symclk32_se)(
			struct dccg *dccg,
			int hpo_se_inst,
			enum phyd32clk_clock_source phyd32clk);
	void (*disable_symclk32_se)(
			struct dccg *dccg,
			int hpo_se_inst);
	void (*enable_symclk32_le)(
			struct dccg *dccg,
			int hpo_le_inst,
			enum phyd32clk_clock_source phyd32clk);
	void (*disable_symclk32_le)(
			struct dccg *dccg,
			int hpo_le_inst);
	void (*set_symclk32_le_root_clock_gating)(
			struct dccg *dccg,
			int hpo_le_inst,
			bool enable);
	void (*set_physymclk)(
			struct dccg *dccg,
			int phy_inst,
			enum physymclk_clock_source clk_src,
			bool force_enable);
	void (*set_dtbclk_dto)(
			struct dccg *dccg,
			const struct dtbclk_dto_params *params);
	void (*set_audio_dtbclk_dto)(
			struct dccg *dccg,
			const struct dtbclk_dto_params *params);
	void (*set_dispclk_change_mode)(
			struct dccg *dccg,
			enum dentist_dispclk_change_mode change_mode);
	void (*disable_dsc)(
		struct dccg *dccg,
		int inst);
	void (*enable_dsc)(
		struct dccg *dccg,
		int inst);
	void (*set_pixel_rate_div)(struct dccg *dccg,
			uint32_t otg_inst,
			enum pixel_rate_div k1,
			enum pixel_rate_div k2);
	void (*set_valid_pixel_rate)(
			struct dccg *dccg,
			int ref_dtbclk_khz,
			int otg_inst,
			int pixclk_khz);
	void (*trigger_dio_fifo_resync)(
			struct dccg *dccg);
	void (*dpp_root_clock_control)(
			struct dccg *dccg,
			unsigned int dpp_inst,
			bool clock_on);
	void (*enable_symclk_se)(
			struct dccg *dccg,
			uint32_t stream_enc_inst,
			uint32_t link_enc_inst);
	void (*disable_symclk_se)(
			struct dccg *dccg,
			uint32_t stream_enc_inst,
			uint32_t link_enc_inst);
};
#endif  
