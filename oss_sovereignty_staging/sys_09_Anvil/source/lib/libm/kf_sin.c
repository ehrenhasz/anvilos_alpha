 

 

 

#include "fdlibm.h"

#ifdef __STDC__
static const float 
#else
static float 
#endif
half =  5.0000000000e-01f, 
S1  = -1.6666667163e-01f,  
S2  =  8.3333337680e-03f,  
S3  = -1.9841270114e-04f,  
S4  =  2.7557314297e-06f,  
S5  = -2.5050759689e-08f,  
S6  =  1.5896910177e-10f;  

#ifdef __STDC__
	float __kernel_sinf(float x, float y, int iy)
#else
	float __kernel_sinf(x, y, iy)
	float x,y; int iy;		 
#endif
{
	float z,r,v;
	__int32_t ix;
	GET_FLOAT_WORD(ix,x);
	ix &= 0x7fffffff;			 
	if(ix<0x32000000)			 
	   {if((int)x==0) return x;}		 
	z	=  x*x;
	v	=  z*x;
	r	=  S2+z*(S3+z*(S4+z*(S5+z*S6)));
	if(iy==0) return x+v*(S1+z*r);
	else      return x-((z*(half*y-v*r)-y)-v*S1);
}
