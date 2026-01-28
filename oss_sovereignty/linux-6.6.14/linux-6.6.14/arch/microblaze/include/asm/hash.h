#ifndef _ASM_HASH_H
#define _ASM_HASH_H
#if CONFIG_XILINX_MICROBLAZE0_USE_HW_MUL == 0
#define HAVE_ARCH__HASH_32 1
static inline u32 __attribute_const__ __hash_32(u32 a)
{
#if CONFIG_XILINX_MICROBLAZE0_USE_BARREL
	unsigned int b, c;
	b =  a << 23;
	c = (a << 19) + a;
	a = (a <<  9) + c;
	b += a;
	a <<= 5;
	a += b;		 
	a <<= 3;
	a += c;		 
	a <<= 3;
	return a - b;	 
#else
	unsigned int b, c, d;
	b = a << 4;	 
	c = b << 1;	 
	b += a;		 
	c += b;		 
	c <<= 3;	 
	c -= a;		 
	d = c << 7;	 
	d += b;		 
	d <<= 8;	 
	d += a;		 
	d <<= 1;	 
	d += b;		 
	d <<= 6;	 
	return d + c;	 
#endif
}
#endif  
#endif  
