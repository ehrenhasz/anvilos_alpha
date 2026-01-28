#ifndef	coldfire_h
#define	coldfire_h
#ifdef CONFIG_CLOCK_FREQ
#define	MCF_CLK		CONFIG_CLOCK_FREQ
#else
#error "Don't know what your ColdFire CPU clock frequency is??"
#endif
#ifdef CONFIG_MBAR
#define	MCF_MBAR	CONFIG_MBAR
#endif
#ifdef CONFIG_IPSBAR
#define	MCF_IPSBAR	CONFIG_IPSBAR
#endif
#endif	 
