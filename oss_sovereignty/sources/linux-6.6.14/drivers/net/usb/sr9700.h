


#ifndef _SR9700_H
#define	_SR9700_H




#define	SR_NCR			0x00
#define		NCR_RST			(1 << 0)
#define		NCR_LBK			(3 << 1)
#define		NCR_FDX			(1 << 3)
#define		NCR_WAKEEN		(1 << 6)

#define	SR_NSR			0x01
#define		NSR_RXRDY		(1 << 0)
#define		NSR_RXOV		(1 << 1)
#define		NSR_TX1END		(1 << 2)
#define		NSR_TX2END		(1 << 3)
#define		NSR_TXFULL		(1 << 4)
#define		NSR_WAKEST		(1 << 5)
#define		NSR_LINKST		(1 << 6)
#define		NSR_SPEED		(1 << 7)

#define	SR_TCR			0x02
#define		TCR_CRC_DIS		(1 << 1)
#define		TCR_PAD_DIS		(1 << 2)
#define		TCR_LC_CARE		(1 << 3)
#define		TCR_CRS_CARE	(1 << 4)
#define		TCR_EXCECM		(1 << 5)
#define		TCR_LF_EN		(1 << 6)

#define	SR_TSR1		0x03
#define		TSR1_EC			(1 << 2)
#define		TSR1_COL		(1 << 3)
#define		TSR1_LC			(1 << 4)
#define		TSR1_NC			(1 << 5)
#define		TSR1_LOC		(1 << 6)
#define		TSR1_TLF		(1 << 7)

#define	SR_TSR2		0x04
#define		TSR2_EC			(1 << 2)
#define		TSR2_COL		(1 << 3)
#define		TSR2_LC			(1 << 4)
#define		TSR2_NC			(1 << 5)
#define		TSR2_LOC		(1 << 6)
#define		TSR2_TLF		(1 << 7)

#define	SR_RCR			0x05
#define		RCR_RXEN		(1 << 0)
#define		RCR_PRMSC		(1 << 1)
#define		RCR_RUNT		(1 << 2)
#define		RCR_ALL			(1 << 3)
#define		RCR_DIS_CRC		(1 << 4)
#define		RCR_DIS_LONG	(1 << 5)

#define	SR_RSR			0x06
#define		RSR_AE			(1 << 2)
#define		RSR_MF			(1 << 6)
#define		RSR_RF			(1 << 7)

#define	SR_ROCR		0x07
#define		ROCR_ROC		(0x7F << 0)
#define		ROCR_RXFU		(1 << 7)

#define	SR_BPTR		0x08
#define		BPTR_JPT		(0x0F << 0)
#define		BPTR_BPHW		(0x0F << 4)

#define	SR_FCTR		0x09
#define		FCTR_LWOT		(0x0F << 0)
#define		FCTR_HWOT		(0x0F << 4)

#define	SR_FCR			0x0A
#define		FCR_FLCE		(1 << 0)
#define		FCR_BKPA		(1 << 4)
#define		FCR_TXPEN		(1 << 5)
#define		FCR_TXPF		(1 << 6)
#define		FCR_TXP0		(1 << 7)

#define	SR_EPCR		0x0B
#define		EPCR_ERRE		(1 << 0)
#define		EPCR_ERPRW		(1 << 1)
#define		EPCR_ERPRR		(1 << 2)
#define		EPCR_EPOS		(1 << 3)
#define		EPCR_WEP		(1 << 4)

#define	SR_EPAR		0x0C
#define		EPAR_EROA		(0x3F << 0)
#define		EPAR_PHY_ADR_MASK	(0x03 << 6)
#define		EPAR_PHY_ADR		(0x01 << 6)

#define	SR_EPDR		0x0D	

#define	SR_WCR			0x0F
#define		WCR_MAGICST		(1 << 0)
#define		WCR_LINKST		(1 << 2)
#define		WCR_MAGICEN		(1 << 3)
#define		WCR_LINKEN		(1 << 5)

#define	SR_PAR			0x10	

#define	SR_MAR			0x16	


#define	SR_PRR			0x1F
#define		PRR_PHY_RST		(1 << 0)

#define	SR_TWPAL		0x20

#define	SR_TWPAH		0x21

#define	SR_TRPAL		0x22

#define	SR_TRPAH		0x23

#define	SR_RWPAL		0x24

#define	SR_RWPAH		0x25

#define	SR_RRPAL		0x26

#define	SR_RRPAH		0x27

#define	SR_VID			0x28	

#define	SR_PID			0x2A	

#define	SR_CHIPR		0x2C


#define	SR_USBDA		0xF0
#define		USBDA_USBFA		(0x7F << 0)

#define	SR_RXC			0xF1

#define	SR_TXC_USBS		0xF2
#define		TXC_USBS_TXC0		(1 << 0)
#define		TXC_USBS_TXC1		(1 << 1)
#define		TXC_USBS_TXC2		(1 << 2)
#define		TXC_USBS_EP1RDY		(1 << 5)
#define		TXC_USBS_SUSFLAG	(1 << 6)
#define		TXC_USBS_RXFAULT	(1 << 7)

#define	SR_USBC			0xF4
#define		USBC_EP3NAK		(1 << 4)
#define		USBC_EP3ACK		(1 << 5)


#define	SR_RD_REGS		0x00
#define	SR_WR_REGS		0x01
#define	SR_WR_REG		0x03
#define	SR_REQ_RD_REG	(USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE)
#define	SR_REQ_WR_REG	(USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE)


#define	SR_SHARE_TIMEOUT	1000
#define	SR_EEPROM_LEN		256
#define	SR_MCAST_SIZE		8
#define	SR_MCAST_ADDR_FLAG	0x80
#define	SR_MCAST_MAX		64
#define	SR_TX_OVERHEAD		2	
#define	SR_RX_OVERHEAD		7	

#endif	
