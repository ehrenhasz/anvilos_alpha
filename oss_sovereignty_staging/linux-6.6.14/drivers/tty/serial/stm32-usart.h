 
 

#define DRIVER_NAME "stm32-usart"

struct stm32_usart_offsets {
	u8 cr1;
	u8 cr2;
	u8 cr3;
	u8 brr;
	u8 gtpr;
	u8 rtor;
	u8 rqr;
	u8 isr;
	u8 icr;
	u8 rdr;
	u8 tdr;
};

struct stm32_usart_config {
	u8 uart_enable_bit;  
	bool has_7bits_data;
	bool has_swap;
	bool has_wakeup;
	bool has_fifo;
	int fifosize;
};

struct stm32_usart_info {
	struct stm32_usart_offsets ofs;
	struct stm32_usart_config cfg;
};

#define UNDEF_REG 0xff

 
#define USART_SR_PE		BIT(0)
#define USART_SR_FE		BIT(1)
#define USART_SR_NE		BIT(2)		 
#define USART_SR_ORE		BIT(3)
#define USART_SR_IDLE		BIT(4)
#define USART_SR_RXNE		BIT(5)
#define USART_SR_TC		BIT(6)
#define USART_SR_TXE		BIT(7)
#define USART_SR_CTSIF		BIT(9)
#define USART_SR_CTS		BIT(10)		 
#define USART_SR_RTOF		BIT(11)		 
#define USART_SR_EOBF		BIT(12)		 
#define USART_SR_ABRE		BIT(14)		 
#define USART_SR_ABRF		BIT(15)		 
#define USART_SR_BUSY		BIT(16)		 
#define USART_SR_CMF		BIT(17)		 
#define USART_SR_SBKF		BIT(18)		 
#define USART_SR_WUF		BIT(20)		 
#define USART_SR_TEACK		BIT(21)		 
#define USART_SR_ERR_MASK	(USART_SR_ORE | USART_SR_NE | USART_SR_FE |\
				 USART_SR_PE)
 
