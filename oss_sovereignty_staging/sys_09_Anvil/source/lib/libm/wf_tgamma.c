 

 

 

#include "math.h"
#include "fdlibm.h"
#define _IEEE_LIBM 1

#ifdef __STDC__
	float tgammaf(float x)
#else
	float tgammaf(x)
	float x;
#endif
{
        float y;
	int local_signgam;
	if (!isfinite(x)) {
	   
	  return x + INFINITY;
	}
	y = expf(__ieee754_lgammaf_r(x,&local_signgam));
	if (local_signgam < 0) y = -y;
#ifdef _IEEE_LIBM
	return y;
#else
	if(_LIB_VERSION == _IEEE_) return y;

	if(!finitef(y)&&finitef(x)) {
	  if(floorf(x)==x&&x<=(float)0.0)
	     
	    return (float)__kernel_standard((double)x,(double)x,141);
	  else
	     
	    return (float)__kernel_standard((double)x,(double)x,140);
	}
	return y;
#endif
}

#ifdef _DOUBLE_IS_32BITS

#ifdef __STDC__
	double tgamma(double x)
#else
	double tgamma(x)
	double x;
#endif
{
	return (double) tgammaf((float) x);
}

#endif  
