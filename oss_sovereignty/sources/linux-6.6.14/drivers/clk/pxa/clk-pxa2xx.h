
#ifndef __CLK_PXA2XX_H
#define __CLK_PXA2XX_H

#define CCCR		(0x0000)  
#define CCSR		(0x000C)  
#define CKEN		(0x0004)  
#define OSCC		(0x0008)  

#define CCCR_N_MASK	0x0380	
#define CCCR_M_MASK	0x0060	
#define CCCR_L_MASK	0x001f	

#define CCCR_CPDIS_BIT	(31)
#define CCCR_PPDIS_BIT	(30)
#define CCCR_LCD_26_BIT	(27)
#define CCCR_A_BIT	(25)

#define CCSR_N2_MASK	CCCR_N_MASK
#define CCSR_M_MASK	CCCR_M_MASK
#define CCSR_L_MASK	CCCR_L_MASK
#define CCSR_N2_SHIFT	7

#define CKEN_AC97CONF   (31)    
#define CKEN_CAMERA	(24)	
#define CKEN_SSP1	(23)	
#define CKEN_MEMC	(22)	
#define CKEN_MEMSTK	(21)	
#define CKEN_IM		(20)	
#define CKEN_KEYPAD	(19)	
#define CKEN_USIM	(18)	
#define CKEN_MSL	(17)	
#define CKEN_LCD	(16)	
#define CKEN_PWRI2C	(15)	
#define CKEN_I2C	(14)	
#define CKEN_FICP	(13)	
#define CKEN_MMC	(12)	
#define CKEN_USB	(11)	
#define CKEN_ASSP	(10)	
#define CKEN_USBHOST	(10)	
#define CKEN_OSTIMER	(9)	
#define CKEN_NSSP	(9)	
#define CKEN_I2S	(8)	
#define CKEN_BTUART	(7)	
#define CKEN_FFUART	(6)	
#define CKEN_STUART	(5)	
#define CKEN_HWUART	(4)	
#define CKEN_SSP3	(4)	
#define CKEN_SSP	(3)	
#define CKEN_SSP2	(3)	
#define CKEN_AC97	(2)	
#define CKEN_PWM1	(1)	
#define CKEN_PWM0	(0)	

#define OSCC_OON	(1 << 1)	
#define OSCC_OOK	(1 << 0)	

#endif
