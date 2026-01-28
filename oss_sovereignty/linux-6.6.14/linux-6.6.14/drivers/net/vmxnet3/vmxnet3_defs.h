#ifndef _VMXNET3_DEFS_H_
#define _VMXNET3_DEFS_H_
#include "upt1_defs.h"
enum {
	VMXNET3_REG_VRRS	= 0x0,	 
	VMXNET3_REG_UVRS	= 0x8,	 
	VMXNET3_REG_DSAL	= 0x10,	 
	VMXNET3_REG_DSAH	= 0x18,	 
	VMXNET3_REG_CMD		= 0x20,	 
	VMXNET3_REG_MACL	= 0x28,	 
	VMXNET3_REG_MACH	= 0x30,	 
	VMXNET3_REG_ICR		= 0x38,	 
	VMXNET3_REG_ECR		= 0x40,  
	VMXNET3_REG_DCR         = 0x48,  
	VMXNET3_REG_PTCR        = 0x88,  
};
enum {
	VMXNET3_REG_IMR		= 0x0,	  
	VMXNET3_REG_TXPROD	= 0x600,  
	VMXNET3_REG_RXPROD	= 0x800,  
	VMXNET3_REG_RXPROD2	= 0xA00	  
};
enum {
	VMXNET3_REG_LB_TXPROD   = 0x1000,  
	VMXNET3_REG_LB_RXPROD   = 0x1400,  
	VMXNET3_REG_LB_RXPROD2  = 0x1800,  
};
#define VMXNET3_PT_REG_SIZE         4096		 
#define VMXNET3_LARGE_PT_REG_SIZE   8192		 
#define VMXNET3_VD_REG_SIZE         4096		 
#define VMXNET3_LARGE_BAR0_REG_SIZE (4096 * 4096)	 
#define VMXNET3_OOB_REG_SIZE        (4094 * 4096)	 
#define VMXNET3_REG_ALIGN       8	 
#define VMXNET3_REG_ALIGN_MASK  0x7
#define VMXNET3_IO_TYPE_PT              0
#define VMXNET3_IO_TYPE_VD              1
#define VMXNET3_IO_ADDR(type, reg)      (((type) << 24) | ((reg) & 0xFFFFFF))
#define VMXNET3_IO_TYPE(addr)           ((addr) >> 24)
#define VMXNET3_IO_REG(addr)            ((addr) & 0xFFFFFF)
enum {
	VMXNET3_CMD_FIRST_SET = 0xCAFE0000,
	VMXNET3_CMD_ACTIVATE_DEV = VMXNET3_CMD_FIRST_SET,
	VMXNET3_CMD_QUIESCE_DEV,
	VMXNET3_CMD_RESET_DEV,
	VMXNET3_CMD_UPDATE_RX_MODE,
	VMXNET3_CMD_UPDATE_MAC_FILTERS,
	VMXNET3_CMD_UPDATE_VLAN_FILTERS,
	VMXNET3_CMD_UPDATE_RSSIDT,
	VMXNET3_CMD_UPDATE_IML,
	VMXNET3_CMD_UPDATE_PMCFG,
	VMXNET3_CMD_UPDATE_FEATURE,
	VMXNET3_CMD_RESERVED1,
	VMXNET3_CMD_LOAD_PLUGIN,
	VMXNET3_CMD_RESERVED2,
	VMXNET3_CMD_RESERVED3,
	VMXNET3_CMD_SET_COALESCE,
	VMXNET3_CMD_REGISTER_MEMREGS,
	VMXNET3_CMD_SET_RSS_FIELDS,
	VMXNET3_CMD_RESERVED4,
	VMXNET3_CMD_RESERVED5,
	VMXNET3_CMD_SET_RING_BUFFER_SIZE,
	VMXNET3_CMD_FIRST_GET = 0xF00D0000,
	VMXNET3_CMD_GET_QUEUE_STATUS = VMXNET3_CMD_FIRST_GET,
	VMXNET3_CMD_GET_STATS,
	VMXNET3_CMD_GET_LINK,
	VMXNET3_CMD_GET_PERM_MAC_LO,
	VMXNET3_CMD_GET_PERM_MAC_HI,
	VMXNET3_CMD_GET_DID_LO,
	VMXNET3_CMD_GET_DID_HI,
	VMXNET3_CMD_GET_DEV_EXTRA_INFO,
	VMXNET3_CMD_GET_CONF_INTR,
	VMXNET3_CMD_GET_RESERVED1,
	VMXNET3_CMD_GET_TXDATA_DESC_SIZE,
	VMXNET3_CMD_GET_COALESCE,
	VMXNET3_CMD_GET_RSS_FIELDS,
	VMXNET3_CMD_GET_RESERVED2,
	VMXNET3_CMD_GET_RESERVED3,
	VMXNET3_CMD_GET_MAX_QUEUES_CONF,
	VMXNET3_CMD_GET_RESERVED4,
	VMXNET3_CMD_GET_MAX_CAPABILITIES,
	VMXNET3_CMD_GET_DCR0_REG,
};
struct Vmxnet3_TxDesc {
	__le64 addr;
#ifdef __BIG_ENDIAN_BITFIELD
	u32 msscof:14;   
	u32 ext1:1;      
	u32 dtype:1;     
	u32 oco:1;       
	u32 gen:1;       
	u32 len:14;
#else
	u32 len:14;
	u32 gen:1;       
	u32 oco:1;       
	u32 dtype:1;     
	u32 ext1:1;      
	u32 msscof:14;   
#endif   
#ifdef __BIG_ENDIAN_BITFIELD
	u32 tci:16;      
	u32 ti:1;        
	u32 ext2:1;
	u32 cq:1;        
	u32 eop:1;       
	u32 om:2;        
	u32 hlen:10;     
#else
	u32 hlen:10;     
	u32 om:2;        
	u32 eop:1;       
	u32 cq:1;        
	u32 ext2:1;
	u32 ti:1;        
	u32 tci:16;      
#endif   
};
#define VMXNET3_OM_NONE         0
#define VMXNET3_OM_ENCAP        1
#define VMXNET3_OM_CSUM         2
#define VMXNET3_OM_TSO          3
#define VMXNET3_TXD_EOP_SHIFT	12
#define VMXNET3_TXD_CQ_SHIFT	13
#define VMXNET3_TXD_GEN_SHIFT	14
#define VMXNET3_TXD_EOP_DWORD_SHIFT 3
#define VMXNET3_TXD_GEN_DWORD_SHIFT 2
#define VMXNET3_TXD_CQ		(1 << VMXNET3_TXD_CQ_SHIFT)
#define VMXNET3_TXD_EOP		(1 << VMXNET3_TXD_EOP_SHIFT)
#define VMXNET3_TXD_GEN		(1 << VMXNET3_TXD_GEN_SHIFT)
#define VMXNET3_HDR_COPY_SIZE   128
struct Vmxnet3_TxDataDesc {
	u8		data[VMXNET3_HDR_COPY_SIZE];
};
typedef u8 Vmxnet3_RxDataDesc;
#define VMXNET3_TCD_GEN_SHIFT	31
#define VMXNET3_TCD_GEN_SIZE	1
#define VMXNET3_TCD_TXIDX_SHIFT	0
#define VMXNET3_TCD_TXIDX_SIZE	12
#define VMXNET3_TCD_GEN_DWORD_SHIFT	3
struct Vmxnet3_TxCompDesc {
	u32		txdIdx:12;     
	u32		ext1:20;
	__le32		ext2;
	__le32		ext3;
	u32		rsvd:24;
	u32		type:7;        
	u32		gen:1;         
};
struct Vmxnet3_RxDesc {
	__le64		addr;
#ifdef __BIG_ENDIAN_BITFIELD
	u32		gen:1;         
	u32		rsvd:15;
	u32		dtype:1;       
	u32		btype:1;       
	u32		len:14;
#else
	u32		len:14;
	u32		btype:1;       
	u32		dtype:1;       
	u32		rsvd:15;
	u32		gen:1;         
#endif
	u32		ext1;
};
#define VMXNET3_RXD_BTYPE_HEAD   0     
#define VMXNET3_RXD_BTYPE_BODY   1     
#define VMXNET3_RXD_BTYPE_SHIFT  14
#define VMXNET3_RXD_GEN_SHIFT    31
#define VMXNET3_RCD_HDR_INNER_SHIFT  13
struct Vmxnet3_RxCompDesc {
#ifdef __BIG_ENDIAN_BITFIELD
	u32		ext2:1;
	u32		cnc:1;         
	u32		rssType:4;     
	u32		rqID:10;       
	u32		sop:1;         
	u32		eop:1;         
	u32		ext1:2;        
	u32		rxdIdx:12;     
#else
	u32		rxdIdx:12;     
	u32		ext1:2;        
	u32		eop:1;         
	u32		sop:1;         
	u32		rqID:10;       
	u32		rssType:4;     
	u32		cnc:1;         
	u32		ext2:1;
#endif   
	__le32		rssHash;       
#ifdef __BIG_ENDIAN_BITFIELD
	u32		tci:16;        
	u32		ts:1;          
	u32		err:1;         
	u32		len:14;        
#else
	u32		len:14;        
	u32		err:1;         
	u32		ts:1;          
	u32		tci:16;        
#endif   
#ifdef __BIG_ENDIAN_BITFIELD
	u32		gen:1;         
	u32		type:7;        
	u32		fcs:1;         
	u32		frg:1;         
	u32		v4:1;          
	u32		v6:1;          
	u32		ipc:1;         
	u32		tcp:1;         
	u32		udp:1;         
	u32		tuc:1;         
	u32		csum:16;
#else
	u32		csum:16;
	u32		tuc:1;         
	u32		udp:1;         
	u32		tcp:1;         
	u32		ipc:1;         
	u32		v6:1;          
	u32		v4:1;          
	u32		frg:1;         
	u32		fcs:1;         
	u32		type:7;        
	u32		gen:1;         
#endif   
};
struct Vmxnet3_RxCompDescExt {
	__le32		dword1;
	u8		segCnt;        
	u8		dupAckCnt;     
	__le16		tsDelta;       
	__le32		dword2;
#ifdef __BIG_ENDIAN_BITFIELD
	u32		gen:1;         
	u32		type:7;        
	u32		fcs:1;         
	u32		frg:1;         
	u32		v4:1;          
	u32		v6:1;          
	u32		ipc:1;         
	u32		tcp:1;         
	u32		udp:1;         
	u32		tuc:1;         
	u32		mss:16;
#else
	u32		mss:16;
	u32		tuc:1;         
	u32		udp:1;         
	u32		tcp:1;         
	u32		ipc:1;         
	u32		v6:1;          
	u32		v4:1;          
	u32		frg:1;         
	u32		fcs:1;         
	u32		type:7;        
	u32		gen:1;         
#endif   
};
#define VMXNET3_RCD_TUC_SHIFT	16
#define VMXNET3_RCD_IPC_SHIFT	19
#define VMXNET3_RCD_TYPE_SHIFT	56
#define VMXNET3_RCD_GEN_SHIFT	63
#define VMXNET3_RCD_CSUM_OK (1 << VMXNET3_RCD_TUC_SHIFT | \
			     1 << VMXNET3_RCD_IPC_SHIFT)
