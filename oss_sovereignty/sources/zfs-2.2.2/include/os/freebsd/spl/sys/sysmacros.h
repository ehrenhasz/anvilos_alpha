






#ifndef _SYS_SYSMACROS_H
#define	_SYS_SYSMACROS_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/isa_defs.h>
#include <sys/libkern.h>
#include <sys/zone.h>
#include <sys/condvar.h>

#ifdef	__cplusplus
extern "C" {
#endif



#define	dtob(DD)	((DD) << DEV_BSHIFT)
#define	btod(BB)	(((BB) + DEV_BSIZE - 1) >> DEV_BSHIFT)
#define	btodt(BB)	((BB) >> DEV_BSHIFT)
#define	lbtod(BB)	(((offset_t)(BB) + DEV_BSIZE - 1) >> DEV_BSHIFT)


#ifndef MIN
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define	MAX(a, b)	((a) < (b) ? (b) : (a))
#endif
#ifndef ABS
#define	ABS(a)		((a) < 0 ? -(a) : (a))
#endif
#ifndef	SIGNOF
#define	SIGNOF(a)	((a) < 0 ? -1 : (a) > 0)
#endif
#ifndef	ARRAY_SIZE
#define	ARRAY_SIZE(a) (sizeof (a) / sizeof (a[0]))
#endif
#ifndef	DIV_ROUND_UP
#define	DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))
#endif

#ifdef _STANDALONE
#define	boot_ncpus 1
#else 
#define	boot_ncpus mp_ncpus
#endif 
#define	kpreempt_disable() critical_enter()
#define	kpreempt_enable() critical_exit()
#define	CPU_SEQID curcpu
#define	CPU_SEQID_UNSTABLE curcpu
#define	is_system_labeled()		0

extern unsigned char byte_to_bcd[256];
extern unsigned char bcd_to_byte[256];

#define	BYTE_TO_BCD(x)	byte_to_bcd[(x) & 0xff]
#define	BCD_TO_BYTE(x)	bcd_to_byte[(x) & 0xff]



#define	O_BITSMAJOR	7	
#define	O_BITSMINOR	8	
#define	O_MAXMAJ	0x7f	
#define	O_MAXMIN	0xff	


#define	L_BITSMAJOR32	14	
#define	L_BITSMINOR32	18	
#define	L_MAXMAJ32	0x3fff	
#define	L_MAXMIN32	0x3ffff	
				
				

#ifdef _LP64
#define	L_BITSMAJOR	32	
#define	L_BITSMINOR	32	
#define	L_MAXMAJ	0xfffffffful	
#define	L_MAXMIN	0xfffffffful	
#else
#define	L_BITSMAJOR	L_BITSMAJOR32
#define	L_BITSMINOR	L_BITSMINOR32
#define	L_MAXMAJ	L_MAXMAJ32
#define	L_MAXMIN	L_MAXMIN32
#endif


#if (L_BITSMAJOR32 == L_BITSMAJOR) && (L_BITSMINOR32 == L_BITSMINOR)

#define	DEVCMPL(x)	(x)
#define	DEVEXPL(x)	(x)

#else

#define	DEVCMPL(x)	\
	(dev32_t)((((x) >> L_BITSMINOR) > L_MAXMAJ32 || \
	    ((x) & L_MAXMIN) > L_MAXMIN32) ? NODEV32 : \
	    ((((x) >> L_BITSMINOR) << L_BITSMINOR32) | ((x) & L_MAXMIN32)))

#define	DEVEXPL(x)	\
	(((x) == NODEV32) ? NODEV : \
	makedevice(((x) >> L_BITSMINOR32) & L_MAXMAJ32, (x) & L_MAXMIN32))

#endif 



#define	cmpdev(x) \
	(o_dev_t)((((x) >> L_BITSMINOR) > O_MAXMAJ || \
	    ((x) & L_MAXMIN) > O_MAXMIN) ? NODEV : \
	    ((((x) >> L_BITSMINOR) << O_BITSMINOR) | ((x) & O_MAXMIN)))



#define	expdev(x) \
	(dev_t)(((dev_t)(((x) >> O_BITSMINOR) & O_MAXMAJ) << L_BITSMINOR) | \
	    ((x) & O_MAXMIN))


#define	IS_P2ALIGNED(v, a) ((((uintptr_t)(v)) & ((uintptr_t)(a) - 1)) == 0)


#define	howmany(x, y)	(((x)+((y)-1))/(y))
#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))


#define	ISP2(x)		(((x) & ((x) - 1)) == 0)




#define	P2ALIGN(x, align)		((x) & -(align))


#define	P2PHASE(x, align)		((x) & ((align) - 1))


#define	P2NPHASE(x, align)		(-(x) & ((align) - 1))


#define	P2ROUNDUP(x, align)		(-(-(x) & -(align)))


#define	P2END(x, align)			(-(~(x) & -(align)))


#define	P2PHASEUP(x, align, phase)	((phase) - (((phase) - (x)) & -(align)))


#define	P2BOUNDARY(off, len, align) \
	(((off) ^ ((off) + (len) - 1)) > (align) - 1)


#define	P2SAMEHIGHBIT(x, y)		(((x) ^ (y)) < ((x) & (y)))


#define	P2ALIGN_TYPED(x, align, type)	\
	((type)(x) & -(type)(align))
#define	P2PHASE_TYPED(x, align, type)	\
	((type)(x) & ((type)(align) - 1))
#define	P2NPHASE_TYPED(x, align, type)	\
	(-(type)(x) & ((type)(align) - 1))
#define	P2ROUNDUP_TYPED(x, align, type)	\
	(-(-(type)(x) & -(type)(align)))
#define	P2END_TYPED(x, align, type)	\
	(-(~(type)(x) & -(type)(align)))
#define	P2PHASEUP_TYPED(x, align, phase, type)	\
	((type)(phase) - (((type)(phase) - (type)(x)) & -(type)(align)))
#define	P2CROSS_TYPED(x, y, align, type)	\
	(((type)(x) ^ (type)(y)) > (type)(align) - 1)
#define	P2SAMEHIGHBIT_TYPED(x, y, type) \
	(((type)(x) ^ (type)(y)) < ((type)(x) & (type)(y)))


#define	INCR_COUNT(var, mutex) mutex_enter(mutex), (*(var))++, mutex_exit(mutex)
#define	DECR_COUNT(var, mutex) mutex_enter(mutex), (*(var))--, mutex_exit(mutex)

#if !defined(_KMEMUSER) && !defined(offsetof)



#define	offsetof(type, field)	__offsetof(type, field)
#endif


static __inline int
highbit(ulong_t i)
{
#if defined(HAVE_INLINE_FLSL)
	return (flsl(i));
#else
	int h = 1;

	if (i == 0)
		return (0);
#ifdef _LP64
	if (i & 0xffffffff00000000ul) {
		h += 32; i >>= 32;
	}
#endif
	if (i & 0xffff0000) {
		h += 16; i >>= 16;
	}
	if (i & 0xff00) {
		h += 8; i >>= 8;
	}
	if (i & 0xf0) {
		h += 4; i >>= 4;
	}
	if (i & 0xc) {
		h += 2; i >>= 2;
	}
	if (i & 0x2) {
		h += 1;
	}
	return (h);
#endif
}


static __inline int
highbit64(uint64_t i)
{
#if defined(HAVE_INLINE_FLSLL)
	return (flsll(i));
#else
	int h = 1;

	if (i == 0)
		return (0);
	if (i & 0xffffffff00000000ULL) {
		h += 32; i >>= 32;
	}
	if (i & 0xffff0000) {
		h += 16; i >>= 16;
	}
	if (i & 0xff00) {
		h += 8; i >>= 8;
	}
	if (i & 0xf0) {
		h += 4; i >>= 4;
	}
	if (i & 0xc) {
		h += 2; i >>= 2;
	}
	if (i & 0x2) {
		h += 1;
	}
	return (h);
#endif
}

#ifdef	__cplusplus
}
#endif

#endif	
