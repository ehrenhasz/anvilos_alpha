


#ifndef AT91_WDT_H
#define AT91_WDT_H

#include <linux/bits.h>

#define AT91_WDT_CR		0x00			
#define  AT91_WDT_WDRSTT	BIT(0)			
#define  AT91_WDT_KEY		(0xa5UL << 24)		

#define AT91_WDT_MR		0x04			
#define  AT91_WDT_WDV		(0xfffUL << 0)		
#define  AT91_WDT_SET_WDV(x)	((x) & AT91_WDT_WDV)
#define  AT91_SAM9X60_PERIODRST	BIT(4)		
#define  AT91_SAM9X60_RPTHRST	BIT(5)		
#define  AT91_WDT_WDFIEN	BIT(12)		
#define  AT91_SAM9X60_WDDIS	BIT(12)		
#define  AT91_WDT_WDRSTEN	BIT(13)		
#define  AT91_WDT_WDRPROC	BIT(14)		
#define  AT91_WDT_WDDIS		BIT(15)		
#define  AT91_WDT_WDD		(0xfffUL << 16)		
#define  AT91_WDT_SET_WDD(x)	(((x) << 16) & AT91_WDT_WDD)
#define  AT91_WDT_WDDBGHLT	BIT(28)		
#define  AT91_WDT_WDIDLEHLT	BIT(29)		

#define AT91_WDT_SR		0x08		
#define  AT91_WDT_WDUNF		BIT(0)		
#define  AT91_WDT_WDERR		BIT(1)		


#define AT91_SAM9X60_VR		0x08


#define AT91_SAM9X60_WLR	0x0c

#define  AT91_SAM9X60_COUNTER	(0xfffUL << 0)
#define  AT91_SAM9X60_SET_COUNTER(x)	((x) & AT91_SAM9X60_COUNTER)


#define AT91_SAM9X60_IER	0x14

#define  AT91_SAM9X60_PERINT	BIT(0)

#define AT91_SAM9X60_IDR	0x18

#define AT91_SAM9X60_ISR	0x1c

#endif