#define VMXNET3_TXD_GEN_SIZE 1
#define VMXNET3_TXD_EOP_SIZE 1
#define VMXNET3_RCD_RSS_TYPE_NONE     0
#define VMXNET3_RCD_RSS_TYPE_IPV4     1
#define VMXNET3_RCD_RSS_TYPE_TCPIPV4  2
#define VMXNET3_RCD_RSS_TYPE_IPV6     3
#define VMXNET3_RCD_RSS_TYPE_TCPIPV6  4
#define VMXNET3_RCD_RSS_TYPE_UDPIPV4  5
#define VMXNET3_RCD_RSS_TYPE_UDPIPV6  6
#define VMXNET3_RCD_RSS_TYPE_ESPIPV4  7
#define VMXNET3_RCD_RSS_TYPE_ESPIPV6  8
union Vmxnet3_GenericDesc {
	__le64				qword[2];
	__le32				dword[4];
	__le16				word[8];
	struct Vmxnet3_TxDesc		txd;
	struct Vmxnet3_RxDesc		rxd;
	struct Vmxnet3_TxCompDesc	tcd;
	struct Vmxnet3_RxCompDesc	rcd;
	struct Vmxnet3_RxCompDescExt 	rcdExt;
};
#define VMXNET3_INIT_GEN       1
#define VMXNET3_MAX_TX_BUF_SIZE  (1 << 14)
#define VMXNET3_TXD_NEEDED(size) (((size) + VMXNET3_MAX_TX_BUF_SIZE - 1) / \
				  VMXNET3_MAX_TX_BUF_SIZE)
