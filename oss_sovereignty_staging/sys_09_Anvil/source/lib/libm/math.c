 

#include "libm.h"

typedef float float_t;
typedef union {
    float f;
    struct {
        uint32_t m : 23;
        uint32_t e : 8;
        uint32_t s : 1;
    };
} float_s_t;

int __signbitf(float f) {
    float_s_t u = {.f = f};
    return u.s;
}

#ifndef NDEBUG
float copysignf(float x, float y) {
    float_s_t fx={.f = x};
    float_s_t fy={.f = y};

    
    fx.s = fy.s;

    return fx.f;
}
#endif

static const float _M_LN10 = 2.30258509299404f; 
float log10f(float x) { return logf(x) / (float)_M_LN10; }

float tanhf(float x) {
    int sign = 0;
    if (x < 0) {
        sign = 1;
        x = -x;
    }
    x = expm1f(-2 * x);
    x = x / (x + 2);
    return sign ? x : -x;
}

 
 

 
 

int __fpclassifyf(float x)
{
	union {float f; uint32_t i;} u = {x};
	int e = u.i>>23 & 0xff;
	if (!e) return u.i<<1 ? FP_SUBNORMAL : FP_ZERO;
	if (e==0xff) return u.i<<9 ? FP_NAN : FP_INFINITE;
	return FP_NORMAL;
}

 
 

 
 

float scalbnf(float x, int n)
{
	union {float f; uint32_t i;} u;
	float_t y = x;

	if (n > 127) {
		y *= 0x1p127f;
		n -= 127;
		if (n > 127) {
			y *= 0x1p127f;
			n -= 127;
			if (n > 127)
				n = 127;
		}
	} else if (n < -126) {
		y *= 0x1p-126f;
		n += 126;
		if (n < -126) {
			y *= 0x1p-126f;
			n += 126;
			if (n < -126)
				n = -126;
		}
	}
	u.i = (uint32_t)(0x7f+n)<<23;
	x = y * u.f;
	return x;
}

 
 

 
 

 
 
 

static const float
bp[]   = {1.0f, 1.5f,},
dp_h[] = { 0.0f, 5.84960938e-01f,},  
dp_l[] = { 0.0f, 1.56322085e-06f,},  
two24  =  16777216.0f,   
huge   =  1.0e30f,
tiny   =  1.0e-30f,
 
L1 =  6.0000002384e-01f,  
L2 =  4.2857143283e-01f,  
L3 =  3.3333334327e-01f,  
L4 =  2.7272811532e-01f,  
L5 =  2.3066075146e-01f,  
L6 =  2.0697501302e-01f,  
P1 =  1.6666667163e-01f,  
P2 = -2.7777778450e-03f,  
P3 =  6.6137559770e-05f,  
P4 = -1.6533901999e-06f,  
P5 =  4.1381369442e-08f,  
lg2     =  6.9314718246e-01f,  
lg2_h   =  6.93145752e-01f,    
lg2_l   =  1.42860654e-06f,    
ovt     =  4.2995665694e-08f,  
cp      =  9.6179670095e-01f,  
cp_h    =  9.6191406250e-01f,  
cp_l    = -1.1736857402e-04f,  
ivln2   =  1.4426950216e+00f,  
ivln2_h =  1.4426879883e+00f,  
ivln2_l =  7.0526075433e-06f;  

