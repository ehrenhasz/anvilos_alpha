 

 

 

#include "fdlibm.h"

#ifdef __STDC__
static const float 
#else
static float 
#endif
one =  1.0000000000e+00f,  
C1  =  4.1666667908e-02f,  
C2  = -1.3888889225e-03f,  
C3  =  2.4801587642e-05f,  
C4  = -2.7557314297e-07f,  
C5  =  2.0875723372e-09f,  
C6  = -1.1359647598e-11f;  

#ifdef __STDC__
	float __kernel_cosf(float x, float y)
#else
	float __kernel_cosf(x, y)
	float x,y;
#endif
{
	float a,hz,z,r,qx;
	__int32_t ix;
	GET_FLOAT_WORD(ix,x);
	ix &= 0x7fffffff;			 
	if(ix<0x32000000) {			 
	    if(((int)x)==0) return one;		 
	}
	z  = x*x;
	r  = z*(C1+z*(C2+z*(C3+z*(C4+z*(C5+z*C6)))));
	if(ix < 0x3e99999a) 			  
	    return one - ((float)0.5*z - (z*r - x*y));
	else {
	    if(ix > 0x3f480000) {		 
		qx = (float)0.28125;
	    } else {
	        SET_FLOAT_WORD(qx,ix-0x01000000);	 
	    }
	    hz = (float)0.5*z-qx;
	    a  = one-qx;
	    return a - (hz - (z*r-x*y));
	}
}
