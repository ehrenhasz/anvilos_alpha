 
 

#ifndef __COMEDI_PLX9080_H
#define __COMEDI_PLX9080_H

#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/io.h>

 
struct plx_dma_desc {
	__le32 pci_start_addr;
	__le32 local_start_addr;
	__le32 transfer_size;
	__le32 next;
};

 

 
#define PLX_REG_LAS0RR		0x0000
 
#define PLX_REG_LAS1RR		0x00f0

#define PLX_LASRR_IO		BIT(0)		 
#define PLX_LASRR_MLOC_ANY32	(BIT(1) * 0)	 
#define PLX_LASRR_MLOC_LT1MB	(BIT(1) * 1)	 
#define PLX_LASRR_MLOC_ANY64	(BIT(1) * 2)	 
#define PLX_LASRR_MLOC_MASK	GENMASK(2, 1)	 
#define PLX_LASRR_PREFETCH	BIT(3)		 
 
#define PLX_LASRR_MEM_MASK	GENMASK(31, 4)
 
#define PLX_LASRR_IO_MASK	GENMASK(31, 2)

 
#define PLX_REG_LAS0BA		0x0004
 
#define PLX_REG_LAS1BA		0x00f4

#define PLX_LASBA_EN		BIT(0)		 
 
#define PLX_LASBA_MEM_MASK	GENMASK(31, 4)
 
#define PLX_LASBA_IO_MASK	GENMASK(31, 2)

 
#define PLX_REG_MARBR		0x0008
 
#define PLX_REG_DMAARB		0x00ac

 
#define PLX_MARBR_LT(x)		(BIT(0) * ((x) & 0xff))
#define PLX_MARBR_LT_MASK	GENMASK(7, 0)
#define PLX_MARBR_TO_LT(r)	((r) & PLX_MARBR_LT_MASK)
 
#define PLX_MARBR_PT(x)		(BIT(8) * ((x) & 0xff))
#define PLX_MARBR_PT_MASK	GENMASK(15, 8)
#define PLX_MARBR_TO_PT(r)	(((r) & PLX_MARBR_PT_MASK) >> 8)
 
#define PLX_MARBR_LTEN		BIT(16)
 
#define PLX_MARBR_PTEN		BIT(17)
 
#define PLX_MARBR_BREQEN	BIT(18)
 
#define PLX_MARBR_PRIO_ROT	(BIT(19) * 0)	 
#define PLX_MARBR_PRIO_DMA0	(BIT(19) * 1)	 
#define PLX_MARBR_PRIO_DMA1	(BIT(19) * 2)	 
#define PLX_MARBR_PRIO_MASK	GENMASK(20, 19)
 
#define PLX_MARBR_DSGUBM	BIT(21)
 
#define PLX_MARBR_DSLLOCKOEN	BIT(22)
 
#define PLX_MARBR_PCIREQM	BIT(23)
 
#define PLX_MARBR_PCIV21M	BIT(24)
 
#define PLX_MARBR_PCIRNWM	BIT(25)
 
#define PLX_MARBR_PCIRWFM	BIT(26)
 
#define PLX_MARBR_GLTBREQ	BIT(27)
 
#define PLX_MARBR_PCIRNFM	BIT(28)
 
#define PLX_MARBR_SUBSYSIDS	BIT(29)

 
#define PLX_REG_BIGEND		0x000c

 
#define PLX_BIGEND_CONFIG	BIT(0)
 
#define PLX_BIGEND_DM		BIT(1)
 
#define PLX_BIGEND_DSAS0	BIT(2)
 
#define PLX_BIGEND_EROM		BIT(3)
 
#define PLX_BIGEND_BEBLM	BIT(4)
 
#define PLX_BIGEND_DSAS1	BIT(5)
 
#define PLX_BIGEND_DMA1		BIT(6)
 
#define PLX_BIGEND_DMA0		BIT(7)
 
#define PLX_BIGEND_DMA(n)	((n) ? PLX_BIGEND_DMA1 : PLX_BIGEND_DMA0)

 

 
#define PLX_REG_EROMRR		0x0010
 
#define PLX_REG_EROMBA		0x0014

 
#define PLX_REG_LBRD0		0x0018
 
