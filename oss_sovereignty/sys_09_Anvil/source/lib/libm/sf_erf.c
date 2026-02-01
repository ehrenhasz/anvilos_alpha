 

 

 

#include "fdlibm.h"

#define __ieee754_expf expf

#ifdef __v810__
#define const
#endif

#ifdef __STDC__
static const float
#else
static float
#endif
tiny	    = 1e-30f,
half=  5.0000000000e-01f,  
one =  1.0000000000e+00f,  
two =  2.0000000000e+00f,  
	 
erx =  8.4506291151e-01f,  
 
efx =  1.2837916613e-01f,  
efx8=  1.0270333290e+00f,  
pp0  =  1.2837916613e-01f,  
pp1  = -3.2504209876e-01f,  
pp2  = -2.8481749818e-02f,  
pp3  = -5.7702702470e-03f,  
pp4  = -2.3763017452e-05f,  
qq1  =  3.9791721106e-01f,  
qq2  =  6.5022252500e-02f,  
qq3  =  5.0813062117e-03f,  
qq4  =  1.3249473704e-04f,  
qq5  = -3.9602282413e-06f,  
 
pa0  = -2.3621185683e-03f,  
pa1  =  4.1485610604e-01f,  
pa2  = -3.7220788002e-01f,  
pa3  =  3.1834661961e-01f,  
pa4  = -1.1089469492e-01f,  
pa5  =  3.5478305072e-02f,  
pa6  = -2.1663755178e-03f,  
qa1  =  1.0642088205e-01f,  
qa2  =  5.4039794207e-01f,  
qa3  =  7.1828655899e-02f,  
qa4  =  1.2617121637e-01f,  
qa5  =  1.3637083583e-02f,  
qa6  =  1.1984500103e-02f,  
 
ra0  = -9.8649440333e-03f,  
ra1  = -6.9385856390e-01f,  
ra2  = -1.0558626175e+01f,  
ra3  = -6.2375331879e+01f,  
ra4  = -1.6239666748e+02f,  
ra5  = -1.8460508728e+02f,  
ra6  = -8.1287437439e+01f,  
ra7  = -9.8143291473e+00f,  
sa1  =  1.9651271820e+01f,  
sa2  =  1.3765776062e+02f,  
sa3  =  4.3456588745e+02f,  
sa4  =  6.4538726807e+02f,  
sa5  =  4.2900814819e+02f,  
sa6  =  1.0863500214e+02f,  
sa7  =  6.5702495575e+00f,  
sa8  = -6.0424413532e-02f,  
 
rb0  = -9.8649431020e-03f,  
rb1  = -7.9928326607e-01f,  
rb2  = -1.7757955551e+01f,  
rb3  = -1.6063638306e+02f,  
rb4  = -6.3756646729e+02f,  
rb5  = -1.0250950928e+03f,  
rb6  = -4.8351919556e+02f,  
sb1  =  3.0338060379e+01f,  
sb2  =  3.2579251099e+02f,  
sb3  =  1.5367296143e+03f,  
sb4  =  3.1998581543e+03f,  
sb5  =  2.5530502930e+03f,  
sb6  =  4.7452853394e+02f,  
sb7  = -2.2440952301e+01f;  

#ifdef __STDC__
	float erff(float x) 
#else
	float erff(x) 
	float x;
