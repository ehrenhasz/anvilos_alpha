#ifndef _SLIC_H
#define _SLIC_H
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/spinlock_types.h>
#include <linux/dma-mapping.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/u64_stats_sync.h>
#define SLIC_VGBSTAT_XPERR		0x40000000
#define SLIC_VGBSTAT_XERRSHFT		25
#define SLIC_VGBSTAT_XCSERR		0x23
#define SLIC_VGBSTAT_XUFLOW		0x22
#define SLIC_VGBSTAT_XHLEN		0x20
#define SLIC_VGBSTAT_NETERR		0x01000000
#define SLIC_VGBSTAT_NERRSHFT		16
#define SLIC_VGBSTAT_NERRMSK		0x1ff
#define SLIC_VGBSTAT_NCSERR		0x103
#define SLIC_VGBSTAT_NUFLOW		0x102
#define SLIC_VGBSTAT_NHLEN		0x100
#define SLIC_VGBSTAT_LNKERR		0x00000080
#define SLIC_VGBSTAT_LERRMSK		0xff
#define SLIC_VGBSTAT_LDEARLY		0x86
#define SLIC_VGBSTAT_LBOFLO		0x85
#define SLIC_VGBSTAT_LCODERR		0x84
#define SLIC_VGBSTAT_LDBLNBL		0x83
#define SLIC_VGBSTAT_LCRCERR		0x82
#define SLIC_VGBSTAT_LOFLO		0x81
#define SLIC_VGBSTAT_LUFLO		0x80
#define SLIC_IRHDDR_FLEN_MSK		0x0000ffff
#define SLIC_IRHDDR_SVALID		0x80000000
#define SLIC_IRHDDR_ERR			0x10000000
#define SLIC_VRHSTAT_802OE		0x80000000
#define SLIC_VRHSTAT_TPOFLO		0x10000000
#define SLIC_VRHSTATB_802UE		0x80000000
#define SLIC_VRHSTATB_RCVE		0x40000000
#define SLIC_VRHSTATB_BUFF		0x20000000
#define SLIC_VRHSTATB_CARRE		0x08000000
#define SLIC_VRHSTATB_LONGE		0x02000000
#define SLIC_VRHSTATB_PREA		0x01000000
#define SLIC_VRHSTATB_CRC		0x00800000
#define SLIC_VRHSTATB_DRBL		0x00400000
#define SLIC_VRHSTATB_CODE		0x00200000
#define SLIC_VRHSTATB_TPCSUM		0x00100000
#define SLIC_VRHSTATB_TPHLEN		0x00080000
#define SLIC_VRHSTATB_IPCSUM		0x00040000
#define SLIC_VRHSTATB_IPLERR		0x00020000
#define SLIC_VRHSTATB_IPHERR		0x00010000
#define SLIC_CMD_XMT_REQ		0x01
#define SLIC_CMD_TYPE_DUMB		3
#define SLIC_RESET_MAGIC		0xDEAD
#define SLIC_ICR_INT_OFF		0
#define SLIC_ICR_INT_ON			1
#define SLIC_ICR_INT_MASK		2
#define SLIC_ISR_ERR			0x80000000
#define SLIC_ISR_RCV			0x40000000
#define SLIC_ISR_CMD			0x20000000
#define SLIC_ISR_IO			0x60000000
#define SLIC_ISR_UPC			0x10000000
#define SLIC_ISR_LEVENT			0x08000000
#define SLIC_ISR_RMISS			0x02000000
#define SLIC_ISR_UPCERR			0x01000000
#define SLIC_ISR_XDROP			0x00800000
#define SLIC_ISR_UPCBSY			0x00020000
#define SLIC_ISR_PING_MASK		0x00700000
#define SLIC_ISR_UPCERR_MASK		(SLIC_ISR_UPCERR | SLIC_ISR_UPCBSY)
#define SLIC_ISR_UPC_MASK		(SLIC_ISR_UPC | SLIC_ISR_UPCERR_MASK)
#define SLIC_WCS_START			0x80000000
#define SLIC_WCS_COMPARE		0x40000000
#define SLIC_RCVWCS_BEGIN		0x40000000
#define SLIC_RCVWCS_FINISH		0x80000000
#define SLIC_MIICR_REG_16		0x00100000
#define SLIC_MRV_REG16_XOVERON		0x0068
#define SLIC_GIG_LINKUP			0x0001
#define SLIC_GIG_FULLDUPLEX		0x0002
#define SLIC_GIG_SPEED_MASK		0x000C
#define SLIC_GIG_SPEED_1000		0x0008
#define SLIC_GIG_SPEED_100		0x0004
#define SLIC_GIG_SPEED_10		0x0000
#define SLIC_GMCR_RESET			0x80000000
#define SLIC_GMCR_GBIT			0x20000000
#define SLIC_GMCR_FULLD			0x10000000
#define SLIC_GMCR_GAPBB_SHIFT		14
#define SLIC_GMCR_GAPR1_SHIFT		7
#define SLIC_GMCR_GAPR2_SHIFT		0
#define SLIC_GMCR_GAPBB_1000		0x60
#define SLIC_GMCR_GAPR1_1000		0x2C
#define SLIC_GMCR_GAPR2_1000		0x40
#define SLIC_GMCR_GAPBB_100		0x70
#define SLIC_GMCR_GAPR1_100		0x2C
#define SLIC_GMCR_GAPR2_100		0x40
#define SLIC_XCR_RESET			0x80000000
#define SLIC_XCR_XMTEN			0x40000000
#define SLIC_XCR_PAUSEEN		0x20000000
#define SLIC_XCR_LOADRNG		0x10000000
#define SLIC_GXCR_RESET			0x80000000
#define SLIC_GXCR_XMTEN			0x40000000
#define SLIC_GXCR_PAUSEEN		0x20000000
#define SLIC_GRCR_RESET			0x80000000
#define SLIC_GRCR_RCVEN			0x40000000
#define SLIC_GRCR_RCVALL		0x20000000
#define SLIC_GRCR_RCVBAD		0x10000000
#define SLIC_GRCR_CTLEN			0x08000000
#define SLIC_GRCR_ADDRAEN		0x02000000
#define SLIC_GRCR_HASHSIZE_SHIFT	17
#define SLIC_GRCR_HASHSIZE		14
#define SLIC_REG_RESET			0x0000
#define SLIC_REG_ICR			0x0008
#define SLIC_REG_ISP			0x0010
#define SLIC_REG_ISR			0x0018
#define SLIC_REG_HBAR			0x0020
#define SLIC_REG_DBAR			0x0028
#define SLIC_REG_CBAR			0x0030
#define	SLIC_REG_WCS			0x0034
#define	SLIC_REG_RBAR			0x0038
#define	SLIC_REG_RSTAT			0x0040
#define	SLIC_REG_LSTAT			0x0048
#define	SLIC_REG_WMCFG			0x0050
#define SLIC_REG_WPHY			0x0058
#define	SLIC_REG_RCBAR			0x0060
#define SLIC_REG_RCONFIG		0x0068
#define SLIC_REG_INTAGG			0x0070
#define	SLIC_REG_WXCFG			0x0078
#define	SLIC_REG_WRCFG			0x0080
#define	SLIC_REG_WRADDRAL		0x0088
#define	SLIC_REG_WRADDRAH		0x0090
#define	SLIC_REG_WRADDRBL		0x0098
#define	SLIC_REG_WRADDRBH		0x00a0
#define	SLIC_REG_MCASTLOW		0x00a8
#define	SLIC_REG_MCASTHIGH		0x00b0
#define SLIC_REG_PING			0x00b8
#define SLIC_REG_DUMP_CMD		0x00c0
#define SLIC_REG_DUMP_DATA		0x00c8
#define	SLIC_REG_PCISTATUS		0x00d0
#define SLIC_REG_WRHOSTID		0x00d8
#define SLIC_REG_LOW_POWER		0x00e0
#define SLIC_REG_QUIESCE		0x00e8
#define SLIC_REG_RESET_IFACE		0x00f0
#define SLIC_REG_ADDR_UPPER		0x00f8
#define SLIC_REG_HBAR64			0x0100
#define SLIC_REG_DBAR64			0x0108
#define SLIC_REG_CBAR64			0x0110
#define SLIC_REG_RBAR64			0x0118
#define	SLIC_REG_RCBAR64		0x0120
#define	SLIC_REG_RSTAT64		0x0128
#define SLIC_REG_RCV_WCS		0x0130
#define SLIC_REG_WRVLANID		0x0138
#define SLIC_REG_READ_XF_INFO		0x0140
#define SLIC_REG_WRITE_XF_INFO		0x0148
#define SLIC_REG_TICKS_PER_SEC		0x0170
#define SLIC_REG_HOSTID			0x1554
#define PCI_VENDOR_ID_ALACRITECH		0x139A
#define PCI_DEVICE_ID_ALACRITECH_MOJAVE		0x0005
#define PCI_SUBDEVICE_ID_ALACRITECH_1000X1	0x0005
#define PCI_SUBDEVICE_ID_ALACRITECH_1000X1_2	0x0006
#define PCI_SUBDEVICE_ID_ALACRITECH_1000X1F	0x0007
#define PCI_SUBDEVICE_ID_ALACRITECH_CICADA	0x0008
#define PCI_SUBDEVICE_ID_ALACRITECH_SES1001T	0x2006
#define PCI_SUBDEVICE_ID_ALACRITECH_SES1001F	0x2007
#define PCI_DEVICE_ID_ALACRITECH_OASIS		0x0007
#define PCI_SUBDEVICE_ID_ALACRITECH_SEN2002XT	0x000B
#define PCI_SUBDEVICE_ID_ALACRITECH_SEN2002XF	0x000C
#define PCI_SUBDEVICE_ID_ALACRITECH_SEN2001XT	0x000D
#define PCI_SUBDEVICE_ID_ALACRITECH_SEN2001XF	0x000E
#define PCI_SUBDEVICE_ID_ALACRITECH_SEN2104EF	0x000F
#define PCI_SUBDEVICE_ID_ALACRITECH_SEN2104ET	0x0010
#define PCI_SUBDEVICE_ID_ALACRITECH_SEN2102EF	0x0011
#define PCI_SUBDEVICE_ID_ALACRITECH_SEN2102ET	0x0012
#define SLIC_NUM_RX_LES			256
#define SLIC_RX_BUFF_SIZE		2048
#define SLIC_RX_BUFF_ALIGN		256
#define SLIC_RX_BUFF_HDR_SIZE		34
#define SLIC_MAX_REQ_RX_DESCS		1
#define SLIC_NUM_TX_DESCS		256
#define SLIC_TX_DESC_ALIGN		32
#define SLIC_MIN_TX_WAKEUP_DESCS	10
#define SLIC_MAX_REQ_TX_DESCS		1
#define SLIC_MAX_TX_COMPLETIONS		100
#define SLIC_NUM_STAT_DESCS		128
#define SLIC_STATS_DESC_ALIGN		256
#define SLIC_NUM_STAT_DESC_ARRAYS	4
#define SLIC_INVALID_STAT_DESC_IDX	0xffffffff
#define SLIC_UPR_LSTAT			0
#define SLIC_UPR_CONFIG			1
#define SLIC_EEPROM_SIZE		128
#define SLIC_EEPROM_MAGIC		0xa5a5
#define SLIC_FIRMWARE_MOJAVE		"slicoss/gbdownload.sys"
#define SLIC_FIRMWARE_OASIS		"slicoss/oasisdownload.sys"
#define SLIC_RCV_FIRMWARE_MOJAVE	"slicoss/gbrcvucode.sys"
#define SLIC_RCV_FIRMWARE_OASIS		"slicoss/oasisrcvucode.sys"
#define SLIC_FIRMWARE_MIN_SIZE		64
#define SLIC_FIRMWARE_MAX_SECTIONS	3
#define SLIC_MODEL_MOJAVE		0
#define SLIC_MODEL_OASIS		1
#define SLIC_INC_STATS_COUNTER(st, counter)	\
do {						\
	u64_stats_update_begin(&(st)->syncp);	\
	(st)->counter++;			\
	u64_stats_update_end(&(st)->syncp);	\
} while (0)
#define SLIC_GET_STATS_COUNTER(newst, st, counter)		\
{								\
	unsigned int start;					\
	do {							\
		start = u64_stats_fetch_begin(&(st)->syncp);	\
		newst = (st)->counter;				\
	} while (u64_stats_fetch_retry(&(st)->syncp, start));	\
}
struct slic_upr {
	dma_addr_t paddr;
	unsigned int type;
	struct list_head list;
};
struct slic_upr_list {
	bool pending;
	struct list_head list;
	spinlock_t lock;
};
struct slic_mojave_eeprom {
	__le16 id;		 
	__le16 eeprom_code_size; 
	__le16 flash_size;	 
	__le16 eeprom_size;	 
	__le16 vendor_id;	 
	__le16 dev_id;		 
	u8 rev_id;		 
	u8 class_code[3];	 
	u8 irqpin_dbg;		 
	u8 irqpin;		 
	u8 min_grant;		 
	u8 max_lat;		 
	__le16 pci_stat;	 
	__le16 sub_vendor_id;	 
	__le16 sub_id;		 
	__le16 dev_id_dbg;	 
	__le16 ramrom;		 
	__le16 dram_size2pci;	 
	__le16 rom_size2pci;	 
	u8 pad[2];		 
	u8 freetime;		 
	u8 ifctrl;		 
	__le16 dram_size;	 
	u8 mac[ETH_ALEN];	 
	u8 mac2[ETH_ALEN];
	u8 pad2[6];
	u16 dev_id2;		 
	u8 irqpin2;		 
	u8 class_code2[3];	 
	u16 cfg_byte6;		 
	u16 pme_cap;		 
	u16 nwclk_ctrl;		 
	u8 fru_format;		 
	u8 fru_assembly[6];	 
	u8 fru_rev[2];
	u8 fru_serial[14];
	u8 fru_pad[3];
	u8 oem_fru[28];		 
	u8 pad3[4];		 
};
struct slic_oasis_eeprom {
	__le16 id;		 
	__le16 eeprom_code_size; 
	__le16 spidev0_cfg;	 
	__le16 spidev1_cfg;	 
	__le16 vendor_id;	 
	__le16 dev_id;		 
	u8 rev_id;		 
	u8 class_code0[3];	 
	u8 irqpin1;		 
	u8 class_code1[3];	 
	u8 irqpin2;		 
	u8 irqpin0;		 
	u8 min_grant;		 
	u8 max_lat;		 
	__le16 sub_vendor_id;	 
	__le16 sub_id;		 
	__le16 flash_size;	 
	__le16 dram_size2pci;	 
	__le16 rom_size2pci;	 
	__le16 dev_id1;		 
	__le16 dev_id2;		 
	__le16 dev_stat_cfg;	 
	__le16 pme_cap;		 
	u8 msi_cap;		 
	u8 clock_div;		 
	__le16 pci_stat_lo;	 
	__le16 pci_stat_hi;	 
	__le16 dram_cfg_lo;	 
	__le16 dram_cfg_hi;	 
	__le16 dram_size;	 
	__le16 gpio_tbi_ctrl;	 
	__le16 eeprom_size;	 
	u8 mac[ETH_ALEN];	 
	u8 mac2[ETH_ALEN];
	u8 fru_format;		 
	u8 fru_assembly[6];	 
	u8 fru_rev[2];
	u8 fru_serial[14];
	u8 fru_pad[3];
	u8 oem_fru[28];		 
	u8 pad[4];		 
};
struct slic_stats {
	u64 rx_packets;
	u64 rx_bytes;
	u64 rx_mcasts;
	u64 rx_errors;
	u64 tx_packets;
	u64 tx_bytes;
	u64 rx_buff_miss;
	u64 tx_dropped;
	u64 irq_errs;
	u64 rx_tpcsum;
	u64 rx_tpoflow;
	u64 rx_tphlen;
	u64 rx_ipcsum;
	u64 rx_iplen;
	u64 rx_iphlen;
	u64 rx_early;
	u64 rx_buffoflow;
	u64 rx_lcode;
	u64 rx_drbl;
	u64 rx_crc;
	u64 rx_oflow802;
	u64 rx_uflow802;
	u64 tx_carrier;
	struct u64_stats_sync syncp;
};
struct slic_shmem_data {
	__le32 isr;
	__le32 link;
};
struct slic_shmem {
	dma_addr_t isr_paddr;
	dma_addr_t link_paddr;
	struct slic_shmem_data *shmem_data;
};
struct slic_rx_info_oasis {
	__le32 frame_status;
	__le32 frame_status_b;
	__le32 time_stamp;
	__le32 checksum;
};
struct slic_rx_info_mojave {
	__le32 frame_status;
	__le16 byte_cnt;
	__le16 tp_chksum;
	__le16 ctx_hash;
	__le16 mac_hash;
	__le16 buff_lnk;
};
struct slic_stat_desc {
	__le32 hnd;
	__u8 pad[8];
	__le32 status;
	__u8 pad2[16];
};
struct slic_stat_queue {
	struct slic_stat_desc *descs[SLIC_NUM_STAT_DESC_ARRAYS];
	dma_addr_t paddr[SLIC_NUM_STAT_DESC_ARRAYS];
	unsigned int addr_offset[SLIC_NUM_STAT_DESC_ARRAYS];
	unsigned int active_array;
	unsigned int len;
	unsigned int done_idx;
	size_t mem_size;
};
struct slic_tx_desc {
	__le32 hnd;
	__le32 rsvd;
	u8 cmd;
	u8 flags;
	__le16 rsvd2;
	__le32 totlen;
	__le32 paddrl;
	__le32 paddrh;
	__le32 len;
	__le32 type;
};
struct slic_tx_buffer {
	struct sk_buff *skb;
	DEFINE_DMA_UNMAP_ADDR(map_addr);
	DEFINE_DMA_UNMAP_LEN(map_len);
	struct slic_tx_desc *desc;
	dma_addr_t desc_paddr;
};
struct slic_tx_queue {
	struct dma_pool *dma_pool;
	struct slic_tx_buffer *txbuffs;
	unsigned int len;
	unsigned int put_idx;
	unsigned int done_idx;
};
struct slic_rx_desc {
	u8 pad[16];
	__le32 buffer;
	__le32 length;
	__le32 status;
};
struct slic_rx_buffer {
	struct sk_buff *skb;
	DEFINE_DMA_UNMAP_ADDR(map_addr);
	DEFINE_DMA_UNMAP_LEN(map_len);
	unsigned int addr_offset;
};
struct slic_rx_queue {
	struct slic_rx_buffer *rxbuffs;
	unsigned int len;
	unsigned int done_idx;
	unsigned int put_idx;
};
struct slic_device {
	struct pci_dev *pdev;
	struct net_device *netdev;
	void __iomem *regs;
	spinlock_t upper_lock;
	struct slic_shmem shmem;
	struct napi_struct napi;
	struct slic_rx_queue rxq;
	struct slic_tx_queue txq;
	struct slic_stat_queue stq;
	struct slic_stats stats;
	struct slic_upr_list upr_list;
	spinlock_t link_lock;
	bool promisc;
	int speed;
	unsigned int duplex;
	bool is_fiber;
	unsigned char model;
};
static inline u32 slic_read(struct slic_device *sdev, unsigned int reg)
{
	return ioread32(sdev->regs + reg);
}
static inline void slic_write(struct slic_device *sdev, unsigned int reg,
			      u32 val)
{
	iowrite32(val, sdev->regs + reg);
}
static inline void slic_flush_write(struct slic_device *sdev)
{
	(void)ioread32(sdev->regs + SLIC_REG_HOSTID);
}
#endif  
