
 

#include "cxd2880_common.h"

int cxd2880_convert2s_complement(u32 value, u32 bitlen)
{
	if (!bitlen || bitlen >= 32)
		return (int)value;

	if (value & (u32)(1 << (bitlen - 1)))
		return (int)(GENMASK(31, bitlen) | value);
	else
		return (int)(GENMASK(bitlen - 1, 0) & value);
}
