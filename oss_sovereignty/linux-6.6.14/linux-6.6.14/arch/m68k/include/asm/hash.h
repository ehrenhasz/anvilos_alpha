#ifndef _ASM_HASH_H
#define _ASM_HASH_H
#define HAVE_ARCH__HASH_32 1
static inline u32 __attribute_const__ __hash_32(u32 x)
{
	u32 a, b;
	asm(   "move.l %2,%0"	 
	"\n	lsl.l #2,%0"	 
	"\n	move.l %0,%1"
	"\n	lsl.l #7,%0"	 
	"\n	add.l %2,%0"	 
	"\n	add.l %0,%1"	 
	"\n	add.l %0,%0"	 
	"\n	add.l %0,%1"	 
	"\n	lsl.l #5,%0"	 
	: "=&d,d" (a), "=&r,r" (b)
	: "r,roi?" (x));	 
	return ((u16)(x*0x61c8) << 16) + a + b;
}
#endif	 