#define PLX_REG_LBRD1		0x00f8

 
#define PLX_LBRD_MSWIDTH_8	(BIT(0) * 0)	 
#define PLX_LBRD_MSWIDTH_16	(BIT(0) * 1)	 
#define PLX_LBRD_MSWIDTH_32	(BIT(0) * 2)	 
#define PLX_LBRD_MSWIDTH_32A	(BIT(0) * 3)	 
#define PLX_LBRD_MSWIDTH_MASK	GENMASK(1, 0)
 
#define PLX_LBRD_MSIWS(x)	(BIT(2) * ((x) & 0xf))
#define PLX_LBRD_MSIWS_MASK	GENMASK(5, 2)
#define PLX_LBRD_TO_MSIWS(r)	(((r) & PLS_LBRD_MSIWS_MASK) >> 2)
 
#define PLX_LBRD_MSREADYIEN	BIT(6)
 
#define PLX_LBRD_MSBTERMIEN	BIT(7)
 
#define PLX_LBRD0_MSPREDIS	BIT(8)
 
#define PLX_LBRD1_MSBURSTEN	BIT(8)
 
#define PLX_LBRD0_EROMPREDIS	BIT(9)
 
#define PLX_LBRD1_MSPREDIS	BIT(9)
 
#define PLX_LBRD_RPFCOUNTEN	BIT(10)
 
#define PLX_LBRD_PFCOUNT(x)	(BIT(11) * ((x) & 0xf))
#define PLX_LBRD_PFCOUNT_MASK	GENMASK(14, 11)
#define PLX_LBRD_TO_PFCOUNT(r)	(((r) & PLX_LBRD_PFCOUNT_MASK) >> 11)
 
#define PLX_LBRD0_EROMWIDTH_8	(BIT(16) * 0)	 
#define PLX_LBRD0_EROMWIDTH_16	(BIT(16) * 1)	 
#define PLX_LBRD0_EROMWIDTH_32	(BIT(16) * 2)	 
#define PLX_LBRD0_EROMWIDTH_32A	(BIT(16) * 3)	 
#define PLX_LBRD0_EROMWIDTH_MASK	GENMASK(17, 16)
 
#define PLX_LBRD0_EROMIWS(x)	(BIT(18) * ((x) & 0xf))
#define PLX_LBRD0_EROMIWS_MASK	GENMASK(21, 18)
#define PLX_LBRD0_TO_EROMIWS(r)	(((r) & PLX_LBRD0_EROMIWS_MASK) >> 18)
 
#define PLX_LBRD0_EROMREADYIEN	BIT(22)
 
#define PLX_LBRD0_EROMBTERMIEN	BIT(23)
 
#define PLX_LBRD0_MSBURSTEN	BIT(24)
 
#define PLX_LBRD0_EELONGLOAD	BIT(25)
 
#define PLX_LBRD0_EROMBURSTEN	BIT(26)
 
#define PLX_LBRD0_DSWMTRDY	BIT(27)
 
#define PLX_LBRD0_TRDELAY(x)	(BIT(28) * ((x) & 0xF))
#define PLX_LBRD0_TRDELAY_MASK	GENMASK(31, 28)
#define PLX_LBRD0_TO_TRDELAY(r)	(((r) & PLX_LBRD0_TRDELAY_MASK) >> 28)

 
#define PLX_REG_DMRR		0x001c

 
#define PLX_REG_DMLBAM		0x0020

 
#define PLX_REG_DMLBAI		0x0024

 
#define PLX_REG_DMPBAM		0x0028

 
#define PLX_DMPBAM_MEMACCEN	BIT(0)
 
#define PLX_DMPBAM_IOACCEN	BIT(1)
 
#define PLX_DMPBAM_LLOCKIEN	BIT(2)
 
#define PLX_DMPBAM_RPSIZE_CONT	((BIT(12) * 0) | (BIT(3) * 0))
#define PLX_DMPBAM_RPSIZE_4	((BIT(12) * 0) | (BIT(3) * 1))
#define PLX_DMPBAM_RPSIZE_8	((BIT(12) * 1) | (BIT(3) * 0))
#define PLX_DMPBAM_RPSIZE_16	((BIT(12) * 1) | (BIT(3) * 1))
#define PLX_DMPBAM_RPSIZE_MASK	(BIT(12) | BIT(3))
 
#define PLX_DMPBAM_RMIRDY	BIT(4)
 
#define PLX_DMPBAM_PAFL(x)	((BIT(10) * !!((x) & 0x10)) | \
				 (BIT(5) * ((x) & 0xf)))
