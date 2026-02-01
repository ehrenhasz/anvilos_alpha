
 

#include "device.h"
#include "mac.h"
#include "srom.h"

 

 

 

 

 

 

 
unsigned char SROMbyReadEmbedded(void __iomem *iobase,
				 unsigned char contnt_offset)
{
	unsigned short wDelay, wNoACK;
	unsigned char byWait;
	unsigned char byData;
	unsigned char byOrg;

	byData = 0xFF;
	byOrg = ioread8(iobase + MAC_REG_I2MCFG);
	 
	iowrite8(byOrg & (~I2MCFG_NORETRY), iobase + MAC_REG_I2MCFG);
	for (wNoACK = 0; wNoACK < W_MAX_I2CRETRY; wNoACK++) {
		iowrite8(EEP_I2C_DEV_ID, iobase + MAC_REG_I2MTGID);
		iowrite8(contnt_offset, iobase + MAC_REG_I2MTGAD);

		 
		iowrite8(I2MCSR_EEMR, iobase + MAC_REG_I2MCSR);
		 
		for (wDelay = 0; wDelay < W_MAX_TIMEOUT; wDelay++) {
			byWait = ioread8(iobase + MAC_REG_I2MCSR);
			if (byWait & (I2MCSR_DONE | I2MCSR_NACK))
				break;
			udelay(CB_DELAY_LOOP_WAIT);
		}
		if ((wDelay < W_MAX_TIMEOUT) &&
		    (!(byWait & I2MCSR_NACK))) {
			break;
		}
	}
	byData = ioread8(iobase + MAC_REG_I2MDIPT);
	iowrite8(byOrg, iobase + MAC_REG_I2MCFG);
	return byData;
}

 
void SROMvReadAllContents(void __iomem *iobase, unsigned char *pbyEepromRegs)
{
	int     ii;

	 
	for (ii = 0; ii < EEP_MAX_CONTEXT_SIZE; ii++) {
		*pbyEepromRegs = SROMbyReadEmbedded(iobase,
						    (unsigned char)ii);
		pbyEepromRegs++;
	}
}

 
void SROMvReadEtherAddress(void __iomem *iobase,
			   unsigned char *pbyEtherAddress)
{
	unsigned char ii;

	 
	for (ii = 0; ii < ETH_ALEN; ii++) {
		*pbyEtherAddress = SROMbyReadEmbedded(iobase, ii);
		pbyEtherAddress++;
	}
}
