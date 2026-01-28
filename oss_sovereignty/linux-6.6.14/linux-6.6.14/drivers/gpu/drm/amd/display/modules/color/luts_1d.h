#ifndef LUTS_1D_H
#define LUTS_1D_H
#include "hw_shared.h"
struct point_config {
	uint32_t custom_float_x;
	uint32_t custom_float_y;
	uint32_t custom_float_slope;
};
struct lut_point {
	uint32_t red;
	uint32_t green;
	uint32_t blue;
	uint32_t delta_red;
	uint32_t delta_green;
	uint32_t delta_blue;
};
struct pwl_1dlut_parameter {
	struct gamma_curve	arr_curve_points[34];
	struct point_config	arr_points[2];
	struct lut_point rgb_resulted[256];
	uint32_t hw_points_num;
};
#endif  