#define PLX_DMPBAM_TO_PAFL(v)	((((BIT(10) & (v)) >> 1) | \
				  (GENMASK(8, 5) & (v))) >> 5)
#define PLX_DMPBAM_PAFL_MASK	(BIT(10) | GENMASK(8, 5))
 
#define PLX_DMPBAM_WIM		BIT(9)
 
#define PLX_DBPBAM_PFLIMIT	BIT(11)
 
#define PLX_DMPBAM_IOREMAPSEL	BIT(13)
 
#define PLX_DMPBAM_WDELAY_NONE	(BIT(14) * 0)
#define PLX_DMPBAM_WDELAY_4	(BIT(14) * 1)
#define PLX_DMPBAM_WDELAY_8	(BIT(14) * 2)
#define PLX_DMPBAM_WDELAY_16	(BIT(14) * 3)
#define PLX_DMPBAM_WDELAY_MASK	GENMASK(15, 14)
 
#define PLX_DMPBAM_REMAP_MASK	GENMASK(31, 16)

 
#define PLX_REG_DMCFGA		0x002c

 
#define PLX_DMCFGA_TYPE0	(BIT(0) * 0)
#define PLX_DMCFGA_TYPE1	(BIT(0) * 1)
#define PLX_DMCFGA_TYPE_MASK	GENMASK(1, 0)
 
#define PLX_DMCFGA_REGNUM(x)	(BIT(2) * ((x) & 0x3f))
#define PLX_DMCFGA_REGNUM_MASK	GENMASK(7, 2)
#define PLX_DMCFGA_TO_REGNUM(r)	(((r) & PLX_DMCFGA_REGNUM_MASK) >> 2)
 
#define PLX_DMCFGA_FUNCNUM(x)	(BIT(8) * ((x) & 0x7))
#define PLX_DMCFGA_FUNCNUM_MASK	GENMASK(10, 8)
#define PLX_DMCFGA_TO_FUNCNUM(r) (((r) & PLX_DMCFGA_FUNCNUM_MASK) >> 8)
 
#define PLX_DMCFGA_DEVNUM(x)	(BIT(11) * ((x) & 0x1f))
#define PLX_DMCFGA_DEVNUM_MASK	GENMASK(15, 11)
#define PLX_DMCFGA_TO_DEVNUM(r)	(((r) & PLX_DMCFGA_DEVNUM_MASK) >> 11)
 
#define PLX_DMCFGA_BUSNUM(x)	(BIT(16) * ((x) & 0xff))
#define PLX_DMCFGA_BUSNUM_MASK	GENMASK(23, 16)
#define PLX_DMCFGA_TO_BUSNUM(r)	(((r) & PLX_DMCFGA_BUSNUM_MASK) >> 16)
 
#define PLX_DMCFGA_CONFIGEN	BIT(31)

 
#define PLX_REG_MBOX(n)		(0x0040 + (n) * 4)
#define PLX_REG_MBOX0		PLX_REG_MBOX(0)
#define PLX_REG_MBOX1		PLX_REG_MBOX(1)
#define PLX_REG_MBOX2		PLX_REG_MBOX(2)
#define PLX_REG_MBOX3		PLX_REG_MBOX(3)
#define PLX_REG_MBOX4		PLX_REG_MBOX(4)
#define PLX_REG_MBOX5		PLX_REG_MBOX(5)
#define PLX_REG_MBOX6		PLX_REG_MBOX(6)
#define PLX_REG_MBOX7		PLX_REG_MBOX(7)

 
#define PLX_REG_ALT_MBOX(n)	((n) < 2 ? 0x0078 + (n) * 4 : PLX_REG_MBOX(n))
#define PLX_REG_ALT_MBOX0	PLX_REG_ALT_MBOX(0)
#define PLX_REG_ALT_MBOX1	PLX_REG_ALT_MBOX(1)

 
#define PLX_REG_P2LDBELL	0x0060

 
#define PLX_REG_L2PDBELL	0x0064

 
#define PLX_REG_INTCSR		0x0068

 
#define PLX_INTCSR_LSEABORTEN	BIT(0)
 
#define PLX_INTCSR_LSEPARITYEN	BIT(1)
 
#define PLX_INTCSR_GENSERR	BIT(2)
 
#define PLX_INTCSR_MBIEN	BIT(3)
 
#define PLX_INTCSR_PIEN		BIT(8)
 
#define PLX_INTCSR_PDBIEN	BIT(9)
 
#define PLX_INTCSR_PABORTIEN	BIT(10)
 
