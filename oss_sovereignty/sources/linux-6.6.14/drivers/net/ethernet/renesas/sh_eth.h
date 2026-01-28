


#ifndef __SH_ETH_H__
#define __SH_ETH_H__

#define CARDNAME	"sh-eth"
#define TX_TIMEOUT	(5*HZ)
#define TX_RING_SIZE	64	
#define RX_RING_SIZE	64	
#define TX_RING_MIN	64
#define RX_RING_MIN	64
#define TX_RING_MAX	1024
#define RX_RING_MAX	1024
#define PKT_BUF_SZ	1538
#define SH_ETH_TSU_TIMEOUT_MS	500
#define SH_ETH_TSU_CAM_ENTRIES	32

enum {
	

	
	EDSR = 0,
	EDMR,
	EDTRR,
	EDRRR,
	EESR,
	EESIPR,
	TDLAR,
	TDFAR,
	TDFXR,
	TDFFR,
	RDLAR,
	RDFAR,
	RDFXR,
	RDFFR,
	TRSCER,
	RMFCR,
	TFTR,
	FDR,
	RMCR,
	EDOCR,
	TFUCR,
	RFOCR,
	RMIIMODE,
	FCFTR,
	RPADIR,
	TRIMD,
	RBWAR,
	TBRAR,

	
	ECMR,
	ECSR,
	ECSIPR,
	PIR,
	PSR,
	RDMLR,
	PIPR,
	RFLR,
	IPGR,
	APR,
	MPR,
	PFTCR,
	PFRCR,
	RFCR,
	RFCF,
	TPAUSER,
	TPAUSECR,
	BCFR,
	BCFRR,
	GECMR,
	BCULR,
	MAHR,
	MALR,
	TROCR,
	CDCR,
	LCCR,
	CNDCR,
	CEFCR,
	FRECR,
	TSFRCR,
	TLFRCR,
	CERCR,
	CEECR,
	MAFCR,
	RTRATE,
	CSMR,
	RMII_MII,

	
	ARSTR,
	TSU_CTRST,
	TSU_FWEN0,
	TSU_FWEN1,
	TSU_FCM,
	TSU_BSYSL0,
	TSU_BSYSL1,
	TSU_PRISL0,
	TSU_PRISL1,
	TSU_FWSL0,
	TSU_FWSL1,
	TSU_FWSLC,
	TSU_QTAG0,			
	TSU_QTAG1,			
	TSU_QTAGM0,
	TSU_QTAGM1,
	TSU_FWSR,
	TSU_FWINMK,
	TSU_ADQT0,
	TSU_ADQT1,
	TSU_VTAG0,
	TSU_VTAG1,
	TSU_ADSBSY,
	TSU_TEN,
	TSU_POST1,
	TSU_POST2,
	TSU_POST3,
	TSU_POST4,
	TSU_ADRH0,
	

	TXNLCR0,
	TXALCR0,
	RXNLCR0,
	RXALCR0,
	FWNLCR0,
	FWALCR0,
	TXNLCR1,
	TXALCR1,
	RXNLCR1,
	RXALCR1,
	FWNLCR1,
	FWALCR1,

	
	SH_ETH_MAX_REGISTER_OFFSET,
};

enum {
	SH_ETH_REG_GIGABIT,
	SH_ETH_REG_FAST_RCAR,
	SH_ETH_REG_FAST_SH4,
	SH_ETH_REG_FAST_SH3_SH2
};


#if defined(CONFIG_CPU_SH4) || defined(CONFIG_ARCH_RENESAS)
#define SH_ETH_RX_ALIGN		32
#else
#define SH_ETH_RX_ALIGN		2
#endif



enum EDSR_BIT {
	EDSR_ENT = 0x01, EDSR_ENR = 0x02,
};
#define EDSR_ENALL (EDSR_ENT|EDSR_ENR)


enum GECMR_BIT {
	GECMR_10 = 0x0, GECMR_100 = 0x04, GECMR_1000 = 0x01,
};


enum EDMR_BIT {
	EDMR_NBST = 0x80,
	EDMR_EL = 0x40, 
	EDMR_DL1 = 0x20, EDMR_DL0 = 0x10,
	EDMR_SRST_GETHER = 0x03,
	EDMR_SRST_ETHER = 0x01,
};


enum EDTRR_BIT {
	EDTRR_TRNS_GETHER = 0x03,
	EDTRR_TRNS_ETHER = 0x01,
};


enum EDRRR_BIT {
	EDRRR_R = 0x01,
};


enum TPAUSER_BIT {
	TPAUSER_TPAUSE = 0x0000ffff,
	TPAUSER_UNLIMITED = 0,
};


