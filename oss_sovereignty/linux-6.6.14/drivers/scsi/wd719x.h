 
#ifndef _WD719X_H_
#define _WD719X_H_

#define WD719X_SG 255		 

struct wd719x_sglist {
	__le32 ptr;
	__le32 length;
} __packed;

enum wd719x_card_type {
	WD719X_TYPE_UNKNOWN = 0,
	WD719X_TYPE_7193,
	WD719X_TYPE_7197,
	WD719X_TYPE_7296,
};

union wd719x_regs {
	__le32 all;	 
	struct {
		u8 OPC;		 
		u8 SCSI;	 
		u8 SUE;		 
		u8 INT;		 
	} bytes;
};

 
struct wd719x_scb {
	__le32 Int_SCB;	 
	u8 SCB_opcode;	 
	u8 CDB_tag;	 
	u8 lun;		 
	u8 devid;	 
	u8 CDB[16];	 
	__le32 data_p;	 
	__le32 data_length;  
	__le32 CDB_link;     
	__le32 sense_buf;    
	u8 sense_buf_length; 
	u8 reserved;	 
	u8 SCB_options;	 
	u8 SCB_tag_msg;	 
	 
	__le32 req_ptr;	 
	u8 host_opcode;	 
	u8 scsi_stat;	 
	u8 ret_error;	 
	u8 int_stat;	 
	__le32 transferred;  
	u8 last_trans[3];   
	u8 length;	 
	u8 sync_offset;	 
	u8 sync_rate;	 
	u8 flags[2];	 
	 
	dma_addr_t phys;	 
	dma_addr_t dma_handle;
	struct scsi_cmnd *cmd;	 
	struct list_head list;
	struct wd719x_sglist sg_list[WD719X_SG] __aligned(8);  
} __packed;

struct wd719x {
	struct Scsi_Host *sh;	 
	struct pci_dev *pdev;
	void __iomem *base;
	enum wd719x_card_type type;  
	void *fw_virt;		 
	dma_addr_t fw_phys;	 
	size_t fw_size;		 
	struct wd719x_host_param *params;  
	dma_addr_t params_phys;  
	void *hash_virt;	 
	dma_addr_t hash_phys;	 
	struct list_head active_scbs;
};

 
#define WD719X_WAIT_FOR_CMD_READY	500
#define WD719X_WAIT_FOR_RISC		2000
#define WD719X_WAIT_FOR_SCSI_RESET	3000000

 
#define WD719X_CMD_READY	0x00  
#define WD719X_CMD_INIT_RISC	0x01  
 
#define WD719X_CMD_BUSRESET	0x03  
#define WD719X_CMD_READ_FIRMVER	0x04  
#define WD719X_CMD_ECHO_BYTES	0x05  
 
 
#define WD719X_CMD_GET_PARAM	0x08  
#define WD719X_CMD_SET_PARAM	0x09  
#define WD719X_CMD_SLEEP	0x0a  
#define WD719X_CMD_READ_INIT	0x0b  
#define WD719X_CMD_RESTORE_INIT	0x0c  
 
 
 
#define WD719X_CMD_ABORT_TAG	0x10  
#define WD719X_CMD_ABORT	0x11  
#define WD719X_CMD_RESET	0x12  
#define WD719X_CMD_INIT_SCAM	0x13  
#define WD719X_CMD_GET_SYNC	0x14  
#define WD719X_CMD_SET_SYNC	0x15  
#define WD719X_CMD_GET_WIDTH	0x16  
#define WD719X_CMD_SET_WIDTH	0x17  
#define WD719X_CMD_GET_TAGS	0x18  
#define WD719X_CMD_SET_TAGS	0x19  
#define WD719X_CMD_GET_PARAM2	0x1a  
#define WD719X_CMD_SET_PARAM2	0x1b  
 
#define WD719X_CMD_PROCESS_SCB	0x80  
 

 
#define WD719X_INT_NONE		0x00  
#define WD719X_INT_NOERRORS	0x01  
#define WD719X_INT_LINKNOERRORS	0x02  
#define WD719X_INT_LINKNOSTATUS	0x03  
#define WD719X_INT_ERRORSLOGGED	0x04  
#define WD719X_INT_SPIDERFAILED	0x05  
#define WD719X_INT_BADINT	0x80  
#define WD719X_INT_PIOREADY	0xf0  

 
#define WD719X_SUE_NOERRORS	0x00  
#define WD719X_SUE_REJECTED	0x01  
#define WD719X_SUE_SCBQFULL	0x02  
 