#define PLX_INTCSR_PLIEN	BIT(11)
 
#define PLX_INTCSR_RAEN		BIT(12)
 
#define PLX_INTCSR_PDBIA	BIT(13)
 
#define PLX_INTCSR_PABORTIA	BIT(14)
 
#define PLX_INTCSR_PLIA		BIT(15)
 
#define PLX_INTCSR_LIOEN	BIT(16)
 
#define PLX_INTCSR_LDBIEN	BIT(17)
 
#define PLX_INTCSR_DMA0IEN	BIT(18)
 
#define PLX_INTCSR_DMA1IEN	BIT(19)
 
#define PLX_INTCSR_DMAIEN(n)	((n) ? PLX_INTCSR_DMA1IEN : PLX_INTCSR_DMA0IEN)
 
#define PLX_INTCSR_LDBIA	BIT(20)
 
#define PLX_INTCSR_DMA0IA	BIT(21)
 
#define PLX_INTCSR_DMA1IA	BIT(22)
 
#define PLX_INTCSR_DMAIA(n)	((n) ? PLX_INTCSR_DMA1IA : PLX_INTCSR_DMA0IA)
 
#define PLX_INTCSR_BISTIA	BIT(23)
 
#define PLX_INTCSR_ABNOTDM	BIT(24)
 
#define PLX_INTCSR_ABNOTDMA0	BIT(25)
 
#define PLX_INTCSR_ABNOTDMA1	BIT(26)
 
#define PLX_INTCSR_ABNOTDMA(n)	((n) ? PLX_INTCSR_ABNOTDMA1 \
				     : PLX_INTCSR_ABNOTDMA0)
 
#define PLX_INTCSR_ABNOTRETRY	BIT(27)
 
#define PLX_INTCSR_MB0IA	BIT(28)
 
#define PLX_INTCSR_MB1IA	BIT(29)
 
#define PLX_INTCSR_MB2IA	BIT(30)
 
#define PLX_INTCSR_MB3IA	BIT(31)
 
#define PLX_INTCSR_MBIA(n)	BIT(28 + (n))

 
#define PLX_REG_CNTRL		0x006c

 
#define PLX_CNTRL_CCRDMA(x)	(BIT(0) * ((x) & 0xf))
#define PLX_CNTRL_CCRDMA_MASK	GENMASK(3, 0)
#define PLX_CNTRL_TO_CCRDMA(r)	((r) & PLX_CNTRL_CCRDMA_MASK)
#define PLX_CNTRL_CCRDMA_NORMAL	PLX_CNTRL_CCRDMA(14)	 
 
#define PLX_CNTRL_CCWDMA(x)	(BIT(4) * ((x) & 0xf))
#define PLX_CNTRL_CCWDMA_MASK	GENMASK(7, 4)
#define PLX_CNTRL_TO_CCWDMA(r)	(((r) & PLX_CNTRL_CCWDMA_MASK) >> 4)
#define PLX_CNTRL_CCWDMA_NORMAL	PLX_CNTRL_CCWDMA(7)	 
 
#define PLX_CNTRL_CCRDM(x)	(BIT(8) * ((x) & 0xf))
#define PLX_CNTRL_CCRDM_MASK	GENMASK(11, 8)
#define PLX_CNTRL_TO_CCRDM(r)	(((r) & PLX_CNTRL_CCRDM_MASK) >> 8)
#define PLX_CNTRL_CCRDM_NORMAL	PLX_CNTRL_CCRDM(6)	 
 
#define PLX_CNTRL_CCWDM(x)	(BIT(12) * ((x) & 0xf))
#define PLX_CNTRL_CCWDM_MASK	GENMASK(15, 12)
#define PLX_CNTRL_TO_CCWDM(r)	(((r) & PLX_CNTRL_CCWDM_MASK) >> 12)
#define PLX_CNTRL_CCWDM_NORMAL	PLX_CNTRL_CCWDM(7)	 
 
#define PLX_CNTRL_USERO		BIT(16)
 
#define PLX_CNTRL_USERI		BIT(17)
 
#define PLX_CNTRL_EESK		BIT(24)
 
#define PLX_CNTRL_EECS		BIT(25)
 
#define PLX_CNTRL_EEWB		BIT(26)
 
#define PLX_CNTRL_EERB		BIT(27)
 
#define PLX_CNTRL_EEPRESENT	BIT(28)
 
