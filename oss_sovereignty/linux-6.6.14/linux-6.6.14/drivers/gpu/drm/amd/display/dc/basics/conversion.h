#ifndef __DAL_CONVERSION_H__
#define __DAL_CONVERSION_H__
#include "include/fixed31_32.h"
uint16_t fixed_point_to_int_frac(
	struct fixed31_32 arg,
	uint8_t integer_bits,
	uint8_t fractional_bits);
void convert_float_matrix(
	uint16_t *matrix,
	struct fixed31_32 *flt,
	uint32_t buffer_size);
void reduce_fraction(uint32_t num, uint32_t den,
		uint32_t *out_num, uint32_t *out_den);
static inline unsigned int log_2(unsigned int num)
{
	return ilog2(num);
}
#endif
