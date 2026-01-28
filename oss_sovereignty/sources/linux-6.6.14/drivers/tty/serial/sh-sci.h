
#include <linux/bitops.h>
#include <linux/serial_core.h>
#include <linux/io.h>

#define SCI_MAJOR		204
#define SCI_MINOR_START		8



enum {
	SCSMR,				
	SCBRR,				
	SCSCR,				
	SCxSR,				
	SCFCR,				
	SCFDR,				
	SCxTDR,				
	SCxRDR,				
	SCLSR,				
	SCTFDR,				
	SCRFDR,				
	SCSPTR,				
	HSSRR,				
	SCPCR,				
	SCPDR,				
	SCDL,				
	SCCKS,				
	HSRTRGR,			
	HSTTRGR,			
	SEMR,				

	SCIx_NR_REGS,
};



#define SCSMR_C_A	BIT(7)	
#define SCSMR_CSYNC	BIT(7)	
#define SCSMR_ASYNC	0	
#define SCSMR_CHR	BIT(6)	
#define SCSMR_PE	BIT(5)	
#define SCSMR_ODD	BIT(4)	
#define SCSMR_STOP	BIT(3)	
#define SCSMR_CKS	0x0003	


#define SCSMR_CKEDG	BIT(12)	
#define SCSMR_SRC_MASK	0x0700	
#define SCSMR_SRC_16	0x0000	
#define SCSMR_SRC_5	0x0100	
#define SCSMR_SRC_7	0x0200	
#define SCSMR_SRC_11	0x0300	
#define SCSMR_SRC_13	0x0400	
#define SCSMR_SRC_17	0x0500	
#define SCSMR_SRC_19	0x0600	
#define SCSMR_SRC_27	0x0700	


#define SCSCR_TEIE	BIT(2)  


#define SCSCR_TDRQE	BIT(15)	
#define SCSCR_RDRQE	BIT(14)	


#define HSSCR_TOT_SHIFT	14


#define SCI_TDRE	BIT(7)	
#define SCI_RDRF	BIT(6)	
#define SCI_ORER	BIT(5)	
#define SCI_FER		BIT(4)	
#define SCI_PER		BIT(3)	
#define SCI_TEND	BIT(2)	
#define SCI_RESERVED	0x03	

#define SCI_DEFAULT_ERROR_MASK (SCI_PER | SCI_FER)

#define SCI_RDxF_CLEAR	(u32)(~(SCI_RESERVED | SCI_RDRF))
#define SCI_ERROR_CLEAR	(u32)(~(SCI_RESERVED | SCI_PER | SCI_FER | SCI_ORER))
#define SCI_TDxE_CLEAR	(u32)(~(SCI_RESERVED | SCI_TEND | SCI_TDRE))
#define SCI_BREAK_CLEAR	(u32)(~(SCI_RESERVED | SCI_PER | SCI_FER | SCI_ORER))


#define SCIF_ER		BIT(7)	
#define SCIF_TEND	BIT(6)	
#define SCIF_TDFE	BIT(5)	
#define SCIF_BRK	BIT(4)	
#define SCIF_FER	BIT(3)	
#define SCIF_PER	BIT(2)	
#define SCIF_RDF	BIT(1)	
#define SCIF_DR		BIT(0)	

#define SCIF_PERC	0xf000	
#define SCIF_FERC	0x0f00	

#define SCIFA_ORER	BIT(9)	

#define SCIF_DEFAULT_ERROR_MASK (SCIF_PER | SCIF_FER | SCIF_BRK | SCIF_ER)

#define SCIF_RDxF_CLEAR		(u32)(~(SCIF_DR | SCIF_RDF))
#define SCIF_ERROR_CLEAR	(u32)(~(SCIF_PER | SCIF_FER | SCIF_ER))
#define SCIF_TDxE_CLEAR		(u32)(~(SCIF_TDFE))
#define SCIF_BREAK_CLEAR	(u32)(~(SCIF_PER | SCIF_FER | SCIF_BRK))


#define SCFCR_RTRG1	BIT(7)	
#define SCFCR_RTRG0	BIT(6)
#define SCFCR_TTRG1	BIT(5)	
#define SCFCR_TTRG0	BIT(4)
#define SCFCR_MCE	BIT(3)	
#define SCFCR_TFRST	BIT(2)	
#define SCFCR_RFRST	BIT(1)	
#define SCFCR_LOOP	BIT(0)	


#define SCLSR_TO	BIT(2)	
#define SCLSR_ORER	BIT(0)	


#define SCSPTR_RTSIO	BIT(7)	
#define SCSPTR_RTSDT	BIT(6)	
#define SCSPTR_CTSIO	BIT(5)	
#define SCSPTR_CTSDT	BIT(4)	
#define SCSPTR_SCKIO	BIT(3)	
#define SCSPTR_SCKDT	BIT(2)	
#define SCSPTR_SPB2IO	BIT(1)	
#define SCSPTR_SPB2DT	BIT(0)	


#define HSCIF_SRE	BIT(15)	
#define HSCIF_SRDE	BIT(14) 

#define HSCIF_SRHP_SHIFT	8
#define HSCIF_SRHP_MASK		0x0f00


#define SCPCR_RTSC	BIT(4)	
#define SCPCR_CTSC	BIT(3)	
#define SCPCR_SCKC	BIT(2)	
#define SCPCR_RXDC	BIT(1)	
#define SCPCR_TXDC	BIT(0)	


#define SCPDR_RTSD	BIT(4)	
#define SCPDR_CTSD	BIT(3)	
#define SCPDR_SCKD	BIT(2)	
#define SCPDR_RXDD	BIT(1)	
#define SCPDR_TXDD	BIT(0)	


#define SCCKS_CKS	BIT(15)	
#define SCCKS_XIN	BIT(14)	

#define SCxSR_TEND(port)	(((port)->type == PORT_SCI) ? SCI_TEND   : SCIF_TEND)
#define SCxSR_RDxF(port)	(((port)->type == PORT_SCI) ? SCI_RDRF   : SCIF_DR | SCIF_RDF)
#define SCxSR_TDxE(port)	(((port)->type == PORT_SCI) ? SCI_TDRE   : SCIF_TDFE)
#define SCxSR_FER(port)		(((port)->type == PORT_SCI) ? SCI_FER    : SCIF_FER)
#define SCxSR_PER(port)		(((port)->type == PORT_SCI) ? SCI_PER    : SCIF_PER)
#define SCxSR_BRK(port)		(((port)->type == PORT_SCI) ? 0x00       : SCIF_BRK)

#define SCxSR_ERRORS(port)	(to_sci_port(port)->params->error_mask)

#define SCxSR_RDxF_CLEAR(port) \
	(((port)->type == PORT_SCI) ? SCI_RDxF_CLEAR : SCIF_RDxF_CLEAR)
#define SCxSR_ERROR_CLEAR(port) \
	(to_sci_port(port)->params->error_clear)
#define SCxSR_TDxE_CLEAR(port) \
	(((port)->type == PORT_SCI) ? SCI_TDxE_CLEAR : SCIF_TDxE_CLEAR)
#define SCxSR_BREAK_CLEAR(port) \
	(((port)->type == PORT_SCI) ? SCI_BREAK_CLEAR : SCIF_BREAK_CLEAR)