#define PLX_CNTRL_EERELOAD	BIT(29)
 
#define PLX_CNTRL_RESET		BIT(30)
 
#define PLX_CNTRL_INITDONE	BIT(31)
 
#define PLX_CNTRL_CC_MASK	\
	(PLX_CNTRL_CCRDMA_MASK | PLX_CNTRL_CCWDMA_MASK | \
	 PLX_CNTRL_CCRDM_MASK | PLX_CNTRL_CCWDM_MASK)
#define PLX_CNTRL_CC_NORMAL	\
	(PLX_CNTRL_CCRDMA_NORMAL | PLX_CNTRL_CCWDMA_NORMAL | \
	 PLX_CNTRL_CCRDM_NORMAL | PLX_CNTRL_CCWDM_NORMAL)  

 
#define PLX_REG_PCIHIDR		0x0070

 
#define PLX_PCIHIDR_9080	0x908010b5

 
#define PLX_REG_PCIHREV		0x0074

 
#define PLX_REG_DMAMODE(n)	((n) ? PLX_REG_DMAMODE1 : PLX_REG_DMAMODE0)
#define PLX_REG_DMAMODE0	0x0080
#define PLX_REG_DMAMODE1	0x0094

 
#define PLX_DMAMODE_WIDTH_8	(BIT(0) * 0)	 
#define PLX_DMAMODE_WIDTH_16	(BIT(0) * 1)	 
#define PLX_DMAMODE_WIDTH_32	(BIT(0) * 2)	 
#define PLX_DMAMODE_WIDTH_32A	(BIT(0) * 3)	 
#define PLX_DMAMODE_WIDTH_MASK	GENMASK(1, 0)
 
#define PLX_DMAMODE_IWS(x)	(BIT(2) * ((x) & 0xf))
#define PLX_DMAMODE_IWS_MASK	GENMASK(5, 2)
#define PLX_DMAMODE_TO_IWS(r)	(((r) & PLX_DMAMODE_IWS_MASK) >> 2)
 
#define PLX_DMAMODE_READYIEN	BIT(6)
 
#define PLX_DMAMODE_BTERMIEN	BIT(7)
 
#define PLX_DMAMODE_BURSTEN	BIT(8)
 
#define PLX_DMAMODE_CHAINEN	BIT(9)
 
#define PLX_DMAMODE_DONEIEN	BIT(10)
 
#define PLX_DMAMODE_LACONST	BIT(11)
 
#define PLX_DMAMODE_DEMAND	BIT(12)
 
#define PLX_DMAMODE_WINVALIDATE	BIT(13)
 
#define PLX_DMAMODE_EOTEN	BIT(14)
 
#define PLX_DMAMODE_STOP	BIT(15)
 
#define PLX_DMAMODE_CLRCOUNT	BIT(16)
 
#define PLX_DMAMODE_INTRPCI	BIT(17)

 
#define PLX_REG_DMAPADR(n)	((n) ? PLX_REG_DMAPADR1 : PLX_REG_DMAPADR0)
#define PLX_REG_DMAPADR0	0x0084
#define PLX_REG_DMAPADR1	0x0098

 
#define PLX_REG_DMALADR(n)	((n) ? PLX_REG_DMALADR1 : PLX_REG_DMALADR0)
#define PLX_REG_DMALADR0	0x0088
#define PLX_REG_DMALADR1	0x009c

 
#define PLX_REG_DMASIZ(n)	((n) ? PLX_REG_DMASIZ1 : PLX_REG_DMASIZ0)
#define PLX_REG_DMASIZ0		0x008c
#define PLX_REG_DMASIZ1		0x00a0

 
#define PLX_REG_DMADPR(n)	((n) ? PLX_REG_DMADPR1 : PLX_REG_DMADPR0)
#define PLX_REG_DMADPR0		0x0090
#define PLX_REG_DMADPR1		0x00a4

 
#define PLX_DMADPR_DESCPCI	BIT(0)
 
#define PLX_DMADPR_CHAINEND	BIT(1)
 
#define PLX_DMADPR_TCINTR	BIT(2)
 
#define PLX_DMADPR_XFERL2P	BIT(3)
 
#define PLX_DMADPR_NEXT_MASK	GENMASK(31, 4)

 
#define PLX_REG_DMACSR(n)	((n) ? PLX_REG_DMACSR1 : PLX_REG_DMACSR0)
#define PLX_REG_DMACSR0		0x00a8
#define PLX_REG_DMACSR1		0x00a9

 
#define PLX_DMACSR_ENABLE	BIT(0)
 
