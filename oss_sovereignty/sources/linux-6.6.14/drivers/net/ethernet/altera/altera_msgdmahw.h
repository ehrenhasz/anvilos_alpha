


#ifndef __ALTERA_MSGDMAHW_H__
#define __ALTERA_MSGDMAHW_H__


struct msgdma_extended_desc {
	u32 read_addr_lo;	
	u32 write_addr_lo;	
	u32 len;		
	u32 burst_seq_num;	
	u32 stride;		
	u32 read_addr_hi;	
	u32 write_addr_hi;	
	u32 control;		
};


#define MSGDMA_DESC_CTL_SET_CH(x)	((x) & 0xff)
#define MSGDMA_DESC_CTL_GEN_SOP		BIT(8)
#define MSGDMA_DESC_CTL_GEN_EOP		BIT(9)
#define MSGDMA_DESC_CTL_PARK_READS	BIT(10)
#define MSGDMA_DESC_CTL_PARK_WRITES	BIT(11)
#define MSGDMA_DESC_CTL_END_ON_EOP	BIT(12)
#define MSGDMA_DESC_CTL_END_ON_LEN	BIT(13)
#define MSGDMA_DESC_CTL_TR_COMP_IRQ	BIT(14)
#define MSGDMA_DESC_CTL_EARLY_IRQ	BIT(15)
#define MSGDMA_DESC_CTL_TR_ERR_IRQ	(0xff << 16)
#define MSGDMA_DESC_CTL_EARLY_DONE	BIT(24)

#define MSGDMA_DESC_CTL_GO		BIT(31)


#define MSGDMA_DESC_CTL_TX_FIRST	(MSGDMA_DESC_CTL_GEN_SOP |	\
					 MSGDMA_DESC_CTL_GO)

#define MSGDMA_DESC_CTL_TX_MIDDLE	(MSGDMA_DESC_CTL_GO)

#define MSGDMA_DESC_CTL_TX_LAST		(MSGDMA_DESC_CTL_GEN_EOP |	\
					 MSGDMA_DESC_CTL_TR_COMP_IRQ |	\
					 MSGDMA_DESC_CTL_GO)

#define MSGDMA_DESC_CTL_TX_SINGLE	(MSGDMA_DESC_CTL_GEN_SOP |	\
					 MSGDMA_DESC_CTL_GEN_EOP |	\
					 MSGDMA_DESC_CTL_TR_COMP_IRQ |	\
					 MSGDMA_DESC_CTL_GO)

#define MSGDMA_DESC_CTL_RX_SINGLE	(MSGDMA_DESC_CTL_END_ON_EOP |	\
					 MSGDMA_DESC_CTL_END_ON_LEN |	\
					 MSGDMA_DESC_CTL_TR_COMP_IRQ |	\
					 MSGDMA_DESC_CTL_EARLY_IRQ |	\
					 MSGDMA_DESC_CTL_TR_ERR_IRQ |	\
					 MSGDMA_DESC_CTL_GO)


#define MSGDMA_DESC_TX_STRIDE		(0x00010001)
#define MSGDMA_DESC_RX_STRIDE		(0x00010001)


struct msgdma_csr {
	u32 status;		
	u32 control;		
	u32 rw_fill_level;	
	u32 resp_fill_level;	
	u32 rw_seq_num;		
	u32 pad[3];		
};


#define MSGDMA_CSR_STAT_BUSY			BIT(0)
#define MSGDMA_CSR_STAT_DESC_BUF_EMPTY		BIT(1)
#define MSGDMA_CSR_STAT_DESC_BUF_FULL		BIT(2)
#define MSGDMA_CSR_STAT_RESP_BUF_EMPTY		BIT(3)
#define MSGDMA_CSR_STAT_RESP_BUF_FULL		BIT(4)
#define MSGDMA_CSR_STAT_STOPPED			BIT(5)
#define MSGDMA_CSR_STAT_RESETTING		BIT(6)
#define MSGDMA_CSR_STAT_STOPPED_ON_ERR		BIT(7)
#define MSGDMA_CSR_STAT_STOPPED_ON_EARLY	BIT(8)
#define MSGDMA_CSR_STAT_IRQ			BIT(9)
#define MSGDMA_CSR_STAT_MASK			0x3FF
#define MSGDMA_CSR_STAT_MASK_WITHOUT_IRQ	0x1FF

#define MSGDMA_CSR_STAT_BUSY_GET(v)			GET_BIT_VALUE(v, 0)
#define MSGDMA_CSR_STAT_DESC_BUF_EMPTY_GET(v)		GET_BIT_VALUE(v, 1)
#define MSGDMA_CSR_STAT_DESC_BUF_FULL_GET(v)		GET_BIT_VALUE(v, 2)
#define MSGDMA_CSR_STAT_RESP_BUF_EMPTY_GET(v)		GET_BIT_VALUE(v, 3)
#define MSGDMA_CSR_STAT_RESP_BUF_FULL_GET(v)		GET_BIT_VALUE(v, 4)
#define MSGDMA_CSR_STAT_STOPPED_GET(v)			GET_BIT_VALUE(v, 5)
#define MSGDMA_CSR_STAT_RESETTING_GET(v)		GET_BIT_VALUE(v, 6)
#define MSGDMA_CSR_STAT_STOPPED_ON_ERR_GET(v)		GET_BIT_VALUE(v, 7)
#define MSGDMA_CSR_STAT_STOPPED_ON_EARLY_GET(v)		GET_BIT_VALUE(v, 8)
#define MSGDMA_CSR_STAT_IRQ_GET(v)			GET_BIT_VALUE(v, 9)


#define MSGDMA_CSR_CTL_STOP			BIT(0)
#define MSGDMA_CSR_CTL_RESET			BIT(1)
#define MSGDMA_CSR_CTL_STOP_ON_ERR		BIT(2)
#define MSGDMA_CSR_CTL_STOP_ON_EARLY		BIT(3)
#define MSGDMA_CSR_CTL_GLOBAL_INTR		BIT(4)
#define MSGDMA_CSR_CTL_STOP_DESCS		BIT(5)


#define MSGDMA_CSR_WR_FILL_LEVEL_GET(v)		(((v) & 0xffff0000) >> 16)
#define MSGDMA_CSR_RD_FILL_LEVEL_GET(v)		((v) & 0x0000ffff)
#define MSGDMA_CSR_RESP_FILL_LEVEL_GET(v)	((v) & 0x0000ffff)


struct msgdma_response {
	u32 bytes_transferred;
	u32 status;
};

#define msgdma_respoffs(a) (offsetof(struct msgdma_response, a))
#define msgdma_csroffs(a) (offsetof(struct msgdma_csr, a))
#define msgdma_descroffs(a) (offsetof(struct msgdma_extended_desc, a))


#define MSGDMA_RESP_EARLY_TERM	BIT(8)
#define MSGDMA_RESP_ERR_MASK	0xFF

#endif 