#define WD719X_SUE_TERM		0x04  
#define WD719X_SUE_CHAN1PAR	0x05  
#define WD719X_SUE_CHAN1ABORT	0x06  
#define WD719X_SUE_CHAN23PAR	0x07  
#define WD719X_SUE_CHAN23ABORT	0x08  
#define WD719X_SUE_TIMEOUT	0x10  
#define WD719X_SUE_RESET	0x11  
#define WD719X_SUE_BUSERROR	0x12  
#define WD719X_SUE_WRONGWAY	0x13  
#define WD719X_SUE_BADPHASE	0x14  
#define WD719X_SUE_TOOLONG	0x15  
#define WD719X_SUE_BUSFREE	0x16  
#define WD719X_SUE_ARSDONE	0x17  
#define WD719X_SUE_IGNORED	0x18  
#define WD719X_SUE_WRONGTAGS	0x19  
#define WD719X_SUE_BADTAGS	0x1a  
#define WD719X_SUE_NOSCAMID	0x1b  

 
#define	WD719X_HASH_TABLE_SIZE	4096

 
 
#define WD719X_AMR_COMMAND		0x00
#define WD719X_AMR_CMD_PARAM		0x01
#define WD719X_AMR_CMD_PARAM_2		0x02
#define WD719X_AMR_CMD_PARAM_3		0x03
#define WD719X_AMR_SCB_IN		0x04

#define WD719X_AMR_BIOS_SHARE_INT	0x0f

#define WD719X_AMR_SCB_OUT		0x18
#define WD719X_AMR_OP_CODE		0x1c
#define WD719X_AMR_SCSI_STATUS		0x1d
#define WD719X_AMR_SCB_ERROR		0x1e
#define WD719X_AMR_INT_STATUS		0x1f

#define WD719X_DISABLE_INT	0x80

 
#define WD719X_SCB_FLAGS_CHECK_DIRECTION	0x01
#define WD719X_SCB_FLAGS_PCI_TO_SCSI		0x02
#define WD719X_SCB_FLAGS_AUTO_REQUEST_SENSE	0x10
#define WD719X_SCB_FLAGS_DO_SCATTER_GATHER	0x20
#define WD719X_SCB_FLAGS_NO_DISCONNECT		0x40

 
 
#define WD719X_PCI_GPIO_CONTROL		0x3C
#define WD719X_PCI_GPIO_DATA		0x3D
#define WD719X_PCI_PORT_RESET		0x3E
#define WD719X_PCI_MODE_SELECT		0x3F

#define WD719X_PCI_EXTERNAL_ADDR	0x60
#define WD719X_PCI_INTERNAL_ADDR	0x64
#define WD719X_PCI_DMA_TRANSFER_SIZE	0x66
#define WD719X_PCI_CHANNEL2_3CMD	0x68
#define WD719X_PCI_CHANNEL2_3STATUS	0x69

#define WD719X_GPIO_ID_BITS		0x0a
#define WD719X_PRAM_BASE_ADDR		0x00

 
#define WD719X_PCI_RESET		 0x01
#define WD719X_ENABLE_ADVANCE_MODE	 0x01

#define WD719X_START_CHANNEL2_3DMA	 0x17
#define WD719X_START_CHANNEL2_3DONE	 0x01
#define WD719X_START_CHANNEL2_3ABORT	 0x20

 
#define WD719X_EE_DI	(1 << 1)
#define WD719X_EE_CS	(1 << 2)
#define WD719X_EE_CLK	(1 << 3)
#define WD719X_EE_DO	(1 << 4)

 
struct wd719x_eeprom_header {
	u8 sig1;
	u8 sig2;
	u8 version;
	u8 checksum;
	u8 cfg_offset;
	u8 cfg_size;
	u8 setup_offset;
	u8 setup_size;
} __packed;

#define WD719X_EE_SIG1		0
#define WD719X_EE_SIG2		1
#define WD719X_EE_VERSION	2
#define WD719X_EE_CHECKSUM	3
#define WD719X_EE_CFG_OFFSET	4
#define WD719X_EE_CFG_SIZE	5
#define WD719X_EE_SETUP_OFFSET	6
#define WD719X_EE_SETUP_SIZE	7

#define WD719X_EE_SCSI_ID_MASK	0xf

 
struct wd719x_host_param {
	u8 ch_1_th;	 
	u8 scsi_conf;	 
	u8 own_scsi_id;	 
	u8 sel_timeout;	 
	u8 sleep_timer;	 
	__le16 cdb_size; 
	__le16 tag_en;	 
	u8 scsi_pad;	 
	__le32 wide;	 
	__le32 sync;	 
	u8 soft_mask;	 
	u8 unsol_mask;	 
} __packed;

#endif  