enum BCFR_BIT {
	BCFR_RPAUSE = 0x0000ffff,
	BCFR_UNLIMITED = 0,
};


enum PIR_BIT {
	PIR_MDI = 0x08, PIR_MDO = 0x04, PIR_MMD = 0x02, PIR_MDC = 0x01,
};


enum PSR_BIT { PSR_LMON = 0x01, };


enum EESR_BIT {
	EESR_TWB1	= 0x80000000,
	EESR_TWB	= 0x40000000,	
	EESR_TC1	= 0x20000000,
	EESR_TUC	= 0x10000000,
	EESR_ROC	= 0x08000000,
	EESR_TABT	= 0x04000000,
	EESR_RABT	= 0x02000000,
	EESR_RFRMER	= 0x01000000,	
	EESR_ADE	= 0x00800000,
	EESR_ECI	= 0x00400000,
	EESR_FTC	= 0x00200000,	
	EESR_TDE	= 0x00100000,
	EESR_TFE	= 0x00080000,	
	EESR_FRC	= 0x00040000,	
	EESR_RDE	= 0x00020000,
	EESR_RFE	= 0x00010000,
	EESR_CND	= 0x00000800,
	EESR_DLC	= 0x00000400,
	EESR_CD		= 0x00000200,
	EESR_TRO	= 0x00000100,
	EESR_RMAF	= 0x00000080,
	EESR_CEEF	= 0x00000040,
	EESR_CELF	= 0x00000020,
	EESR_RRF	= 0x00000010,
	EESR_RTLF	= 0x00000008,
	EESR_RTSF	= 0x00000004,
	EESR_PRE	= 0x00000002,
	EESR_CERF	= 0x00000001,
};

#define EESR_RX_CHECK		(EESR_FRC  | 		\
				 EESR_RMAF |  \
				 EESR_RRF  | 	\
				 EESR_RTLF | 	\
				 EESR_RTSF | 	\
				 EESR_PRE  | 	\
				 EESR_CERF)  

#define DEFAULT_TX_CHECK	(EESR_FTC | EESR_CND | EESR_DLC | EESR_CD | \
				 EESR_TRO)
#define DEFAULT_EESR_ERR_CHECK	(EESR_TWB | EESR_TABT | EESR_RABT | EESR_RFE | \
				 EESR_RDE | EESR_RFRMER | EESR_ADE | \
				 EESR_TFE | EESR_TDE)


enum EESIPR_BIT {
	EESIPR_TWB1IP	= 0x80000000,
	EESIPR_TWBIP	= 0x40000000,	
	EESIPR_TC1IP	= 0x20000000,
	EESIPR_TUCIP	= 0x10000000,
	EESIPR_ROCIP	= 0x08000000,
	EESIPR_TABTIP	= 0x04000000,
	EESIPR_RABTIP	= 0x02000000,
	EESIPR_RFCOFIP	= 0x01000000,
	EESIPR_ADEIP	= 0x00800000,
	EESIPR_ECIIP	= 0x00400000,
	EESIPR_FTCIP	= 0x00200000,	
	EESIPR_TDEIP	= 0x00100000,
	EESIPR_TFUFIP	= 0x00080000,
	EESIPR_FRIP	= 0x00040000,
	EESIPR_RDEIP	= 0x00020000,
	EESIPR_RFOFIP	= 0x00010000,
	EESIPR_CNDIP	= 0x00000800,
	EESIPR_DLCIP	= 0x00000400,
	EESIPR_CDIP	= 0x00000200,
	EESIPR_TROIP	= 0x00000100,
	EESIPR_RMAFIP	= 0x00000080,
	EESIPR_CEEFIP	= 0x00000040,
	EESIPR_CELFIP	= 0x00000020,
	EESIPR_RRFIP	= 0x00000010,
	EESIPR_RTLFIP	= 0x00000008,
	EESIPR_RTSFIP	= 0x00000004,
	EESIPR_PREIP	= 0x00000002,
	EESIPR_CERFIP	= 0x00000001,
};


enum FCFTR_BIT {
	FCFTR_RFF2 = 0x00040000, FCFTR_RFF1 = 0x00020000,
	FCFTR_RFF0 = 0x00010000, FCFTR_RFD2 = 0x00000004,
	FCFTR_RFD1 = 0x00000002, FCFTR_RFD0 = 0x00000001,
};
#define DEFAULT_FIFO_F_D_RFF	(FCFTR_RFF2 | FCFTR_RFF1 | FCFTR_RFF0)
#define DEFAULT_FIFO_F_D_RFD	(FCFTR_RFD2 | FCFTR_RFD1 | FCFTR_RFD0)


