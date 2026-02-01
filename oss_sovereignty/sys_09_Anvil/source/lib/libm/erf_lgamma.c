 

 

 

#include "fdlibm.h"

#define __ieee754_logf logf

#ifdef __STDC__
static const float 
#else
static float 
#endif
two23=  8.3886080000e+06f,  
half=  5.0000000000e-01f,  
one =  1.0000000000e+00f,  
pi  =  3.1415927410e+00f,  
a0  =  7.7215664089e-02f,  
a1  =  3.2246702909e-01f,  
a2  =  6.7352302372e-02f,  
a3  =  2.0580807701e-02f,  
a4  =  7.3855509982e-03f,  
a5  =  2.8905137442e-03f,  
a6  =  1.1927076848e-03f,  
a7  =  5.1006977446e-04f,  
a8  =  2.2086278477e-04f,  
a9  =  1.0801156895e-04f,  
a10 =  2.5214456400e-05f,  
a11 =  4.4864096708e-05f,  
tc  =  1.4616321325e+00f,  
tf  = -1.2148628384e-01f,  
 
tt  =  6.6971006518e-09f,  
t0  =  4.8383611441e-01f,  
t1  = -1.4758771658e-01f,  
t2  =  6.4624942839e-02f,  
t3  = -3.2788541168e-02f,  
t4  =  1.7970675603e-02f,  
t5  = -1.0314224288e-02f,  
t6  =  6.1005386524e-03f,  
t7  = -3.6845202558e-03f,  
t8  =  2.2596477065e-03f,  
t9  = -1.4034647029e-03f,  
t10 =  8.8108185446e-04f,  
t11 = -5.3859531181e-04f,  
t12 =  3.1563205994e-04f,  
t13 = -3.1275415677e-04f,  
t14 =  3.3552918467e-04f,  
u0  = -7.7215664089e-02f,  
u1  =  6.3282704353e-01f,  
u2  =  1.4549225569e+00f,  
u3  =  9.7771751881e-01f,  
u4  =  2.2896373272e-01f,  
u5  =  1.3381091878e-02f,  
v1  =  2.4559779167e+00f,  
v2  =  2.1284897327e+00f,  
v3  =  7.6928514242e-01f,  
v4  =  1.0422264785e-01f,  
v5  =  3.2170924824e-03f,  
s0  = -7.7215664089e-02f,  
s1  =  2.1498242021e-01f,  
s2  =  3.2577878237e-01f,  
s3  =  1.4635047317e-01f,  
s4  =  2.6642270386e-02f,  
s5  =  1.8402845599e-03f,  
s6  =  3.1947532989e-05f,  
r1  =  1.3920053244e+00f,  
r2  =  7.2193557024e-01f,  
r3  =  1.7193385959e-01f,  
r4  =  1.8645919859e-02f,  
r5  =  7.7794247773e-04f,  
r6  =  7.3266842264e-06f,  
w0  =  4.1893854737e-01f,  
w1  =  8.3333335817e-02f,  
w2  = -2.7777778450e-03f,  
w3  =  7.9365057172e-04f,  
w4  = -5.9518753551e-04f,  
w5  =  8.3633989561e-04f,  
w6  = -1.6309292987e-03f;  

#ifdef __STDC__
static const float zero=  0.0000000000e+00f;
#else
static float zero=  0.0000000000e+00f;
#endif

#ifdef __STDC__
	static float sin_pif(float x)
#else
	static float sin_pif(x)
	float x;
#endif
{
	float y,z;
	__int32_t n,ix;

	GET_FLOAT_WORD(ix,x);
	ix &= 0x7fffffff;

	if(ix<0x3e800000) return __kernel_sinf(pi*x,zero,0);
	y = -x;		 

     
	z = floorf(y);
	if(z!=y) {				 
	    y  *= (float)0.5;
	    y   = (float)2.0*(y - floorf(y));	 
	    n   = (__int32_t) (y*(float)4.0);
	} else {
            if(ix>=0x4b800000) {
                y = zero; n = 0;                  
            } else {
                if(ix<0x4b000000) z = y+two23;	 
		GET_FLOAT_WORD(n,z);
		n &= 1;
                y  = n;
                n<<= 2;
            }
        }
	switch (n) {
	    case 0:   y =  __kernel_sinf(pi*y,zero,0); break;
	    case 1:   
	    case 2:   y =  __kernel_cosf(pi*((float)0.5-y),zero); break;
	    case 3:  
	    case 4:   y =  __kernel_sinf(pi*(one-y),zero,0); break;
	    case 5:
	    case 6:   y = -__kernel_cosf(pi*(y-(float)1.5),zero); break;
	    default:  y =  __kernel_sinf(pi*(y-(float)2.0),zero,0); break;
	    }
	return -y;
}