#define VMXNET3_MAX_TXD_PER_PKT 16
#define VMXNET3_MAX_TSO_TXD_PER_PKT 24
#define VMXNET3_MAX_RX_BUF_SIZE  ((1 << 14) - 1)
#define VMXNET3_MIN_T0_BUF_SIZE  128
#define VMXNET3_MAX_CSUM_OFFSET  1024
#define VMXNET3_RING_BA_ALIGN   512
#define VMXNET3_RING_BA_MASK    (VMXNET3_RING_BA_ALIGN - 1)
#define VMXNET3_RING_SIZE_ALIGN 32
#define VMXNET3_RING_SIZE_MASK  (VMXNET3_RING_SIZE_ALIGN - 1)
#define VMXNET3_TXDATA_DESC_SIZE_ALIGN 64
#define VMXNET3_TXDATA_DESC_SIZE_MASK  (VMXNET3_TXDATA_DESC_SIZE_ALIGN - 1)
#define VMXNET3_RXDATA_DESC_SIZE_ALIGN 64
#define VMXNET3_RXDATA_DESC_SIZE_MASK  (VMXNET3_RXDATA_DESC_SIZE_ALIGN - 1)
#define VMXNET3_TX_RING_MAX_SIZE   4096
#define VMXNET3_TC_RING_MAX_SIZE   4096
#define VMXNET3_RX_RING_MAX_SIZE   4096
#define VMXNET3_RX_RING2_MAX_SIZE  4096
#define VMXNET3_RC_RING_MAX_SIZE   8192
#define VMXNET3_TXDATA_DESC_MIN_SIZE 128
#define VMXNET3_TXDATA_DESC_MAX_SIZE 2048
#define VMXNET3_RXDATA_DESC_MAX_SIZE 2048
enum {
 VMXNET3_ERR_NOEOP        = 0x80000000,   
 VMXNET3_ERR_TXD_REUSE    = 0x80000001,   
 VMXNET3_ERR_BIG_PKT      = 0x80000002,   
 VMXNET3_ERR_DESC_NOT_SPT = 0x80000003,   
 VMXNET3_ERR_SMALL_BUF    = 0x80000004,   
 VMXNET3_ERR_STRESS       = 0x80000005,   
 VMXNET3_ERR_SWITCH       = 0x80000006,   
 VMXNET3_ERR_TXD_INVALID  = 0x80000007,   
};
#define VMXNET3_CDTYPE_TXCOMP      0     
#define VMXNET3_CDTYPE_RXCOMP      3     
#define VMXNET3_CDTYPE_RXCOMP_LRO  4     
enum {
	VMXNET3_GOS_BITS_UNK    = 0,    
	VMXNET3_GOS_BITS_32     = 1,
	VMXNET3_GOS_BITS_64     = 2,
};
#define VMXNET3_GOS_TYPE_LINUX	1
struct Vmxnet3_GOSInfo {
#ifdef __BIG_ENDIAN_BITFIELD
	u32		gosMisc:10;     
	u32		gosVer:16;      
	u32		gosType:4;      
	u32		gosBits:2;     
#else
	u32		gosBits:2;      
	u32		gosType:4;      
	u32		gosVer:16;      
	u32		gosMisc:10;     
#endif   
};
struct Vmxnet3_DriverInfo {
	__le32				version;
	struct Vmxnet3_GOSInfo		gos;
	__le32				vmxnet3RevSpt;
	__le32				uptVerSpt;
};
#define VMXNET3_REV1_MAGIC  3133079265u
#define VMXNET3_QUEUE_DESC_ALIGN  128
struct Vmxnet3_MiscConf {
	struct Vmxnet3_DriverInfo driverInfo;
	__le64		uptFeatures;
	__le64		ddPA;          
	__le64		queueDescPA;   
	__le32		ddLen;         
	__le32		queueDescLen;  
	__le32		mtu;
	__le16		maxNumRxSG;
	u8		numTxQueues;
	u8		numRxQueues;
	__le32		reserved[4];
};
struct Vmxnet3_TxQueueConf {
	__le64		txRingBasePA;
	__le64		dataRingBasePA;
	__le64		compRingBasePA;
	__le64		ddPA;          
	__le64		reserved;
	__le32		txRingSize;    
	__le32		dataRingSize;  
	__le32		compRingSize;  
	__le32		ddLen;         
	u8		intrIdx;
	u8		_pad1[1];
	__le16		txDataRingDescSize;
	u8		_pad2[4];
};
struct Vmxnet3_RxQueueConf {
	__le64		rxRingBasePA[2];
	__le64		compRingBasePA;
	__le64		ddPA;             
	__le64		rxDataRingBasePA;
	__le32		rxRingSize[2];    
	__le32		compRingSize;     
	__le32		ddLen;            
	u8		intrIdx;
	u8		_pad1[1];
	__le16		rxDataRingDescSize;   
	u8		_pad2[4];
};
enum vmxnet3_intr_mask_mode {
	VMXNET3_IMM_AUTO   = 0,
	VMXNET3_IMM_ACTIVE = 1,
	VMXNET3_IMM_LAZY   = 2
};
enum vmxnet3_intr_type {
	VMXNET3_IT_AUTO = 0,
	VMXNET3_IT_INTX = 1,
	VMXNET3_IT_MSI  = 2,
	VMXNET3_IT_MSIX = 3
};
#define VMXNET3_MAX_TX_QUEUES  8
#define VMXNET3_MAX_RX_QUEUES  16
#define VMXNET3_MAX_INTRS      25
#define VMXNET3_EXT_MAX_TX_QUEUES  32
#define VMXNET3_EXT_MAX_RX_QUEUES  32
#define VMXNET3_EXT_MAX_INTRS      65
#define VMXNET3_FIRST_SET_INTRS    64
#define VMXNET3_IC_DISABLE_ALL  0x1    
struct Vmxnet3_IntrConf {
	bool		autoMask;
	u8		numIntrs;       
	u8		eventIntrIdx;
	u8		modLevels[VMXNET3_MAX_INTRS];	 
	__le32		intrCtrl;
	__le32		reserved[2];
};
struct Vmxnet3_IntrConfExt {
	u8              autoMask;
	u8              numIntrs;       
	u8              eventIntrIdx;
	u8              reserved;
	__le32          intrCtrl;
	__le32          reserved1;
	u8              modLevels[VMXNET3_EXT_MAX_INTRS];  
	u8              reserved2[3];
};
#define VMXNET3_VFT_SIZE  (4096 / (sizeof(u32) * 8))
struct Vmxnet3_QueueStatus {
	bool		stopped;
	u8		_pad[3];
	__le32		error;
};
struct Vmxnet3_TxQueueCtrl {
	__le32		txNumDeferred;
	__le32		txThreshold;
	__le64		reserved;
};
struct Vmxnet3_RxQueueCtrl {
	bool		updateRxProd;
	u8		_pad[7];
	__le64		reserved;
};
enum {
	VMXNET3_RXM_UCAST     = 0x01,   
	VMXNET3_RXM_MCAST     = 0x02,   
	VMXNET3_RXM_BCAST     = 0x04,   
	VMXNET3_RXM_ALL_MULTI = 0x08,   
	VMXNET3_RXM_PROMISC   = 0x10   
};
struct Vmxnet3_RxFilterConf {
	__le32		rxMode;        
	__le16		mfTableLen;    
	__le16		_pad1;
	__le64		mfTablePA;     
	__le32		vfTable[VMXNET3_VFT_SIZE];  
};
#define VMXNET3_PM_MAX_FILTERS        6
#define VMXNET3_PM_MAX_PATTERN_SIZE   128
#define VMXNET3_PM_MAX_MASK_SIZE      (VMXNET3_PM_MAX_PATTERN_SIZE / 8)
#define VMXNET3_PM_WAKEUP_MAGIC       cpu_to_le16(0x01)   
#define VMXNET3_PM_WAKEUP_FILTER      cpu_to_le16(0x02)   
struct Vmxnet3_PM_PktFilter {
	u8		maskSize;
	u8		patternSize;
	u8		mask[VMXNET3_PM_MAX_MASK_SIZE];
	u8		pattern[VMXNET3_PM_MAX_PATTERN_SIZE];
	u8		pad[6];
};
struct Vmxnet3_PMConf {
	__le16		wakeUpEvents;   
	u8		numFilters;
	u8		pad[5];
	struct Vmxnet3_PM_PktFilter filters[VMXNET3_PM_MAX_FILTERS];
};
struct Vmxnet3_VariableLenConfDesc {
	__le32		confVer;
	__le32		confLen;
	__le64		confPA;
};
struct Vmxnet3_TxQueueDesc {
	struct Vmxnet3_TxQueueCtrl		ctrl;
	struct Vmxnet3_TxQueueConf		conf;
	struct Vmxnet3_QueueStatus		status;
	struct UPT1_TxStats			stats;
	u8					_pad[88];  
};
struct Vmxnet3_RxQueueDesc {
	struct Vmxnet3_RxQueueCtrl		ctrl;
	struct Vmxnet3_RxQueueConf		conf;
	struct Vmxnet3_QueueStatus		status;
	struct UPT1_RxStats			stats;
	u8				      __pad[88];  
};
struct Vmxnet3_SetPolling {
	u8					enablePolling;
};
#define VMXNET3_COAL_STATIC_MAX_DEPTH		128
#define VMXNET3_COAL_RBC_MIN_RATE		100
#define VMXNET3_COAL_RBC_MAX_RATE		100000
enum Vmxnet3_CoalesceMode {
	VMXNET3_COALESCE_DISABLED   = 0,
	VMXNET3_COALESCE_ADAPT      = 1,
	VMXNET3_COALESCE_STATIC     = 2,
	VMXNET3_COALESCE_RBC        = 3
};
struct Vmxnet3_CoalesceRbc {
	u32					rbc_rate;
};
struct Vmxnet3_CoalesceStatic {
	u32					tx_depth;
	u32					tx_comp_depth;
	u32					rx_depth;
};
struct Vmxnet3_CoalesceScheme {
	enum Vmxnet3_CoalesceMode		coalMode;
	union {
		struct Vmxnet3_CoalesceRbc	coalRbc;
		struct Vmxnet3_CoalesceStatic	coalStatic;
	} coalPara;
};
struct Vmxnet3_MemoryRegion {
	__le64					startPA;
	__le32					length;
	__le16					txQueueBits;
	__le16					rxQueueBits;
};
#define MAX_MEMORY_REGION_PER_QUEUE 16
#define MAX_MEMORY_REGION_PER_DEVICE 256
struct Vmxnet3_MemRegs {
	__le16					numRegs;
	__le16					pad[3];
	struct Vmxnet3_MemoryRegion		memRegs[1];
};
enum Vmxnet3_RSSField {
	VMXNET3_RSS_FIELDS_TCPIP4 = 0x0001,
	VMXNET3_RSS_FIELDS_TCPIP6 = 0x0002,
	VMXNET3_RSS_FIELDS_UDPIP4 = 0x0004,
	VMXNET3_RSS_FIELDS_UDPIP6 = 0x0008,
	VMXNET3_RSS_FIELDS_ESPIP4 = 0x0010,
	VMXNET3_RSS_FIELDS_ESPIP6 = 0x0020,
};
struct Vmxnet3_RingBufferSize {
	__le16             ring1BufSizeType0;
	__le16             ring1BufSizeType1;
	__le16             ring2BufSizeType1;
	__le16             pad;
};
union Vmxnet3_CmdInfo {
	struct Vmxnet3_VariableLenConfDesc	varConf;
	struct Vmxnet3_SetPolling		setPolling;
	enum   Vmxnet3_RSSField                 setRssFields;
	struct Vmxnet3_RingBufferSize           ringBufSize;
	__le64					data[2];
};
struct Vmxnet3_DSDevRead {
	struct Vmxnet3_MiscConf			misc;
	struct Vmxnet3_IntrConf			intrConf;
	struct Vmxnet3_RxFilterConf		rxFilterConf;
	struct Vmxnet3_VariableLenConfDesc	rssConfDesc;
	struct Vmxnet3_VariableLenConfDesc	pmConfDesc;
	struct Vmxnet3_VariableLenConfDesc	pluginConfDesc;
};
struct Vmxnet3_DSDevReadExt {
	struct Vmxnet3_IntrConfExt              intrConfExt;
};
struct Vmxnet3_DriverShared {
	__le32				magic;
	__le32                          size;  
	struct Vmxnet3_DSDevRead	devRead;
	__le32				ecr;
	__le32				reserved;
	union {
		__le32			reserved1[4];
		union Vmxnet3_CmdInfo	cmdInfo;  
	} cu;
	struct Vmxnet3_DSDevReadExt     devReadExt;
};
#define VMXNET3_ECR_RQERR       (1 << 0)
#define VMXNET3_ECR_TQERR       (1 << 1)
#define VMXNET3_ECR_LINK        (1 << 2)
#define VMXNET3_ECR_DIC         (1 << 3)
#define VMXNET3_ECR_DEBUG       (1 << 4)
#define VMXNET3_FLIP_RING_GEN(gen) ((gen) = (gen) ^ 0x1)
#define VMXNET3_INC_RING_IDX_ONLY(idx, ring_size) \
	do {\
		(idx)++;\
		if (unlikely((idx) == (ring_size))) {\
			(idx) = 0;\
		} \
	} while (0)
