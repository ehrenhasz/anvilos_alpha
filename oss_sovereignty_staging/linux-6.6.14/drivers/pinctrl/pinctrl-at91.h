 
 

#ifndef __PINCTRL_AT91_H
#define __PINCTRL_AT91_H

#define PIO_PER		0x00	 
#define PIO_PDR		0x04	 
#define PIO_PSR		0x08	 
#define PIO_OER		0x10	 
#define PIO_ODR		0x14	 
#define PIO_OSR		0x18	 
#define PIO_IFER	0x20	 
#define PIO_IFDR	0x24	 
#define PIO_IFSR	0x28	 
#define PIO_SODR	0x30	 
#define PIO_CODR	0x34	 
#define PIO_ODSR	0x38	 
#define PIO_PDSR	0x3c	 
#define PIO_IER		0x40	 
#define PIO_IDR		0x44	 
#define PIO_IMR		0x48	 
#define PIO_ISR		0x4c	 
#define PIO_MDER	0x50	 
#define PIO_MDDR	0x54	 
#define PIO_MDSR	0x58	 
#define PIO_PUDR	0x60	 
#define PIO_PUER	0x64	 
#define PIO_PUSR	0x68	 
#define PIO_ASR		0x70	 
#define PIO_ABCDSR1	0x70	 
#define PIO_BSR		0x74	 
#define PIO_ABCDSR2	0x74	 
#define PIO_ABSR	0x78	 
#define PIO_IFSCDR	0x80	 
#define PIO_IFSCER	0x84	 
#define PIO_IFSCSR	0x88	 
#define PIO_SCDR	0x8c	 
#define		PIO_SCDR_DIV	(0x3fff <<  0)		 
#define PIO_PPDDR	0x90	 
#define PIO_PPDER	0x94	 
#define PIO_PPDSR	0x98	 
#define PIO_OWER	0xa0	 
#define PIO_OWDR	0xa4	 
#define PIO_OWSR	0xa8	 
#define PIO_AIMER	0xb0	 
#define PIO_AIMDR	0xb4	 
#define PIO_AIMMR	0xb8	 
#define PIO_ESR		0xc0	 
#define PIO_LSR		0xc4	 
#define PIO_ELSR	0xc8	 
#define PIO_FELLSR	0xd0	 
#define PIO_REHLSR	0xd4	 
#define PIO_FRLHSR	0xd8	 
#define PIO_SCHMITT	0x100	 

#define SAMA5D3_PIO_DRIVER1		0x118   
#define SAMA5D3_PIO_DRIVER2		0x11C   

#define AT91SAM9X5_PIO_DRIVER1	0x114   
#define AT91SAM9X5_PIO_DRIVER2	0x118   

#define SAM9X60_PIO_SLEWR	0x110	 
#define SAM9X60_PIO_DRIVER1	0x118	 

#endif
