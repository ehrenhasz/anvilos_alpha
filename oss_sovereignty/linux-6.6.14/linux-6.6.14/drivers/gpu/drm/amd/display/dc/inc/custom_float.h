#ifndef CUSTOM_FLOAT_H_
#define CUSTOM_FLOAT_H_
#include "bw_fixed.h"
#include "hw_shared.h"
#include "opp.h"
bool convert_to_custom_float_format(
	struct fixed31_32 value,
	const struct custom_float_format *format,
	uint32_t *result);
#endif  
