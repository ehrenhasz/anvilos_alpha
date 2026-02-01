 

#ifndef COLOR_MOD_COLOR_GAMMA_H_
#define COLOR_MOD_COLOR_GAMMA_H_

#include "color_table.h"

struct dc_transfer_func;
struct dc_gamma;
struct dc_transfer_func_distributed_points;
struct dc_rgb_fixed;
struct dc_color_caps;
enum dc_transfer_func_predefined;

 
union regamma_flags {
	unsigned int raw;
	struct {
		unsigned int gammaRampArray       :1;    
		unsigned int gammaFromEdid        :1;    
		unsigned int gammaFromEdidEx      :1;    
		unsigned int gammaFromUser        :1;    
		unsigned int coeffFromUser        :1;    
		unsigned int coeffFromEdid        :1;    
		unsigned int applyDegamma         :1;    
		unsigned int gammaPredefinedSRGB  :1;    
		unsigned int gammaPredefinedPQ    :1;    
		unsigned int gammaPredefinedPQ2084Interim :1;    
		unsigned int gammaPredefined36    :1;    
		unsigned int gammaPredefinedReset :1;    
	} bits;
};

struct regamma_ramp {
	unsigned short gamma[256*3];  
};

struct regamma_coeff {
	int    gamma[3];
	int    A0[3];
	int    A1[3];
	int    A2[3];
	int    A3[3];
};

struct regamma_lut {
	union regamma_flags flags;
	union {
		struct regamma_ramp ramp;
		struct regamma_coeff coeff;
	};
};

struct hdr_tm_params {
	unsigned int sdr_white_level;
	unsigned int min_content; 
	unsigned int max_content; 
	unsigned int min_display; 
	unsigned int max_display; 
	unsigned int skip_tm; 
};

struct calculate_buffer {
	int buffer_index;
	struct fixed31_32 buffer[NUM_PTS_IN_REGION];
	struct fixed31_32 gamma_of_2;
};

struct translate_from_linear_space_args {
	struct fixed31_32 arg;
	struct fixed31_32 a0;
	struct fixed31_32 a1;
	struct fixed31_32 a2;
	struct fixed31_32 a3;
	struct fixed31_32 gamma;
	struct calculate_buffer *cal_buffer;
};

void setup_x_points_distribution(void);
void log_x_points_distribution(struct dal_logger *logger);
void precompute_pq(void);
void precompute_de_pq(void);

bool mod_color_calculate_regamma_params(struct dc_transfer_func *output_tf,
		const struct dc_gamma *ramp, bool mapUserRamp, bool canRomBeUsed,
		const struct hdr_tm_params *fs_params,
		struct calculate_buffer *cal_buffer);

bool mod_color_calculate_degamma_params(struct dc_color_caps *dc_caps,
		struct dc_transfer_func *output_tf,
		const struct dc_gamma *ramp, bool mapUserRamp);

bool calculate_user_regamma_coeff(struct dc_transfer_func *output_tf,
		const struct regamma_lut *regamma,
		struct calculate_buffer *cal_buffer,
		const struct dc_gamma *ramp);

bool calculate_user_regamma_ramp(struct dc_transfer_func *output_tf,
		const struct regamma_lut *regamma,
		struct calculate_buffer *cal_buffer,
		const struct dc_gamma *ramp);


#endif  