enum RMCR_BIT {
	RMCR_RNC = 0x00000001,
};


enum ECMR_BIT {
	ECMR_TRCCM = 0x04000000, ECMR_RCSC = 0x00800000,
	ECMR_DPAD = 0x00200000, ECMR_RZPF = 0x00100000,
	ECMR_ZPF = 0x00080000, ECMR_PFR = 0x00040000, ECMR_RXF = 0x00020000,
	ECMR_TXF = 0x00010000, ECMR_MCT = 0x00002000, ECMR_PRCEF = 0x00001000,
	ECMR_MPDE = 0x00000200, ECMR_RE = 0x00000040, ECMR_TE = 0x00000020,
	ECMR_RTM = 0x00000010, ECMR_ILB = 0x00000008, ECMR_ELB = 0x00000004,
	ECMR_DM = 0x00000002, ECMR_PRM = 0x00000001,
};


enum ECSR_BIT {
	ECSR_BRCRX = 0x20, ECSR_PSRTO = 0x10,
	ECSR_LCHNG = 0x04,
	ECSR_MPD = 0x02, ECSR_ICD = 0x01,
};

#define DEFAULT_ECSR_INIT	(ECSR_BRCRX | ECSR_PSRTO | ECSR_LCHNG | \
				 ECSR_ICD | ECSIPR_MPDIP)


enum ECSIPR_BIT {
	ECSIPR_BRCRXIP = 0x20, ECSIPR_PSRTOIP = 0x10,
	ECSIPR_LCHNGIP = 0x04,
	ECSIPR_MPDIP = 0x02, ECSIPR_ICDIP = 0x01,
};

#define DEFAULT_ECSIPR_INIT	(ECSIPR_BRCRXIP | ECSIPR_PSRTOIP | \
				 ECSIPR_LCHNGIP | ECSIPR_ICDIP | ECSIPR_MPDIP)


enum APR_BIT {
	APR_AP = 0x0000ffff,
};


enum MPR_BIT {
	MPR_MP = 0x0000ffff,
};


enum TRSCER_BIT {
	TRSCER_CNDCE	= 0x00000800,
	TRSCER_DLCCE	= 0x00000400,
	TRSCER_CDCE	= 0x00000200,
	TRSCER_TROCE	= 0x00000100,
	TRSCER_RMAFCE	= 0x00000080,
	TRSCER_RRFCE	= 0x00000010,
	TRSCER_RTLFCE	= 0x00000008,
	TRSCER_RTSFCE	= 0x00000004,
	TRSCER_PRECE	= 0x00000002,
	TRSCER_CERFCE	= 0x00000001,
};

#define DEFAULT_TRSCER_ERR_MASK (TRSCER_RMAFCE | TRSCER_RRFCE | TRSCER_CDCE)


enum RPADIR_BIT {
	RPADIR_PADS = 0x1f0000, RPADIR_PADR = 0xffff,
};


#define DEFAULT_FDR_INIT	0x00000707


enum ARSTR_BIT { ARSTR_ARST = 0x00000001, };


enum TSU_FWEN0_BIT {
	TSU_FWEN0_0 = 0x00000001,
};


enum TSU_ADSBSY_BIT {
	TSU_ADSBSY_0 = 0x00000001,
};


enum TSU_TEN_BIT {
	TSU_TEN_0 = 0x80000000,
};


enum TSU_FWSL0_BIT {
	TSU_FWSL0_FW50 = 0x1000, TSU_FWSL0_FW40 = 0x0800,
	TSU_FWSL0_FW30 = 0x0400, TSU_FWSL0_FW20 = 0x0200,
	TSU_FWSL0_FW10 = 0x0100, TSU_FWSL0_RMSA0 = 0x0010,
};


enum TSU_FWSLC_BIT {
	TSU_FWSLC_POSTENU = 0x2000, TSU_FWSLC_POSTENL = 0x1000,
	TSU_FWSLC_CAMSEL03 = 0x0080, TSU_FWSLC_CAMSEL02 = 0x0040,
	TSU_FWSLC_CAMSEL01 = 0x0020, TSU_FWSLC_CAMSEL00 = 0x0010,
	TSU_FWSLC_CAMSEL13 = 0x0008, TSU_FWSLC_CAMSEL12 = 0x0004,
	TSU_FWSLC_CAMSEL11 = 0x0002, TSU_FWSLC_CAMSEL10 = 0x0001,
};