float powf(float x, float y)
{
	float z,ax,z_h,z_l,p_h,p_l;
	float y1,t1,t2,r,s,sn,t,u,v,w;
	int32_t i,j,k,yisint,n;
	int32_t hx,hy,ix,iy,is;

	GET_FLOAT_WORD(hx, x);
	GET_FLOAT_WORD(hy, y);
	ix = hx & 0x7fffffff;
	iy = hy & 0x7fffffff;

	 
	if (iy == 0)
		return 1.0f;
	 
	if (hx == 0x3f800000)
		return 1.0f;
	 
	if (ix > 0x7f800000 || iy > 0x7f800000)
		return x + y;

	 
	yisint  = 0;
	if (hx < 0) {
		if (iy >= 0x4b800000)
			yisint = 2;  
		else if (iy >= 0x3f800000) {
			k = (iy>>23) - 0x7f;          
			j = iy>>(23-k);
			if ((j<<(23-k)) == iy)
				yisint = 2 - (j & 1);
		}
	}

	 
	if (iy == 0x7f800000) {   
		if (ix == 0x3f800000)       
			return 1.0f;
		else if (ix > 0x3f800000)   
			return hy >= 0 ? y : 0.0f;
		else if (ix != 0)           
			return hy >= 0 ? 0.0f: -y;
	}
	if (iy == 0x3f800000)     
		return hy >= 0 ? x : 1.0f/x;
	if (hy == 0x40000000)     
		return x*x;
	if (hy == 0x3f000000) {   
		if (hx >= 0)      
			return sqrtf(x);
	}

	ax = fabsf(x);
	 
	if (ix == 0x7f800000 || ix == 0 || ix == 0x3f800000) {  
		z = ax;
		if (hy < 0)   
			z = 1.0f/z;
		if (hx < 0) {
			if (((ix-0x3f800000)|yisint) == 0) {
				z = (z-z)/(z-z);  
			} else if (yisint == 1)
				z = -z;           
		}
		return z;
	}

	sn = 1.0f;  
	if (hx < 0) {
		if (yisint == 0)  
			return (x-x)/(x-x);
		if (yisint == 1)  
			sn = -1.0f;
	}

	 
	if (iy > 0x4d000000) {  
		 
		if (ix < 0x3f7ffff8)
			return hy < 0 ? sn*huge*huge : sn*tiny*tiny;
		if (ix > 0x3f800007)
			return hy > 0 ? sn*huge*huge : sn*tiny*tiny;
		 
		t = ax - 1;      
		w = (t*t)*(0.5f - t*(0.333333333333f - t*0.25f));
		u = ivln2_h*t;   
		v = t*ivln2_l - w*ivln2;
		t1 = u + v;
		GET_FLOAT_WORD(is, t1);
		SET_FLOAT_WORD(t1, is & 0xfffff000);
		t2 = v - (t1-u);
	} else {
		float s2,s_h,s_l,t_h,t_l;
		n = 0;
		 
		if (ix < 0x00800000) {
			ax *= two24;
			n -= 24;
			GET_FLOAT_WORD(ix, ax);
		}
		n += ((ix)>>23) - 0x7f;
		j = ix & 0x007fffff;
		 
		ix = j | 0x3f800000;      
		if (j <= 0x1cc471)        
			k = 0;
		else if (j < 0x5db3d7)    
			k = 1;
		else {
			k = 0;
			n += 1;
			ix -= 0x00800000;
		}
		SET_FLOAT_WORD(ax, ix);

		 
		u = ax - bp[k];    
		v = 1.0f/(ax+bp[k]);
		s = u*v;
		s_h = s;
		GET_FLOAT_WORD(is, s_h);
		SET_FLOAT_WORD(s_h, is & 0xfffff000);
		 
		is = ((ix>>1) & 0xfffff000) | 0x20000000;
		SET_FLOAT_WORD(t_h, is + 0x00400000 + (k<<21));
		t_l = ax - (t_h - bp[k]);
		s_l = v*((u - s_h*t_h) - s_h*t_l);
		 
		s2 = s*s;
		r = s2*s2*(L1+s2*(L2+s2*(L3+s2*(L4+s2*(L5+s2*L6)))));
		r += s_l*(s_h+s);
		s2 = s_h*s_h;
		t_h = 3.0f + s2 + r;
		GET_FLOAT_WORD(is, t_h);
		SET_FLOAT_WORD(t_h, is & 0xfffff000);
		t_l = r - ((t_h - 3.0f) - s2);
		 
		u = s_h*t_h;
		v = s_l*t_h + t_l*s;
		 
		p_h = u + v;
		GET_FLOAT_WORD(is, p_h);
		SET_FLOAT_WORD(p_h, is & 0xfffff000);
		p_l = v - (p_h - u);
		z_h = cp_h*p_h;   
		z_l = cp_l*p_h + p_l*cp+dp_l[k];
		 
		t = (float)n;
		t1 = (((z_h + z_l) + dp_h[k]) + t);
		GET_FLOAT_WORD(is, t1);
		SET_FLOAT_WORD(t1, is & 0xfffff000);
		t2 = z_l - (((t1 - t) - dp_h[k]) - z_h);
	}

	 
	GET_FLOAT_WORD(is, y);
	SET_FLOAT_WORD(y1, is & 0xfffff000);
	p_l = (y-y1)*t1 + y*t2;
	p_h = y1*t1;
	z = p_l + p_h;
	GET_FLOAT_WORD(j, z);
	if (j > 0x43000000)           
		return sn*huge*huge;   
	else if (j == 0x43000000) {   
		if (p_l + ovt > z - p_h)
			return sn*huge*huge;   
	} else if ((j&0x7fffffff) > 0x43160000)    
		return sn*tiny*tiny;   
	else if (j == 0xc3160000) {   
		if (p_l <= z-p_h)
			return sn*tiny*tiny;   
	}
	 
	i = j & 0x7fffffff;
	k = (i>>23) - 0x7f;
	n = 0;
	if (i > 0x3f000000) {    
		n = j + (0x00800000>>(k+1));
		k = ((n&0x7fffffff)>>23) - 0x7f;   
		SET_FLOAT_WORD(t, n & ~(0x007fffff>>k));
		n = ((n&0x007fffff)|0x00800000)>>(23-k);
		if (j < 0)
			n = -n;
		p_h -= t;
	}
	t = p_l + p_h;
	GET_FLOAT_WORD(is, t);
	SET_FLOAT_WORD(t, is & 0xffff8000);
	u = t*lg2_h;
	v = (p_l-(t-p_h))*lg2 + t*lg2_l;
	z = u + v;
	w = v - (z - u);
	t = z*z;
	t1 = z - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))));
	r = (z*t1)/(t1-2.0f) - (w+z*w);
	z = 1.0f - (r - z);
	GET_FLOAT_WORD(j, z);
	j += n<<23;
	if ((j>>23) <= 0)   
		z = scalbnf(z, n);
	else
		SET_FLOAT_WORD(z, j);
	return sn*z;
}

 
 

 
 

 
 
 