#define PLX_DMACSR_START	BIT(1)
 
#define PLX_DMACSR_ABORT	BIT(2)
 
#define PLX_DMACSR_CLEARINTR	BIT(3)
 
#define PLX_DMACSR_DONE		BIT(4)

 
#define PLX_REG_DMATHR		0x00b0

 

 
#define PLX_DMATHR_C0PLAF(x)	(BIT(0) * ((x) & 0xf))
#define PLX_DMATHR_C0PLAF_MASK	GENMASK(3, 0)
#define PLX_DMATHR_TO_C0PLAF(r)	((r) & PLX_DMATHR_C0PLAF_MASK)
 
#define PLX_DMATHR_C0LPAE(x)	(BIT(4) * ((x) & 0xf))
#define PLX_DMATHR_C0LPAE_MASK	GENMASK(7, 4)
#define PLX_DMATHR_TO_C0LPAE(r)	(((r) & PLX_DMATHR_C0LPAE_MASK) >> 4)
 
#define PLX_DMATHR_C0LPAF(x)	(BIT(8) * ((x) & 0xf))
#define PLX_DMATHR_C0LPAF_MASK	GENMASK(11, 8)
#define PLX_DMATHR_TO_C0LPAF(r)	(((r) & PLX_DMATHR_C0LPAF_MASK) >> 8)
 
#define PLX_DMATHR_C0PLAE(x)	(BIT(12) * ((x) & 0xf))
#define PLX_DMATHR_C0PLAE_MASK	GENMASK(15, 12)
#define PLX_DMATHR_TO_C0PLAE(r)	(((r) & PLX_DMATHR_C0PLAE_MASK) >> 12)
 
#define PLX_DMATHR_C1PLAF(x)	(BIT(16) * ((x) & 0xf))
#define PLX_DMATHR_C1PLAF_MASK	GENMASK(19, 16)
#define PLX_DMATHR_TO_C1PLAF(r)	(((r) & PLX_DMATHR_C1PLAF_MASK) >> 16)
 
#define PLX_DMATHR_C1LPAE(x)	(BIT(20) * ((x) & 0xf))
#define PLX_DMATHR_C1LPAE_MASK	GENMASK(23, 20)
#define PLX_DMATHR_TO_C1LPAE(r)	(((r) & PLX_DMATHR_C1LPAE_MASK) >> 20)
 
#define PLX_DMATHR_C1LPAF(x)	(BIT(24) * ((x) & 0xf))
#define PLX_DMATHR_C1LPAF_MASK	GENMASK(27, 24)
#define PLX_DMATHR_TO_C1LPAF(r)	(((r) & PLX_DMATHR_C1LPAF_MASK) >> 24)
 
#define PLX_DMATHR_C1PLAE(x)	(BIT(28) * ((x) & 0xf))
#define PLX_DMATHR_C1PLAE_MASK	GENMASK(31, 28)
#define PLX_DMATHR_TO_C1PLAE(r)	(((r) & PLX_DMATHR_C1PLAE_MASK) >> 28)

 

 
#define PLX_REG_QSR		0x00e8

 
#define PLX_QSR_VALUE_AFTER_RESET	0x00000050

 

#define PLX_PREFETCH   32

 
static inline int plx9080_abort_dma(void __iomem *iobase, unsigned int channel)
{
	void __iomem *dma_cs_addr;
	u8 dma_status;
	const int timeout = 10000;
	unsigned int i;

	dma_cs_addr = iobase + PLX_REG_DMACSR(channel);

	 
	dma_status = readb(dma_cs_addr);
	if ((dma_status & PLX_DMACSR_ENABLE) == 0)
		return 0;

	 
	for (i = 0; (dma_status & PLX_DMACSR_DONE) && i < timeout; i++) {
		udelay(1);
		dma_status = readb(dma_cs_addr);
	}
	if (i == timeout)
		return -ETIMEDOUT;

	 
	writeb(PLX_DMACSR_ABORT, dma_cs_addr);
	 
	dma_status = readb(dma_cs_addr);
	for (i = 0; (dma_status & PLX_DMACSR_DONE) == 0 && i < timeout; i++) {
		udelay(1);
		dma_status = readb(dma_cs_addr);
	}
	if (i == timeout)
		return -ETIMEDOUT;

	return 0;
}

#endif  
