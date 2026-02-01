 
 

#ifndef __PIXELGEN_GLOBAL_H_INCLUDED__
#define __PIXELGEN_GLOBAL_H_INCLUDED__

#include <type_support.h>

 
 
typedef struct isp2401_sync_generator_cfg_s isp2401_sync_generator_cfg_t;
struct isp2401_sync_generator_cfg_s {
	u32	hblank_cycles;
	u32	vblank_cycles;
	u32	pixels_per_clock;
	u32	nr_of_frames;
	u32	pixels_per_line;
	u32	lines_per_frame;
};

typedef enum {
	PIXELGEN_TPG_MODE_RAMP = 0,
	PIXELGEN_TPG_MODE_CHBO,
	PIXELGEN_TPG_MODE_MONO,
	N_PIXELGEN_TPG_MODE
} pixelgen_tpg_mode_t;

 
typedef struct pixelgen_tpg_cfg_s pixelgen_tpg_cfg_t;
struct pixelgen_tpg_cfg_s {
	pixelgen_tpg_mode_t	mode;	 

	struct {
		 
		u32 R1;
		u32 G1;
		u32 B1;

		 
		u32 R2;
		u32 G2;
		u32 B2;
	} color_cfg;

	struct {
		u32	h_mask;		 
		u32	v_mask;		 
		u32	hv_mask;	 
	} mask_cfg;

	struct {
		s32	h_delta;	 
		s32	v_delta;	 
	} delta_cfg;

	isp2401_sync_generator_cfg_t	 sync_gen_cfg;
};

 
typedef struct pixelgen_prbs_cfg_s pixelgen_prbs_cfg_t;
struct pixelgen_prbs_cfg_s {
	s32	seed0;
	s32	seed1;

	isp2401_sync_generator_cfg_t	sync_gen_cfg;
};

 
#endif  
