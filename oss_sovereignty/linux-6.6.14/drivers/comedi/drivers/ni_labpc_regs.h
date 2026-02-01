 
 

#ifndef _NI_LABPC_REGS_H
#define _NI_LABPC_REGS_H

 
#define STAT1_REG		0x00	 
#define STAT1_DAVAIL		BIT(0)
#define STAT1_OVERRUN		BIT(1)
#define STAT1_OVERFLOW		BIT(2)
#define STAT1_CNTINT		BIT(3)
#define STAT1_GATA0		BIT(5)
#define STAT1_EXTGATA0		BIT(6)
#define CMD1_REG		0x00	 
#define CMD1_MA(x)		(((x) & 0x7) << 0)
#define CMD1_TWOSCMP		BIT(3)
#define CMD1_GAIN(x)		(((x) & 0x7) << 4)
#define CMD1_SCANEN		BIT(7)
#define CMD2_REG		0x01	 
#define CMD2_PRETRIG		BIT(0)
#define CMD2_HWTRIG		BIT(1)
#define CMD2_SWTRIG		BIT(2)
#define CMD2_TBSEL		BIT(3)
#define CMD2_2SDAC0		BIT(4)
#define CMD2_2SDAC1		BIT(5)
#define CMD2_LDAC(x)		BIT(6 + ((x) & 0x1))
#define CMD3_REG		0x02	 
#define CMD3_DMAEN		BIT(0)
#define CMD3_DIOINTEN		BIT(1)
#define CMD3_DMATCINTEN		BIT(2)
#define CMD3_CNTINTEN		BIT(3)
#define CMD3_ERRINTEN		BIT(4)
#define CMD3_FIFOINTEN		BIT(5)
#define ADC_START_CONVERT_REG	0x03	 
#define DAC_LSB_REG(x)		(0x04 + 2 * (x))  
#define DAC_MSB_REG(x)		(0x05 + 2 * (x))  
#define ADC_FIFO_CLEAR_REG	0x08	 
#define ADC_FIFO_REG		0x0a	 
#define DMATC_CLEAR_REG		0x0a	 
#define TIMER_CLEAR_REG		0x0c	 
#define CMD6_REG		0x0e	 
#define CMD6_NRSE		BIT(0)
#define CMD6_ADCUNI		BIT(1)
#define CMD6_DACUNI(x)		BIT(2 + ((x) & 0x1))
#define CMD6_HFINTEN		BIT(5)
#define CMD6_DQINTEN		BIT(6)
#define CMD6_SCANUP		BIT(7)
#define CMD4_REG		0x0f	 
#define CMD4_INTSCAN		BIT(0)
#define CMD4_EOIRCV		BIT(1)
#define CMD4_ECLKDRV		BIT(2)
#define CMD4_SEDIFF		BIT(3)
#define CMD4_ECLKRCV		BIT(4)
#define DIO_BASE_REG		0x10	 
#define COUNTER_A_BASE_REG	0x14	 
#define COUNTER_B_BASE_REG	0x18	 
#define CMD5_REG		0x1c	 
#define CMD5_WRTPRT		BIT(2)
#define CMD5_DITHEREN		BIT(3)
#define CMD5_CALDACLD		BIT(4)
#define CMD5_SCLK		BIT(5)
#define CMD5_SDATA		BIT(6)
#define CMD5_EEPROMCS		BIT(7)
#define STAT2_REG		0x1d	 
#define STAT2_PROMOUT		BIT(0)
#define STAT2_OUTA1		BIT(1)
#define STAT2_FIFONHF		BIT(2)
#define INTERVAL_COUNT_REG	0x1e	 
#define INTERVAL_STROBE_REG	0x1f	 

#endif  