#define USART_SR_DUMMY_RX	BIT(16)

 
#define USART_DR_MASK		GENMASK(8, 0)

 
#define USART_BRR_DIV_F_MASK	GENMASK(3, 0)
#define USART_BRR_DIV_M_MASK	GENMASK(15, 4)
#define USART_BRR_DIV_M_SHIFT	4
#define USART_BRR_04_R_SHIFT	1

 
#define USART_CR1_SBK		BIT(0)
#define USART_CR1_RWU		BIT(1)		 
#define USART_CR1_UESM		BIT(1)		 
#define USART_CR1_RE		BIT(2)
#define USART_CR1_TE		BIT(3)
#define USART_CR1_IDLEIE	BIT(4)
#define USART_CR1_RXNEIE	BIT(5)
#define USART_CR1_TCIE		BIT(6)
#define USART_CR1_TXEIE		BIT(7)
#define USART_CR1_PEIE		BIT(8)
#define USART_CR1_PS		BIT(9)
#define USART_CR1_PCE		BIT(10)
#define USART_CR1_WAKE		BIT(11)
#define USART_CR1_M0		BIT(12)		 
#define USART_CR1_MME		BIT(13)		 
#define USART_CR1_CMIE		BIT(14)		 
#define USART_CR1_OVER8		BIT(15)
#define USART_CR1_DEDT_MASK	GENMASK(20, 16)	 
#define USART_CR1_DEAT_MASK	GENMASK(25, 21)	 
#define USART_CR1_RTOIE		BIT(26)		 
#define USART_CR1_EOBIE		BIT(27)		 
#define USART_CR1_M1		BIT(28)		 
#define USART_CR1_IE_MASK	(GENMASK(8, 4) | BIT(14) | BIT(26) | BIT(27))
#define USART_CR1_FIFOEN	BIT(29)		 
#define USART_CR1_DEAT_SHIFT 21
#define USART_CR1_DEDT_SHIFT 16

 
#define USART_CR2_ADD_MASK	GENMASK(3, 0)	 
#define USART_CR2_ADDM7		BIT(4)		 
#define USART_CR2_LBCL		BIT(8)
#define USART_CR2_CPHA		BIT(9)
#define USART_CR2_CPOL		BIT(10)
#define USART_CR2_CLKEN		BIT(11)
#define USART_CR2_STOP_2B	BIT(13)
#define USART_CR2_STOP_MASK	GENMASK(13, 12)
#define USART_CR2_LINEN		BIT(14)
#define USART_CR2_SWAP		BIT(15)		 
#define USART_CR2_RXINV		BIT(16)		 
#define USART_CR2_TXINV		BIT(17)		 
#define USART_CR2_DATAINV	BIT(18)		 
#define USART_CR2_MSBFIRST	BIT(19)		 
#define USART_CR2_ABREN		BIT(20)		 
#define USART_CR2_ABRMOD_MASK	GENMASK(22, 21)	 
#define USART_CR2_RTOEN		BIT(23)		 
#define USART_CR2_ADD_F7_MASK	GENMASK(31, 24)	 

 
#define USART_CR3_EIE		BIT(0)
#define USART_CR3_IREN		BIT(1)
#define USART_CR3_IRLP		BIT(2)
#define USART_CR3_HDSEL		BIT(3)
#define USART_CR3_NACK		BIT(4)
#define USART_CR3_SCEN		BIT(5)
#define USART_CR3_DMAR		BIT(6)
#define USART_CR3_DMAT		BIT(7)
#define USART_CR3_RTSE		BIT(8)
#define USART_CR3_CTSE		BIT(9)
#define USART_CR3_CTSIE		BIT(10)
#define USART_CR3_ONEBIT	BIT(11)
#define USART_CR3_OVRDIS	BIT(12)		 
#define USART_CR3_DDRE		BIT(13)		 
#define USART_CR3_DEM		BIT(14)		 
#define USART_CR3_DEP		BIT(15)		 
#define USART_CR3_SCARCNT_MASK	GENMASK(19, 17)	 
#define USART_CR3_WUS_MASK	GENMASK(21, 20)	 
#define USART_CR3_WUS_START_BIT	BIT(21)		 
#define USART_CR3_WUFIE		BIT(22)		 
#define USART_CR3_TXFTIE	BIT(23)		 
#define USART_CR3_TCBGTIE	BIT(24)		 
#define USART_CR3_RXFTCFG_MASK	GENMASK(27, 25)	 
#define USART_CR3_RXFTCFG_SHIFT	25		 
#define USART_CR3_RXFTIE	BIT(28)		 
#define USART_CR3_TXFTCFG_MASK	GENMASK(31, 29)	 
#define USART_CR3_TXFTCFG_SHIFT	29		 

 
#define USART_GTPR_PSC_MASK	GENMASK(7, 0)
#define USART_GTPR_GT_MASK	GENMASK(15, 8)

 
#define USART_RTOR_RTO_MASK	GENMASK(23, 0)	 
#define USART_RTOR_BLEN_MASK	GENMASK(31, 24)	 

 
#define USART_RQR_ABRRQ		BIT(0)		 
#define USART_RQR_SBKRQ		BIT(1)		 
#define USART_RQR_MMRQ		BIT(2)		 
#define USART_RQR_RXFRQ		BIT(3)		 
#define USART_RQR_TXFRQ		BIT(4)		 

 
#define USART_ICR_PECF		BIT(0)		 
#define USART_ICR_FECF		BIT(1)		 
#define USART_ICR_ORECF		BIT(3)		 
#define USART_ICR_IDLECF	BIT(4)		 
#define USART_ICR_TCCF		BIT(6)		 
#define USART_ICR_CTSCF		BIT(9)		 
#define USART_ICR_RTOCF		BIT(11)		 
#define USART_ICR_EOBCF		BIT(12)		 
#define USART_ICR_CMCF		BIT(17)		 
#define USART_ICR_WUCF		BIT(20)		 

#define STM32_SERIAL_NAME "ttySTM"
#define STM32_MAX_PORTS 8

#define RX_BUF_L 4096		  
#define RX_BUF_P (RX_BUF_L / 2)	  
#define TX_BUF_L RX_BUF_L	  

#define STM32_USART_TIMEOUT_USEC USEC_PER_SEC  

struct stm32_port {
	struct uart_port port;
	struct clk *clk;
	const struct stm32_usart_info *info;
	struct dma_chan *rx_ch;   
	dma_addr_t rx_dma_buf;    
	unsigned char *rx_buf;    
	struct dma_chan *tx_ch;   
	dma_addr_t tx_dma_buf;    
	unsigned char *tx_buf;    
	u32 cr1_irq;		  
	u32 cr3_irq;		  
	int last_res;
	bool tx_dma_busy;	  
	bool rx_dma_busy;	  
	bool throttled;		  
	bool hw_flow_control;
	bool swap;		  
	bool fifoen;
	int rxftcfg;		 
	int txftcfg;		 
	bool wakeup_src;
	int rdr_mask;		 
	struct mctrl_gpios *gpios;  
	struct dma_tx_state rx_dma_state;
};

static struct stm32_port stm32_ports[STM32_MAX_PORTS];
static struct uart_driver stm32_usart_driver;
