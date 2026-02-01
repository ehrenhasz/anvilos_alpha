 

 

 

#include "fdlibm.h"

#ifdef __STDC__
	float ldexpf(float value, int exp)
#else
	float ldexpf(value, exp)
	float value; int exp;
#endif
{
	if(!isfinite(value)||value==(float)0.0) return value;
	value = scalbnf(value,exp);
	
	return value;
}

#ifdef _DOUBLE_IS_32BITS

#ifdef __STDC__
	double ldexp(double value, int exp)
#else
	double ldexp(value, exp)
	double value; int exp;
#endif
{
	return (double) ldexpf((float) value, exp);
}

#endif  
