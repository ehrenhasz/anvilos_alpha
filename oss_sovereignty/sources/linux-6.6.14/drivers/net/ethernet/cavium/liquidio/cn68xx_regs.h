


#ifndef __CN68XX_REGS_H__
#define __CN68XX_REGS_H__



#define    CN68XX_SLI_IQ_PORT0_PKIND             0x0800

#define    CN68XX_SLI_IQ_PORT_PKIND(iq)           \
	(CN68XX_SLI_IQ_PORT0_PKIND + ((iq) * CN6XXX_IQ_OFFSET))




#define    CN68XX_SLI_TX_PIPE                    0x1230




#define    CN68XX_INTR_PIPE_ERR                  BIT_ULL(61)

#endif