#define VMXNET3_SET_VFTABLE_ENTRY(vfTable, vid) \
	(vfTable[vid >> 5] |= (1 << (vid & 31)))
#define VMXNET3_CLEAR_VFTABLE_ENTRY(vfTable, vid) \
	(vfTable[vid >> 5] &= ~(1 << (vid & 31)))
#define VMXNET3_VFTABLE_ENTRY_IS_SET(vfTable, vid) \
	((vfTable[vid >> 5] & (1 << (vid & 31))) != 0)
#define VMXNET3_MAX_MTU     9000
#define VMXNET3_V6_MAX_MTU  9190
#define VMXNET3_MIN_MTU     60
#define VMXNET3_LINK_UP         (10000 << 16 | 1)     
#define VMXNET3_LINK_DOWN       0
#define VMXNET3_DCR_ERROR                          31    
#define VMXNET3_CAP_UDP_RSS                        0     
#define VMXNET3_CAP_ESP_RSS_IPV4                   1     
#define VMXNET3_CAP_GENEVE_CHECKSUM_OFFLOAD        2     
#define VMXNET3_CAP_GENEVE_TSO                     3     
#define VMXNET3_CAP_VXLAN_CHECKSUM_OFFLOAD         4     
#define VMXNET3_CAP_VXLAN_TSO                      5     
#define VMXNET3_CAP_GENEVE_OUTER_CHECKSUM_OFFLOAD  6     
#define VMXNET3_CAP_VXLAN_OUTER_CHECKSUM_OFFLOAD   7     
#define VMXNET3_CAP_PKT_STEERING_IPV4              8     
#define VMXNET3_CAP_VERSION_4_MAX                  VMXNET3_CAP_PKT_STEERING_IPV4
#define VMXNET3_CAP_ESP_RSS_IPV6                   9     
#define VMXNET3_CAP_VERSION_5_MAX                  VMXNET3_CAP_ESP_RSS_IPV6
#define VMXNET3_CAP_ESP_OVER_UDP_RSS               10    
#define VMXNET3_CAP_INNER_RSS                      11    
#define VMXNET3_CAP_INNER_ESP_RSS                  12    
#define VMXNET3_CAP_CRC32_HASH_FUNC                13    
#define VMXNET3_CAP_VERSION_6_MAX                  VMXNET3_CAP_CRC32_HASH_FUNC
#define VMXNET3_CAP_OAM_FILTER                     14    
#define VMXNET3_CAP_ESP_QS                         15    
#define VMXNET3_CAP_LARGE_BAR                      16    
#define VMXNET3_CAP_OOORX_COMP                     17    
#define VMXNET3_CAP_VERSION_7_MAX                  18
#define VMXNET3_CAP_MAX                            VMXNET3_CAP_VERSION_7_MAX
#endif  
