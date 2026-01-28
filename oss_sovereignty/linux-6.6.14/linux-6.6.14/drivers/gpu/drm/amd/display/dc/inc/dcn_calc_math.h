#ifndef _DCN_CALC_MATH_H_
#define _DCN_CALC_MATH_H_
float dcn_bw_mod(const float arg1, const float arg2);
float dcn_bw_min2(const float arg1, const float arg2);
unsigned int dcn_bw_max(const unsigned int arg1, const unsigned int arg2);
float dcn_bw_max2(const float arg1, const float arg2);
float dcn_bw_floor2(const float arg, const float significance);
float dcn_bw_floor(const float arg);
float dcn_bw_ceil2(const float arg, const float significance);
float dcn_bw_ceil(const float arg);
float dcn_bw_max3(float v1, float v2, float v3);
float dcn_bw_max5(float v1, float v2, float v3, float v4, float v5);
float dcn_bw_pow(float a, float exp);
float dcn_bw_log(float a, float b);
double dcn_bw_fabs(double a);
#endif  
