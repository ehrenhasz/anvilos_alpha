 
#ifndef __ASM_ARCH_REGS_AC97_H
#define __ASM_ARCH_REGS_AC97_H

 

#define POCR		(0x0000)  	 
#define POCR_FEIE	(1 << 3)	 
#define POCR_FSRIE	(1 << 1)	 

#define PICR		(0x0004) 	 
#define PICR_FEIE	(1 << 3)	 
#define PICR_FSRIE	(1 << 1)	 

#define MCCR		(0x0008)  	 
#define MCCR_FEIE	(1 << 3)	 
#define MCCR_FSRIE	(1 << 1)	 

#define GCR		(0x000C) 	  
#ifdef CONFIG_PXA3xx
#define GCR_CLKBPB	(1 << 31)	 
#endif
#define GCR_nDMAEN	(1 << 24)	 
#define GCR_CDONE_IE	(1 << 19)	 
#define GCR_SDONE_IE	(1 << 18)	 
#define GCR_SECRDY_IEN	(1 << 9)	 
#define GCR_PRIRDY_IEN	(1 << 8)	 
#define GCR_SECRES_IEN	(1 << 5)	 
#define GCR_PRIRES_IEN	(1 << 4)	 
#define GCR_ACLINK_OFF	(1 << 3)	 
#define GCR_WARM_RST	(1 << 2)	 
#define GCR_COLD_RST	(1 << 1)	 
#define GCR_GIE		(1 << 0)	 

#define POSR		(0x0010)  	 
#define POSR_FIFOE	(1 << 4)	 
#define POSR_FSR	(1 << 2)	 

#define PISR		(0x0014)  	 
#define PISR_FIFOE	(1 << 4)	 
#define PISR_EOC	(1 << 3)	 
#define PISR_FSR	(1 << 2)	 

#define MCSR		(0x0018)  	 
#define MCSR_FIFOE	(1 << 4)	 
#define MCSR_EOC	(1 << 3)	 
#define MCSR_FSR	(1 << 2)	 

#define GSR		(0x001C)  	 
#define GSR_CDONE	(1 << 19)	 
#define GSR_SDONE	(1 << 18)	 
#define GSR_RDCS	(1 << 15)	 
#define GSR_BIT3SLT12	(1 << 14)	 
#define GSR_BIT2SLT12	(1 << 13)	 
#define GSR_BIT1SLT12	(1 << 12)	 
#define GSR_SECRES	(1 << 11)	 
#define GSR_PRIRES	(1 << 10)	 
#define GSR_SCR		(1 << 9)	 
#define GSR_PCR		(1 << 8)	 
#define GSR_MCINT	(1 << 7)	 
#define GSR_POINT	(1 << 6)	 
#define GSR_PIINT	(1 << 5)	 
#define GSR_ACOFFD	(1 << 3)	 
#define GSR_MOINT	(1 << 2)	 
#define GSR_MIINT	(1 << 1)	 
#define GSR_GSCI	(1 << 0)	 

#define CAR		(0x0020)  	 
#define CAR_CAIP	(1 << 0)	 

#define PCDR		(0x0040)  	 
#define MCDR		(0x0060)  	 

#define MOCR		(0x0100)  	 
#define MOCR_FEIE	(1 << 3)	 
#define MOCR_FSRIE	(1 << 1)	 

#define MICR		(0x0108)  	 
#define MICR_FEIE	(1 << 3)	 
#define MICR_FSRIE	(1 << 1)	 

#define MOSR		(0x0110)  	 
#define MOSR_FIFOE	(1 << 4)	 
#define MOSR_FSR	(1 << 2)	 

#define MISR		(0x0118)  	 
#define MISR_FIFOE	(1 << 4)	 
#define MISR_EOC	(1 << 3)	 
#define MISR_FSR	(1 << 2)	 

#define MODR		(0x0140)  	 

#define PAC_REG_BASE	(0x0200)  	 
#define SAC_REG_BASE	(0x0300)  	 
#define PMC_REG_BASE	(0x0400)  	 
#define SMC_REG_BASE	(0x0500)  	 

#endif  
