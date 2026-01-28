#ifndef	nettel_h
#define	nettel_h
#ifdef CONFIG_NETtel
#ifdef CONFIG_COLDFIRE
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/io.h>
#endif
#if defined(CONFIG_M5307)
#define	MCFPP_DCD1	0x0001
#define	MCFPP_DCD0	0x0002
#define	MCFPP_DTR1	0x0004
#define	MCFPP_DTR0	0x0008
#define	NETtel_LEDADDR	0x30400000
#ifndef __ASSEMBLY__
extern volatile unsigned short ppdata;
static __inline__ unsigned int mcf_getppdata(void)
{
	volatile unsigned short *pp;
	pp = (volatile unsigned short *) MCFSIM_PADAT;
	return((unsigned int) *pp);
}
static __inline__ void mcf_setppdata(unsigned int mask, unsigned int bits)
{
	volatile unsigned short *pp;
	pp = (volatile unsigned short *) MCFSIM_PADAT;
	ppdata = (ppdata & ~mask) | bits;
	*pp = ppdata;
}
#endif
#elif defined(CONFIG_M5206e)
#define	NETtel_LEDADDR	0x50000000
#elif defined(CONFIG_M5272)
#define	MCFPP_DCD0	0x0080
#define	MCFPP_DCD1	0x0000		 
#define	MCFPP_DTR0	0x0040
#define	MCFPP_DTR1	0x0000		 
#ifndef __ASSEMBLY__
static __inline__ unsigned int mcf_getppdata(void)
{
	return readw(MCFSIM_PBDAT);
}
static __inline__ void mcf_setppdata(unsigned int mask, unsigned int bits)
{
	writew((readw(MCFSIM_PBDAT) & ~mask) | bits, MCFSIM_PBDAT);
}
#endif
#endif
#endif  
#endif	 