#ifdef __STDC__
	float __ieee754_lgammaf_r(float x, int *signgamp)
#else
	float __ieee754_lgammaf_r(x,signgamp)
	float x; int *signgamp;
#endif
{
	float t,y,z,nadj = 0.0,p,p1,p2,p3,q,r,w;
	__int32_t i,hx,ix;

	GET_FLOAT_WORD(hx,x);

     
	*signgamp = 1;
	ix = hx&0x7fffffff;
	if(ix>=0x7f800000) return x*x;
	if(ix==0) return one/zero;
	if(ix<0x1c800000) {	 
	    if(hx<0) {
	        *signgamp = -1;
	        return -__ieee754_logf(-x);
	    } else return -__ieee754_logf(x);
	}
	if(hx<0) {
	    if(ix>=0x4b000000) 	 
		return one/zero;
	    t = sin_pif(x);
	    if(t==zero) return one/zero;  
	    nadj = __ieee754_logf(pi/fabsf(t*x));
	    if(t<zero) *signgamp = -1;
	    x = -x;
	}

     
	if (ix==0x3f800000||ix==0x40000000) r = 0;
     
	else if(ix<0x40000000) {
	    if(ix<=0x3f666666) { 	 
		r = -__ieee754_logf(x);
		if(ix>=0x3f3b4a20) {y = one-x; i= 0;}
		else if(ix>=0x3e6d3308) {y= x-(tc-one); i=1;}
	  	else {y = x; i=2;}
	    } else {
	  	r = zero;
	        if(ix>=0x3fdda618) {y=(float)2.0-x;i=0;}  
	        else if(ix>=0x3F9da620) {y=x-tc;i=1;}  
		else {y=x-one;i=2;}
	    }
	    switch(i) {
	      case 0:
		z = y*y;
		p1 = a0+z*(a2+z*(a4+z*(a6+z*(a8+z*a10))));
		p2 = z*(a1+z*(a3+z*(a5+z*(a7+z*(a9+z*a11)))));
		p  = y*p1+p2;
		r  += (p-(float)0.5*y); break;
	      case 1:
		z = y*y;
		w = z*y;
		p1 = t0+w*(t3+w*(t6+w*(t9 +w*t12)));	 
		p2 = t1+w*(t4+w*(t7+w*(t10+w*t13)));
		p3 = t2+w*(t5+w*(t8+w*(t11+w*t14)));
		p  = z*p1-(tt-w*(p2+y*p3));
		r += (tf + p); break;
	      case 2:	
		p1 = y*(u0+y*(u1+y*(u2+y*(u3+y*(u4+y*u5)))));
		p2 = one+y*(v1+y*(v2+y*(v3+y*(v4+y*v5))));
		r += (-(float)0.5*y + p1/p2);
	    }
	}
	else if(ix<0x41000000) { 			 
	    i = (__int32_t)x;
	    t = zero;
	    y = x-(float)i;
	    p = y*(s0+y*(s1+y*(s2+y*(s3+y*(s4+y*(s5+y*s6))))));
	    q = one+y*(r1+y*(r2+y*(r3+y*(r4+y*(r5+y*r6)))));
	    r = half*y+p/q;
	    z = one;	 
	    switch(i) {
	    case 7: z *= (y+(float)6.0);	 
	    case 6: z *= (y+(float)5.0);	 
	    case 5: z *= (y+(float)4.0);	 
	    case 4: z *= (y+(float)3.0);	 
	    case 3: z *= (y+(float)2.0);	 
		    r += __ieee754_logf(z); break;
	    }
     
	} else if (ix < 0x5c800000) {
	    t = __ieee754_logf(x);
	    z = one/x;
	    y = z*z;
	    w = w0+z*(w1+y*(w2+y*(w3+y*(w4+y*(w5+y*w6)))));
	    r = (x-half)*(t-one)+w;
	} else 
     
	    r =  x*(__ieee754_logf(x)-one);
	if(hx<0) r = nadj - r;
	return r;
}