static const float
half[2] = {0.5f,-0.5f},
ln2hi   = 6.9314575195e-1f,   
ln2lo   = 1.4286067653e-6f,   
invln2  = 1.4426950216e+0f,   
 
expf_P1 =  1.6666625440e-1f,  
expf_P2 = -2.7667332906e-3f;  

float expf(float x)
{
	float_t hi, lo, c, xx, y;
	int k, sign;
	uint32_t hx;

	GET_FLOAT_WORD(hx, x);
	sign = hx >> 31;    
	hx &= 0x7fffffff;   

	 
	if (hx >= 0x42aeac50) {   
		if (hx >= 0x42b17218 && !sign) {   
			 
			x *= 0x1p127f;
			return x;
		}
		if (sign) {
			 
			FORCE_EVAL(-0x1p-149f/x);
			if (hx >= 0x42cff1b5)   
				return 0;
		}
	}

	 
	if (hx > 0x3eb17218) {   
		if (hx > 0x3f851592)   
			k = (int)(invln2*x + half[sign]);
		else
			k = 1 - sign - sign;
		hi = x - k*ln2hi;   
		lo = k*ln2lo;
		x = hi - lo;
	} else if (hx > 0x39000000) {   
		k = 0;
		hi = x;
		lo = 0;
	} else {
		 
		FORCE_EVAL(0x1p127f + x);
		return 1 + x;
	}

	 
	xx = x*x;
	c = x - xx*(expf_P1+xx*expf_P2);
	y = 1 + (x*c/(2-c) - lo + hi);
	if (k == 0)
		return y;
	return scalbnf(y, k);
}

 
 

 
 

 
 
 