#endif
{
	__int32_t hx,ix,i;
	float R,S,P,Q,s,y,z,r;
	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(!FLT_UWORD_IS_FINITE(ix)) {		 
	    i = ((__uint32_t)hx>>31)<<1;
	    return (float)(1-i)+one/x;	 
	}

	if(ix < 0x3f580000) {		 
	    if(ix < 0x31800000) { 	 
	        if (ix < 0x04000000) 
		     
		    return (float)0.125*((float)8.0*x+efx8*x);
		return x + efx*x;
	    }
	    z = x*x;
	    r = pp0+z*(pp1+z*(pp2+z*(pp3+z*pp4)));
	    s = one+z*(qq1+z*(qq2+z*(qq3+z*(qq4+z*qq5))));
	    y = r/s;
	    return x + x*y;
	}
	if(ix < 0x3fa00000) {		 
	    s = fabsf(x)-one;
	    P = pa0+s*(pa1+s*(pa2+s*(pa3+s*(pa4+s*(pa5+s*pa6)))));
	    Q = one+s*(qa1+s*(qa2+s*(qa3+s*(qa4+s*(qa5+s*qa6)))));
	    if(hx>=0) return erx + P/Q; else return -erx - P/Q;
	}
	if (ix >= 0x40c00000) {		 
	    if(hx>=0) return one-tiny; else return tiny-one;
	}
	x = fabsf(x);
 	s = one/(x*x);
	if(ix< 0x4036DB6E) {	 
	    R=ra0+s*(ra1+s*(ra2+s*(ra3+s*(ra4+s*(
				ra5+s*(ra6+s*ra7))))));
	    S=one+s*(sa1+s*(sa2+s*(sa3+s*(sa4+s*(
				sa5+s*(sa6+s*(sa7+s*sa8)))))));
	} else {	 
	    R=rb0+s*(rb1+s*(rb2+s*(rb3+s*(rb4+s*(
				rb5+s*rb6)))));
	    S=one+s*(sb1+s*(sb2+s*(sb3+s*(sb4+s*(
				sb5+s*(sb6+s*sb7))))));
	}
	GET_FLOAT_WORD(ix,x);
	SET_FLOAT_WORD(z,ix&0xfffff000);
	r  =  __ieee754_expf(-z*z-(float)0.5625)*__ieee754_expf((z-x)*(z+x)+R/S);
	if(hx>=0) return one-r/x; else return  r/x-one;
}

#ifdef __STDC__
	float erfcf(float x) 
#else
	float erfcf(x) 
	float x;
#endif
{
	__int32_t hx,ix;
	float R,S,P,Q,s,y,z,r;
	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(!FLT_UWORD_IS_FINITE(ix)) {			 
						 
	    return (float)(((__uint32_t)hx>>31)<<1)+one/x;
	}

	if(ix < 0x3f580000) {		 
	    if(ix < 0x23800000)  	 
		return one-x;
	    z = x*x;
	    r = pp0+z*(pp1+z*(pp2+z*(pp3+z*pp4)));
	    s = one+z*(qq1+z*(qq2+z*(qq3+z*(qq4+z*qq5))));
	    y = r/s;
	    if(hx < 0x3e800000) {  	 
		return one-(x+x*y);
	    } else {
		r = x*y;
		r += (x-half);
	        return half - r ;
	    }
	}
	if(ix < 0x3fa00000) {		 
	    s = fabsf(x)-one;
	    P = pa0+s*(pa1+s*(pa2+s*(pa3+s*(pa4+s*(pa5+s*pa6)))));
	    Q = one+s*(qa1+s*(qa2+s*(qa3+s*(qa4+s*(qa5+s*qa6)))));
	    if(hx>=0) {
	        z  = one-erx; return z - P/Q; 
	    } else {
		z = erx+P/Q; return one+z;
	    }
	}
	if (ix < 0x41e00000) {		 
	    x = fabsf(x);
 	    s = one/(x*x);
	    if(ix< 0x4036DB6D) {	 
	        R=ra0+s*(ra1+s*(ra2+s*(ra3+s*(ra4+s*(
				ra5+s*(ra6+s*ra7))))));
	        S=one+s*(sa1+s*(sa2+s*(sa3+s*(sa4+s*(
				sa5+s*(sa6+s*(sa7+s*sa8)))))));
	    } else {			 
		if(hx<0&&ix>=0x40c00000) return two-tiny; 
	        R=rb0+s*(rb1+s*(rb2+s*(rb3+s*(rb4+s*(
				rb5+s*rb6)))));
	        S=one+s*(sb1+s*(sb2+s*(sb3+s*(sb4+s*(
				sb5+s*(sb6+s*sb7))))));
	    }
	    GET_FLOAT_WORD(ix,x);
	    SET_FLOAT_WORD(z,ix&0xfffff000);
	    r  =  __ieee754_expf(-z*z-(float)0.5625)*
			__ieee754_expf((z-x)*(z+x)+R/S);
	    if(hx>0) return r/x; else return two-r/x;
	} else {
	    if(hx>0) return tiny*tiny; else return two-tiny;
	}
}

#ifdef _DOUBLE_IS_32BITS

#ifdef __STDC__
	double erf(double x)
#else
	double erf(x)
	double x;
#endif
{
	return (double) erff((float) x);
}

#ifdef __STDC__
	double erfc(double x)
#else
	double erfc(x)
	double x;
#endif
{
	return (double) erfcf((float) x);
}

#endif  
