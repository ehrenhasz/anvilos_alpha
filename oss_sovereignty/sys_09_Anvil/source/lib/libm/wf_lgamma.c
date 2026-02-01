 

 

 

#include "fdlibm.h"
#define _IEEE_LIBM 1

#ifdef __STDC__
	float lgammaf(float x)
#else
	float lgammaf(x)
	float x;
#endif
{
#ifdef _IEEE_LIBM
        int sign;
	return __ieee754_lgammaf_r(x,&sign);
#else
        float y;
	struct exception exc;
        y = __ieee754_lgammaf_r(x,&(_REENT_SIGNGAM(_REENT)));
        if(_LIB_VERSION == _IEEE_) return y;
        if(!finitef(y)&&finitef(x)) {
#ifndef HUGE_VAL 
#define HUGE_VAL inf
	    double inf = 0.0;

	    SET_HIGH_WORD(inf,0x7ff00000);	 
#endif
	    exc.name = "lgammaf";
	    exc.err = 0;
	    exc.arg1 = exc.arg2 = (double)x;
            if (_LIB_VERSION == _SVID_)
               exc.retval = HUGE;
            else
               exc.retval = HUGE_VAL;
	    if(floorf(x)==x&&x<=(float)0.0) {
		 
		exc.type = SING;
		if (_LIB_VERSION == _POSIX_)
		   errno = EDOM;
		else if (!matherr(&exc)) {
		   errno = EDOM;
		}

            } else {
		 
		exc.type = OVERFLOW;
                if (_LIB_VERSION == _POSIX_)
		   errno = ERANGE;
                else if (!matherr(&exc)) {
                   errno = ERANGE;
		}
            }
	    if (exc.err != 0)
	       errno = exc.err;
            return (float)exc.retval; 
        } else
            return y;
#endif
}             

#ifdef _DOUBLE_IS_32BITS

#ifdef __STDC__
	double lgamma(double x)
#else
	double lgamma(x)
	double x;
#endif
{
	return (double) lgammaf((float) x);
}

#endif  
