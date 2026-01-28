#ifndef __ALTERA_SGDMAHW_H__
#define __ALTERA_SGDMAHW_H__
struct sgdma_descrip {
	u32	raddr;  
	u32	pad1;
	u32	waddr;
	u32	pad2;
	u32	next;
	u32	pad3;
	u16	bytes;
	u8	rburst;
	u8	wburst;
	u16	bytes_xferred;	 
	u8	status;
	u8	control;
} __packed;
#define SGDMA_DESC_LEN	sizeof(struct sgdma_descrip)
#define SGDMA_STATUS_ERR		BIT(0)
#define SGDMA_STATUS_LENGTH_ERR		BIT(1)
#define SGDMA_STATUS_CRC_ERR		BIT(2)
#define SGDMA_STATUS_TRUNC_ERR		BIT(3)
#define SGDMA_STATUS_PHY_ERR		BIT(4)
#define SGDMA_STATUS_COLL_ERR		BIT(5)
#define SGDMA_STATUS_EOP		BIT(7)
#define SGDMA_CONTROL_EOP		BIT(0)
#define SGDMA_CONTROL_RD_FIXED		BIT(1)
#define SGDMA_CONTROL_WR_FIXED		BIT(2)
#define SGDMA_CONTROL_HW_OWNED		BIT(7)
struct sgdma_csr {
	u32	status;
	u32	pad1[3];
	u32	control;
	u32	pad2[3];
	u32	next_descrip;
	u32	pad3[3];
};
#define sgdma_csroffs(a) (offsetof(struct sgdma_csr, a))
#define sgdma_descroffs(a) (offsetof(struct sgdma_descrip, a))
#define SGDMA_STSREG_ERR	BIT(0)  
#define SGDMA_STSREG_EOP	BIT(1)  
#define SGDMA_STSREG_DESCRIP	BIT(2)  
#define SGDMA_STSREG_CHAIN	BIT(3)  
#define SGDMA_STSREG_BUSY	BIT(4)  
#define SGDMA_CTRLREG_IOE	BIT(0)  
#define SGDMA_CTRLREG_IOEOP	BIT(1)  
#define SGDMA_CTRLREG_IDESCRIP	BIT(2)  
#define SGDMA_CTRLREG_ILASTD	BIT(3)  
#define SGDMA_CTRLREG_INTEN	BIT(4)  
#define SGDMA_CTRLREG_START	BIT(5)  
#define SGDMA_CTRLREG_STOPERR	BIT(6)  
#define SGDMA_CTRLREG_INTMAX	BIT(7)  
#define SGDMA_CTRLREG_RESET	BIT(16) 
#define SGDMA_CTRLREG_COBHW	BIT(17) 
#define SGDMA_CTRLREG_POLL	BIT(18) 
#define SGDMA_CTRLREG_CLRINT	BIT(31) 
#endif  