static const float
o_threshold = 8.8721679688e+01f,  
ln2_hi      = 6.9313812256e-01f,  
ln2_lo      = 9.0580006145e-06f,  

 
Q1 = -3.3333212137e-2f,  
Q2 =  1.5807170421e-3f;  

float expm1f(float x)
{
	float_t y,hi,lo,c,t,e,hxs,hfx,r1,twopk;
	union {float f; uint32_t i;} u = {x};
	uint32_t hx = u.i & 0x7fffffff;
	int k, sign = u.i >> 31;

	 
	if (hx >= 0x4195b844) {   
		if (hx > 0x7f800000)   
			return x;
		if (sign)
			return -1;
		if (x > o_threshold) {
			x *= 0x1p127f;
			return x;
		}
	}

	 
	if (hx > 0x3eb17218) {            
		if (hx < 0x3F851592) {        
			if (!sign) {
				hi = x - ln2_hi;
				lo = ln2_lo;
				k =  1;
			} else {
				hi = x + ln2_hi;
				lo = -ln2_lo;
				k = -1;
			}
		} else {
			k  = (int)(invln2*x + (sign ? -0.5f : 0.5f));
			t  = k;
			hi = x - t*ln2_hi;       
			lo = t*ln2_lo;
		}
		x = hi-lo;
		c = (hi-x)-lo;
	} else if (hx < 0x33000000) {   
		if (hx < 0x00800000)
			FORCE_EVAL(x*x);
		return x;
	} else
		k = 0;

	 
	hfx = 0.5f*x;
	hxs = x*hfx;
	r1 = 1.0f+hxs*(Q1+hxs*Q2);
	t  = 3.0f - r1*hfx;
	e  = hxs*((r1-t)/(6.0f - x*t));
	if (k == 0)   
		return x - (x*e-hxs);
	e  = x*(e-c) - c;
	e -= hxs;
	 
	if (k == -1)
		return 0.5f*(x-e) - 0.5f;
	if (k == 1) {
		if (x < -0.25f)
			return -2.0f*(e-(x+0.5f));
		return 1.0f + 2.0f*(x-e);
	}
	u.i = (0x7f+k)<<23;   
	twopk = u.f;
	if (k < 0 || k > 56) {    
		y = x - e + 1.0f;
		if (k == 128)
			y = y*2.0f*0x1p127f;
		else
			y = y*twopk;
		return y - 1.0f;
	}
	u.i = (0x7f-k)<<23;   
	if (k < 23)
		y = (x-e+(1-u.f))*twopk;
	else
		y = (x-(e+u.f)+1)*twopk;
	return y;
}

 
 

 
 

 
static const int k = 235;
static const float kln2 = 0x1.45c778p+7f;

 
float __expo2f(float x)
{
	float scale;

	 
	SET_FLOAT_WORD(scale, (uint32_t)(0x7f + k/2) << 23);
	 
	return expf(x - kln2) * scale * scale;
}

 
 

 
 

 
 
 

static const float
 
Lg1 = 0xaaaaaa.0p-24,  
Lg2 = 0xccce13.0p-25,  
Lg3 = 0x91e9ee.0p-25,  
Lg4 = 0xf89e26.0p-26;  

