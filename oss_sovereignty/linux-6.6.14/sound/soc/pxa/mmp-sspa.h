 
 
#ifndef _MMP_SSPA_H
#define _MMP_SSPA_H

 
#define SSPA_D			(0x00)
#define SSPA_ID			(0x04)
#define SSPA_CTL		(0x08)
#define SSPA_SP			(0x0c)
#define SSPA_FIFO_UL		(0x10)
#define SSPA_INT_MASK		(0x14)
#define SSPA_C			(0x18)
#define SSPA_FIFO_NOFS		(0x1c)
#define SSPA_FIFO_SIZE		(0x20)

 
#define	SSPA_CTL_XPH		(1 << 31)	 
#define	SSPA_CTL_XFIG		(1 << 15)	 
#define	SSPA_CTL_JST		(1 << 3)	 
#define	SSPA_CTL_XFRLEN2_MASK	(7 << 24)
#define	SSPA_CTL_XFRLEN2(x)	((x) << 24)	 
#define	SSPA_CTL_XWDLEN2_MASK	(7 << 21)
#define	SSPA_CTL_XWDLEN2(x)	((x) << 21)	 
#define	SSPA_CTL_XDATDLY(x)	((x) << 19)	 
#define	SSPA_CTL_XSSZ2_MASK	(7 << 16)
#define	SSPA_CTL_XSSZ2(x)	((x) << 16)	 
#define	SSPA_CTL_XFRLEN1_MASK	(7 << 8)
#define	SSPA_CTL_XFRLEN1(x)	((x) << 8)	 
#define	SSPA_CTL_XWDLEN1_MASK	(7 << 5)
#define	SSPA_CTL_XWDLEN1(x)	((x) << 5)	 
#define	SSPA_CTL_XSSZ1_MASK	(7 << 0)
#define	SSPA_CTL_XSSZ1(x)	((x) << 0)	 

#define SSPA_CTL_8_BITS		(0x0)		 
#define SSPA_CTL_12_BITS	(0x1)
#define SSPA_CTL_16_BITS	(0x2)
#define SSPA_CTL_20_BITS	(0x3)
#define SSPA_CTL_24_BITS	(0x4)
#define SSPA_CTL_32_BITS	(0x5)

 
#define	SSPA_SP_WEN		(1 << 31)	 
#define	SSPA_SP_MSL		(1 << 18)	 
#define	SSPA_SP_CLKP		(1 << 17)	 
#define	SSPA_SP_FSP		(1 << 16)	 
#define	SSPA_SP_FFLUSH		(1 << 2)	 
#define	SSPA_SP_S_RST		(1 << 1)	 
#define	SSPA_SP_S_EN		(1 << 0)	 
#define	SSPA_SP_FWID_MASK	(0x3f << 20)
#define	SSPA_SP_FWID(x)		((x) << 20)	 
#define	SSPA_TXSP_FPER_MASK	(0x3f << 4)
#define	SSPA_TXSP_FPER(x)	((x) << 4)	 

 
#define MMP_SSPA_CLK_PLL	0
#define MMP_SSPA_CLK_VCXO	1
#define MMP_SSPA_CLK_AUDIO	3

 
#define MMP_SYSCLK		0
#define MMP_SSPA_CLK		1

#endif  
