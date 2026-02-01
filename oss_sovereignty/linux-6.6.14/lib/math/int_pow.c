
 

#include <linux/export.h>
#include <linux/math.h>
#include <linux/types.h>

 
u64 int_pow(u64 base, unsigned int exp)
{
	u64 result = 1;

	while (exp) {
		if (exp & 1)
			result *= base;
		exp >>= 1;
		base *= base;
	}

	return result;
}
EXPORT_SYMBOL_GPL(int_pow);