#define TSU_VTAG_ENABLE		0x80000000
#define TSU_VTAG_VID_MASK	0x00000fff


struct sh_eth_txdesc {
	u32 status;		
	u32 len;		
	u32 addr;		
	u32 pad0;		
} __aligned(2) __packed;


enum TD_STS_BIT {
	TD_TACT	= 0x80000000,
	TD_TDLE	= 0x40000000,
	TD_TFP1	= 0x20000000,
	TD_TFP0	= 0x10000000,
	TD_TFE	= 0x08000000,
	TD_TWBI	= 0x04000000,
};
#define TDF1ST	TD_TFP1
#define TDFEND	TD_TFP0
#define TD_TFP	(TD_TFP1 | TD_TFP0)


enum TD_LEN_BIT {
	TD_TBL	= 0xffff0000,	
};


struct sh_eth_rxdesc {
	u32 status;		
	u32 len;		
	u32 addr;		
	u32 pad0;		
} __aligned(2) __packed;


enum RD_STS_BIT {
	RD_RACT	= 0x80000000,
	RD_RDLE	= 0x40000000,
	RD_RFP1	= 0x20000000,
	RD_RFP0	= 0x10000000,
	RD_RFE	= 0x08000000,
	RD_RFS10 = 0x00000200,
	RD_RFS9	= 0x00000100,
	RD_RFS8	= 0x00000080,
	RD_RFS7	= 0x00000040,
	RD_RFS6	= 0x00000020,
	RD_RFS5	= 0x00000010,
	RD_RFS4	= 0x00000008,
	RD_RFS3	= 0x00000004,
	RD_RFS2	= 0x00000002,
	RD_RFS1	= 0x00000001,
};
#define RDF1ST	RD_RFP1
#define RDFEND	RD_RFP0
#define RD_RFP	(RD_RFP1 | RD_RFP0)


enum RD_LEN_BIT {
	RD_RFL	= 0x0000ffff,	
	RD_RBL	= 0xffff0000,	
};


struct sh_eth_cpu_data {
	
	int (*soft_reset)(struct net_device *ndev);

	
	void (*chip_reset)(struct net_device *ndev);
	void (*set_duplex)(struct net_device *ndev);
	void (*set_rate)(struct net_device *ndev);

	
	int register_type;
	u32 edtrr_trns;
	u32 eesipr_value;

	
	u32 ecsr_value;
	u32 ecsipr_value;
	u32 fdr_value;
	u32 fcftr_value;

	
	u32 tx_check;
	u32 eesr_err_check;

	
	u32 trscer_err_mask;

	
	unsigned long irq_flags; 
	unsigned no_psr:1;	
	unsigned apr:1;		
	unsigned mpr:1;		
	unsigned tpauser:1;	
	unsigned gecmr:1;	
	unsigned bculr:1;	
	unsigned tsu:1;		
	unsigned hw_swap:1;	
	unsigned nbst:1;	
	unsigned rpadir:1;	
	unsigned no_trimd:1;	
	unsigned no_ade:1;	
	unsigned no_xdfar:1;	
	unsigned xdfar_rw:1;	
	unsigned csmr:1;	
	unsigned rx_csum:1;	
	unsigned select_mii:1;	
	unsigned rmiimode:1;	
	unsigned rtrate:1;	
	unsigned magic:1;	
	unsigned no_tx_cntrs:1;	
	unsigned cexcr:1;	
	unsigned dual_port:1;	
};

struct sh_eth_private {
	struct platform_device *pdev;
	struct sh_eth_cpu_data *cd;
	const u16 *reg_offset;
	void __iomem *addr;
	void __iomem *tsu_addr;
	struct clk *clk;
	u32 num_rx_ring;
	u32 num_tx_ring;
	dma_addr_t rx_desc_dma;
	dma_addr_t tx_desc_dma;
	struct sh_eth_rxdesc *rx_ring;
	struct sh_eth_txdesc *tx_ring;
	struct sk_buff **rx_skbuff;
	struct sk_buff **tx_skbuff;
	spinlock_t lock;		
	u32 cur_rx, dirty_rx;		
	u32 cur_tx, dirty_tx;
	u32 rx_buf_sz;			
	struct napi_struct napi;
	bool irq_enabled;
	
	u32 phy_id;			
	struct mii_bus *mii_bus;	
	int link;
	phy_interface_t phy_interface;
	int msg_enable;
	int speed;
	int duplex;
	int port;			
	int vlan_num_ids;		

	unsigned no_ether_link:1;
	unsigned ether_link_active_low:1;
	unsigned is_opened:1;
	unsigned wol_enabled:1;
};

#endif	
