 
 
 

#include <math.h>
#include <stdint.h>

static const double
ln2_hi = 6.93147180369123816490e-01,   
ln2_lo = 1.90821492927058770002e-10,   
Lg1 = 6.666666666666735130e-01,   
Lg2 = 3.999999999940941908e-01,   
Lg3 = 2.857142874366239149e-01,   
Lg4 = 2.222219843214978396e-01,   
Lg5 = 1.818357216161805012e-01,   
Lg6 = 1.531383769920937332e-01,   
Lg7 = 1.479819860511658591e-01;   

double log(double x)
{
	union {double f; uint64_t i;} u = {x};
	double_t hfsq,f,s,z,R,w,t1,t2,dk;
	uint32_t hx;
	int k;

	hx = u.i>>32;
	k = 0;
	if (hx < 0x00100000 || hx>>31) {
		if (u.i<<1 == 0)
			return -1/(x*x);   
		if (hx>>31)
			return (x-x)/0.0;  
		 
		k -= 54;
		x *= 0x1p54;
		u.f = x;
		hx = u.i>>32;
	} else if (hx >= 0x7ff00000) {
		return x;
	} else if (hx == 0x3ff00000 && u.i<<32 == 0)
		return 0;

	 
	hx += 0x3ff00000 - 0x3fe6a09e;
	k += (int)(hx>>20) - 0x3ff;
	hx = (hx&0x000fffff) + 0x3fe6a09e;
	u.i = (uint64_t)hx<<32 | (u.i&0xffffffff);
	x = u.f;

	f = x - 1.0;
	hfsq = 0.5*f*f;
	s = f/(2.0+f);
	z = s*s;
	w = z*z;
	t1 = w*(Lg2+w*(Lg4+w*Lg6));
	t2 = z*(Lg1+w*(Lg3+w*(Lg5+w*Lg7)));
	R = t2 + t1;
	dk = k;
	return s*(hfsq+R) + dk*ln2_lo - hfsq + f + dk*ln2_hi;
}
