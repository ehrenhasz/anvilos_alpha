 

 

 

#include "fdlibm.h"

#ifdef __STDC__
static const float one = 1.0f;
#else
static float one = 1.0f;
#endif

#ifdef __STDC__
	float modff(float x, float *iptr)
#else
	float modff(x, iptr)
	float x,*iptr;
#endif
{
	__int32_t i0,j0;
	__uint32_t i;
	GET_FLOAT_WORD(i0,x);
	j0 = ((i0>>23)&0xff)-0x7f;	 
	if(j0<23) {			 
	    if(j0<0) {			 
	        SET_FLOAT_WORD(*iptr,i0&0x80000000);	 
		return x;
	    } else {
		i = (0x007fffff)>>j0;
		if((i0&i)==0) {			 
		    __uint32_t ix;
		    *iptr = x;
		    GET_FLOAT_WORD(ix,x);
		    SET_FLOAT_WORD(x,ix&0x80000000);	 
		    return x;
		} else {
		    SET_FLOAT_WORD(*iptr,i0&(~i));
		    return x - *iptr;
		}
	    }
	} else {			 
	    __uint32_t ix;
	    *iptr = x*one;
	    GET_FLOAT_WORD(ix,x);
	    SET_FLOAT_WORD(x,ix&0x80000000);	 
	    return x;
	}
}

#ifdef _DOUBLE_IS_32BITS

#ifdef __STDC__
	double modf(double x, double *iptr)
#else
	double modf(x, iptr)
	double x,*iptr;
#endif
{
	return (double) modff((float) x, (float *) iptr);
}

#endif  