float logf(float x)
{
	union {float f; uint32_t i;} u = {x};
	float_t hfsq,f,s,z,R,w,t1,t2,dk;
	uint32_t ix;
	int k;

	ix = u.i;
	k = 0;
	if (ix < 0x00800000 || ix>>31) {   
		if (ix<<1 == 0)
			return -1/(x*x);   
		if (ix>>31)
			return (x-x)/0.0f;  
		 
		k -= 25;
		x *= 0x1p25f;
		u.f = x;
		ix = u.i;
	} else if (ix >= 0x7f800000) {
		return x;
	} else if (ix == 0x3f800000)
		return 0;

	 
	ix += 0x3f800000 - 0x3f3504f3;
	k += (int)(ix>>23) - 0x7f;
	ix = (ix&0x007fffff) + 0x3f3504f3;
	u.i = ix;
	x = u.f;

	f = x - 1.0f;
	s = f/(2.0f + f);
	z = s*s;
	w = z*z;
	t1= w*(Lg2+w*Lg4);
	t2= z*(Lg1+w*Lg3);
	R = t2 + t1;
	hfsq = 0.5f*f*f;
	dk = k;
	return s*(hfsq+R) + dk*ln2_lo - hfsq + f + dk*ln2_hi;
}

 
 

 
 

float coshf(float x)
{
	union {float f; uint32_t i;} u = {.f = x};
	uint32_t w;
	float t;

	 
	u.i &= 0x7fffffff;
	x = u.f;
	w = u.i;

	 
	if (w < 0x3f317217) {
		if (w < 0x3f800000 - (12<<23)) {
			FORCE_EVAL(x + 0x1p120f);
			return 1;
		}
		t = expm1f(x);
		return 1 + t*t/(2*(1+t));
	}

	 
	if (w < 0x42b17217) {
		t = expf(x);
		return 0.5f*(t + 1/t);
	}

	 
	t = __expo2f(x);
	return t;
}

 
 

 
 

float sinhf(float x)
{
	union {float f; uint32_t i;} u = {.f = x};
	uint32_t w;
	float t, h, absx;

	h = 0.5;
	if (u.i >> 31)
		h = -h;
	 
	u.i &= 0x7fffffff;
	absx = u.f;
	w = u.i;

	 
	if (w < 0x42b17217) {
		t = expm1f(absx);
		if (w < 0x3f800000) {
			if (w < 0x3f800000 - (12<<23))
				return x;
			return h*(2*t - t*t/(t+1));
		}
		return h*(t + t/(t+1));
	}

	 
	t = 2*h*__expo2f(absx);
	return t;
}

 
 

 
 

float ceilf(float x)
{
	union {float f; uint32_t i;} u = {x};
	int e = (int)(u.i >> 23 & 0xff) - 0x7f;
	uint32_t m;

	if (e >= 23)
		return x;
	if (e >= 0) {
		m = 0x007fffff >> e;
		if ((u.i & m) == 0)
			return x;
		FORCE_EVAL(x + 0x1p120f);
		if (u.i >> 31 == 0)
			u.i += m;
		u.i &= ~m;
	} else {
		FORCE_EVAL(x + 0x1p120f);
		if (u.i >> 31)
			u.f = -0.0;
		else if (u.i << 1)
			u.f = 1.0;
	}
	return u.f;
}

float floorf(float x)
{
	union {float f; uint32_t i;} u = {x};
	int e = (int)(u.i >> 23 & 0xff) - 0x7f;
	uint32_t m;

	if (e >= 23)
		return x;
	if (e >= 0) {
		m = 0x007fffff >> e;
		if ((u.i & m) == 0)
			return x;
		FORCE_EVAL(x + 0x1p120f);
		if (u.i >> 31)
			u.i += m;
		u.i &= ~m;
	} else {
		FORCE_EVAL(x + 0x1p120f);
		if (u.i >> 31 == 0)
			u.i = 0;
		else if (u.i << 1)
			u.f = -1.0;
	}
	return u.f;
}

float truncf(float x)
{
	union {float f; uint32_t i;} u = {x};
	int e = (int)(u.i >> 23 & 0xff) - 0x7f + 9;
	uint32_t m;

	if (e >= 23 + 9)
		return x;
	if (e < 9)
		e = 1;
	m = -1U >> e;
	if ((u.i & m) == 0)
		return x;
	FORCE_EVAL(x + 0x1p120f);
	u.i &= ~m;
	return u.f;
}
