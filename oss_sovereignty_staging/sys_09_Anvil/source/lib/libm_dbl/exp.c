 
 
 

#include "libm.h"

static const double
half[2] = {0.5,-0.5},
ln2hi = 6.93147180369123816490e-01,  
ln2lo = 1.90821492927058770002e-10,  
invln2 = 1.44269504088896338700e+00,  
P1   =  1.66666666666666019037e-01,  
P2   = -2.77777777770155933842e-03,  
P3   =  6.61375632143793436117e-05,  
P4   = -1.65339022054652515390e-06,  
P5   =  4.13813679705723846039e-08;  

double exp(double x)
{
	double_t hi, lo, c, xx, y;
	int k, sign;
	uint32_t hx;

	GET_HIGH_WORD(hx, x);
	sign = hx>>31;
	hx &= 0x7fffffff;   

	 
	if (hx >= 0x4086232b) {   
		if (isnan(x))
			return x;
		if (x > 709.782712893383973096) {
			 
			x *= 0x1p1023;
			return x;
		}
		if (x < -708.39641853226410622) {
			 
			FORCE_EVAL((float)(-0x1p-149/x));
			if (x < -745.13321910194110842)
				return 0;
		}
	}

	 
	if (hx > 0x3fd62e42) {   
		if (hx >= 0x3ff0a2b2)   
			k = (int)(invln2*x + half[sign]);
		else
			k = 1 - sign - sign;
		hi = x - k*ln2hi;   
		lo = k*ln2lo;
		x = hi - lo;
	} else if (hx > 0x3e300000)  {   
		k = 0;
		hi = x;
		lo = 0;
	} else {
		 
		FORCE_EVAL(0x1p1023 + x);
		return 1 + x;
	}

	 
	xx = x*x;
	c = x - xx*(P1+xx*(P2+xx*(P3+xx*(P4+xx*P5))));
	y = 1 + (x*c/(2-c) - lo + hi);
	if (k == 0)
		return y;
	return scalbn(y, k);
}
