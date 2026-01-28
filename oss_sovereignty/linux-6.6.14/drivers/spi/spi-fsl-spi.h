#ifndef __SPI_FSL_SPI_H__
#define __SPI_FSL_SPI_H__
struct fsl_spi_reg {
	__be32 cap;  
	u8 res1[0x1C];
	__be32 mode;
	__be32 event;
	__be32 mask;
	__be32 command;
	__be32 transmit;
	__be32 receive;
	__be32 slvsel;  
};
#define	SPMODE_LOOP		(1 << 30)
#define	SPMODE_CI_INACTIVEHIGH	(1 << 29)
#define	SPMODE_CP_BEGIN_EDGECLK	(1 << 28)
#define	SPMODE_DIV16		(1 << 27)
#define	SPMODE_REV		(1 << 26)
#define	SPMODE_MS		(1 << 25)
#define	SPMODE_ENABLE		(1 << 24)
#define	SPMODE_LEN(x)		((x) << 20)
#define	SPMODE_PM(x)		((x) << 16)
#define	SPMODE_OP		(1 << 14)
#define	SPMODE_CG(x)		((x) << 7)
#define SPCAP_SSEN(x)		(((x) >> 16) & 0x1)
#define SPCAP_SSSZ(x)		(((x) >> 24) & 0xff)
#define SPCAP_MAXWLEN(x)	(((x) >> 20) & 0xf)
#define	SPMODE_INIT_VAL (SPMODE_CI_INACTIVEHIGH | SPMODE_DIV16 | SPMODE_REV | \
			 SPMODE_MS | SPMODE_LEN(7) | SPMODE_PM(0xf))
#define	SPIE_NE		0x00000200	 
#define	SPIE_NF		0x00000100	 
#define	SPIM_NE		0x00000200	 
#define	SPIM_NF		0x00000100	 
#endif  
