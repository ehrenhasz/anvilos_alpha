 

#include "os_types.h"
#include "dcn_calc_math.h"

#define isNaN(number) ((number) != (number))

 

float dcn_bw_mod(const float arg1, const float arg2)
{
	if (isNaN(arg1))
		return arg2;
	if (isNaN(arg2))
		return arg1;
	return arg1 - arg1 * ((int) (arg1 / arg2));
}

float dcn_bw_min2(const float arg1, const float arg2)
{
	if (isNaN(arg1))
		return arg2;
	if (isNaN(arg2))
		return arg1;
	return arg1 < arg2 ? arg1 : arg2;
}

unsigned int dcn_bw_max(const unsigned int arg1, const unsigned int arg2)
{
	return arg1 > arg2 ? arg1 : arg2;
}
float dcn_bw_max2(const float arg1, const float arg2)
{
	if (isNaN(arg1))
		return arg2;
	if (isNaN(arg2))
		return arg1;
	return arg1 > arg2 ? arg1 : arg2;
}

float dcn_bw_floor2(const float arg, const float significance)
{
	ASSERT(significance != 0);

	return ((int) (arg / significance)) * significance;
}
float dcn_bw_floor(const float arg)
{
	return ((int) (arg));
}

float dcn_bw_ceil(const float arg)
{
	return (int) (arg + 0.99999);
}

float dcn_bw_ceil2(const float arg, const float significance)
{
	ASSERT(significance != 0);

	return ((int) (arg / significance + 0.99999)) * significance;
}

float dcn_bw_max3(float v1, float v2, float v3)
{
	return v3 > dcn_bw_max2(v1, v2) ? v3 : dcn_bw_max2(v1, v2);
}

float dcn_bw_max5(float v1, float v2, float v3, float v4, float v5)
{
	return dcn_bw_max3(v1, v2, v3) > dcn_bw_max2(v4, v5) ? dcn_bw_max3(v1, v2, v3) : dcn_bw_max2(v4, v5);
}

float dcn_bw_pow(float a, float exp)
{
	float temp;
	 
	if ((int)exp == 0)
		return 1;
	temp = dcn_bw_pow(a, (int)(exp / 2));
	if (((int)exp % 2) == 0) {
		return temp * temp;
	} else {
		if ((int)exp > 0)
			return a * temp * temp;
		else
			return (temp * temp) / a;
	}
}

double dcn_bw_fabs(double a)
{
	if (a > 0)
		return (a);
	else
		return (-a);
}


float dcn_bw_log(float a, float b)
{
	int * const exp_ptr = (int *)(&a);
	int x = *exp_ptr;
	const int log_2 = ((x >> 23) & 255) - 128;
	x &= ~(255 << 23);
	x += 127 << 23;
	*exp_ptr = x;

	a = ((-1.0f / 3) * a + 2) * a - 2.0f / 3;

	if (b > 2.00001 || b < 1.99999)
		return (a + log_2) / dcn_bw_log(b, 2);
	else
		return (a + log_2);
}
