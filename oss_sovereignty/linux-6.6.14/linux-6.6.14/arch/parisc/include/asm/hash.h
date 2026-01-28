#ifndef _ASM_HASH_H
#define _ASM_HASH_H
#define HAVE_ARCH__HASH_32 1
static inline u32 __attribute_const__ __hash_32(u32 x)
{
	u32 a, b, c;
	a = x << 19;		 
	b = x << 9;	a += x;
	c = x << 23;	b += a;
			c += b;
	b <<= 11;
	a += c << 3;	b -= c;
	return (a << 3) + b;
}
#if BITS_PER_LONG == 64
#define HAVE_ARCH_HASH_64 1
#define _ASSIGN(dst, src, ...) asm("" : "=r" (dst) : "0" (src), ##__VA_ARGS__)
static __always_inline u32 __attribute_const__
hash_64(u64 a, unsigned int bits)
{
	u64 b, c, d;
	if (!__builtin_constant_p(bits))
		asm("" : "=q" (bits) : "0" (64 - bits));
	else
		bits = 64 - bits;
	_ASSIGN(b, a*5);	c = a << 13;
	b = (b << 2) + a;	_ASSIGN(d, a << 17);
	a = b + (a << 1);	c += d;
	d = a << 10;		_ASSIGN(a, a << 19);
	d = a - d;		_ASSIGN(a, a << 4, "X" (d));
	c += b;			a += b;
	d -= c;			c += a << 1;
	a += c << 3;		_ASSIGN(b, b << (7+31), "X" (c), "X" (d));
	a <<= 31;		b += d;
	a += b;
	return a >> bits;
}
#undef _ASSIGN	 
#endif  
#endif  
