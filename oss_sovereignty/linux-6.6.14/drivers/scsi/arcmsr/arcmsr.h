 
#include <linux/interrupt.h>
struct device_attribute;
 
#define ARCMSR_NAME			"arcmsr"
#define ARCMSR_MAX_FREECCB_NUM		1024
#define ARCMSR_MAX_OUTSTANDING_CMD	1024
#define ARCMSR_DEFAULT_OUTSTANDING_CMD	128
#define ARCMSR_MIN_OUTSTANDING_CMD	32
#define ARCMSR_DRIVER_VERSION		"v1.50.00.13-20230206"
#define ARCMSR_SCSI_INITIATOR_ID	255
#define ARCMSR_MAX_XFER_SECTORS		512
#define ARCMSR_MAX_XFER_SECTORS_B	4096
#define ARCMSR_MAX_XFER_SECTORS_C	304
#define ARCMSR_MAX_TARGETID		17
#define ARCMSR_MAX_TARGETLUN		8
#define ARCMSR_MAX_CMD_PERLUN		128
#define ARCMSR_DEFAULT_CMD_PERLUN	32
#define ARCMSR_MIN_CMD_PERLUN		1
#define ARCMSR_MAX_QBUFFER		4096
#define ARCMSR_DEFAULT_SG_ENTRIES	38
#define ARCMSR_MAX_HBB_POSTQUEUE	264
#define ARCMSR_MAX_ARC1214_POSTQUEUE	256
#define ARCMSR_MAX_ARC1214_DONEQUEUE	257
#define ARCMSR_MAX_HBE_DONEQUEUE	512
#define ARCMSR_MAX_XFER_LEN		0x26000  
#define ARCMSR_CDB_SG_PAGE_LENGTH	256
#define ARCMST_NUM_MSIX_VECTORS		4
#ifndef PCI_DEVICE_ID_ARECA_1880
#define PCI_DEVICE_ID_ARECA_1880	0x1880
#endif
#ifndef PCI_DEVICE_ID_ARECA_1214
#define PCI_DEVICE_ID_ARECA_1214	0x1214
#endif
#ifndef PCI_DEVICE_ID_ARECA_1203
#define PCI_DEVICE_ID_ARECA_1203	0x1203
#endif
#ifndef PCI_DEVICE_ID_ARECA_1884
#define PCI_DEVICE_ID_ARECA_1884	0x1884
#endif
#define PCI_DEVICE_ID_ARECA_1886	0x188A
#define	ARCMSR_HOURS			(1000 * 60 * 60 * 4)
#define	ARCMSR_MINUTES			(1000 * 60 * 60)
#define ARCMSR_DEFAULT_TIMEOUT		90
 
#define ARC_SUCCESS	0
#define ARC_FAILURE	1
 
#define dma_addr_hi32(addr)	(uint32_t) ((addr>>16)>>16)
#define dma_addr_lo32(addr)	(uint32_t) (addr & 0xffffffff)
 
struct CMD_MESSAGE
{
      uint32_t HeaderLength;
      uint8_t  Signature[8];
      uint32_t Timeout;
      uint32_t ControlCode;
      uint32_t ReturnCode;
      uint32_t Length;
};
 
#define	ARCMSR_API_DATA_BUFLEN	1032
struct CMD_MESSAGE_FIELD
{
    struct CMD_MESSAGE			cmdmessage;
    uint8_t				messagedatabuffer[ARCMSR_API_DATA_BUFLEN];
};
 
#define ARCMSR_MESSAGE_FAIL			0x0001
 
#define ARECA_SATA_RAID				0x90000000
 
#define FUNCTION_READ_RQBUFFER			0x0801
#define FUNCTION_WRITE_WQBUFFER			0x0802
#define FUNCTION_CLEAR_RQBUFFER			0x0803
#define FUNCTION_CLEAR_WQBUFFER			0x0804
#define FUNCTION_CLEAR_ALLQBUFFER		0x0805
#define FUNCTION_RETURN_CODE_3F			0x0806
#define FUNCTION_SAY_HELLO			0x0807
#define FUNCTION_SAY_GOODBYE			0x0808
#define FUNCTION_FLUSH_ADAPTER_CACHE		0x0809
#define FUNCTION_GET_FIRMWARE_STATUS		0x080A
#define FUNCTION_HARDWARE_RESET			0x080B
 
#define ARCMSR_MESSAGE_READ_RQBUFFER       \
	ARECA_SATA_RAID | FUNCTION_READ_RQBUFFER
#define ARCMSR_MESSAGE_WRITE_WQBUFFER      \
	ARECA_SATA_RAID | FUNCTION_WRITE_WQBUFFER
#define ARCMSR_MESSAGE_CLEAR_RQBUFFER      \
	ARECA_SATA_RAID | FUNCTION_CLEAR_RQBUFFER
#define ARCMSR_MESSAGE_CLEAR_WQBUFFER      \
	ARECA_SATA_RAID | FUNCTION_CLEAR_WQBUFFER
#define ARCMSR_MESSAGE_CLEAR_ALLQBUFFER    \
	ARECA_SATA_RAID | FUNCTION_CLEAR_ALLQBUFFER
#define ARCMSR_MESSAGE_RETURN_CODE_3F      \
	ARECA_SATA_RAID | FUNCTION_RETURN_CODE_3F
#define ARCMSR_MESSAGE_SAY_HELLO           \
	ARECA_SATA_RAID | FUNCTION_SAY_HELLO
#define ARCMSR_MESSAGE_SAY_GOODBYE         \
	ARECA_SATA_RAID | FUNCTION_SAY_GOODBYE
#define ARCMSR_MESSAGE_FLUSH_ADAPTER_CACHE \
	ARECA_SATA_RAID | FUNCTION_FLUSH_ADAPTER_CACHE
 
#define ARCMSR_MESSAGE_RETURNCODE_OK		0x00000001
#define ARCMSR_MESSAGE_RETURNCODE_ERROR		0x00000006
#define ARCMSR_MESSAGE_RETURNCODE_3F		0x0000003F
#define ARCMSR_MESSAGE_RETURNCODE_BUS_HANG_ON	0x00000088
 
#define IS_DMA64	(sizeof(dma_addr_t) == 8)
#define IS_SG64_ADDR	0x01000000  
struct  SG32ENTRY
{
	__le32		length;
	__le32		address;
}__attribute__ ((packed));
struct  SG64ENTRY
{
	__le32		length;
	__le32		address;
	__le32		addresshigh;
}__attribute__ ((packed));
 
struct QBUFFER
{
	uint32_t      data_len;
	uint8_t       data[124];
};
 
struct FIRMWARE_INFO
{
	uint32_t	signature;		 
	uint32_t	request_len;		 
	uint32_t	numbers_queue;		 
	uint32_t	sdram_size;		 
	uint32_t	ide_channels;		 
	char		vendor[40];		 
	char		model[8];		 
	char		firmware_ver[16];     	 
	char		device_map[16];		 
	uint32_t	cfgVersion;		 
	uint8_t		cfgSerial[16];		 
	uint32_t	cfgPicStatus;		 
};
 
#define ARCMSR_SIGNATURE_GET_CONFIG		0x87974060
#define ARCMSR_SIGNATURE_SET_CONFIG		0x87974063
 
#define ARCMSR_INBOUND_MESG0_NOP		0x00000000
#define ARCMSR_INBOUND_MESG0_GET_CONFIG		0x00000001
#define ARCMSR_INBOUND_MESG0_SET_CONFIG		0x00000002
#define ARCMSR_INBOUND_MESG0_ABORT_CMD		0x00000003
#define ARCMSR_INBOUND_MESG0_STOP_BGRB		0x00000004
#define ARCMSR_INBOUND_MESG0_FLUSH_CACHE	0x00000005
#define ARCMSR_INBOUND_MESG0_START_BGRB		0x00000006
#define ARCMSR_INBOUND_MESG0_CHK331PENDING	0x00000007
#define ARCMSR_INBOUND_MESG0_SYNC_TIMER		0x00000008
 
#define ARCMSR_INBOUND_DRIVER_DATA_WRITE_OK	0x00000001
#define ARCMSR_INBOUND_DRIVER_DATA_READ_OK	0x00000002
#define ARCMSR_OUTBOUND_IOP331_DATA_WRITE_OK	0x00000001
#define ARCMSR_OUTBOUND_IOP331_DATA_READ_OK	0x00000002
 
#define ARCMSR_CCBPOST_FLAG_SGL_BSIZE		0x80000000
#define ARCMSR_CCBPOST_FLAG_IAM_BIOS		0x40000000
#define ARCMSR_CCBREPLY_FLAG_IAM_BIOS		0x40000000
#define ARCMSR_CCBREPLY_FLAG_ERROR_MODE0	0x10000000
#define ARCMSR_CCBREPLY_FLAG_ERROR_MODE1	0x00000001
 
#define ARCMSR_OUTBOUND_MESG1_FIRMWARE_OK	0x80000000
 
#define ARCMSR_ARC1680_BUS_RESET		0x00000003
 
#define ARCMSR_ARC1880_RESET_ADAPTER		0x00000024
#define ARCMSR_ARC1880_DiagWrite_ENABLE		0x00000080

 
 
 
#define ARCMSR_DRV2IOP_DOORBELL                       0x00020400
#define ARCMSR_DRV2IOP_DOORBELL_MASK                  0x00020404
 
#define ARCMSR_IOP2DRV_DOORBELL                       0x00020408
#define ARCMSR_IOP2DRV_DOORBELL_MASK                  0x0002040C
 
#define ARCMSR_IOP2DRV_DOORBELL_1203                  0x00021870
#define ARCMSR_IOP2DRV_DOORBELL_MASK_1203             0x00021874
 
#define ARCMSR_DRV2IOP_DOORBELL_1203                  0x00021878
#define ARCMSR_DRV2IOP_DOORBELL_MASK_1203             0x0002187C
 
 
#define ARCMSR_IOP2DRV_DATA_WRITE_OK                  0x00000001
 
#define ARCMSR_IOP2DRV_DATA_READ_OK                   0x00000002
#define ARCMSR_IOP2DRV_CDB_DONE                       0x00000004
#define ARCMSR_IOP2DRV_MESSAGE_CMD_DONE               0x00000008

#define ARCMSR_DOORBELL_HANDLE_INT		      0x0000000F
#define ARCMSR_DOORBELL_INT_CLEAR_PATTERN   	      0xFF00FFF0
#define ARCMSR_MESSAGE_INT_CLEAR_PATTERN	      0xFF00FFF7
 
#define ARCMSR_MESSAGE_GET_CONFIG		      0x00010008
 
#define ARCMSR_MESSAGE_SET_CONFIG		      0x00020008
 
#define ARCMSR_MESSAGE_ABORT_CMD		      0x00030008
 
#define ARCMSR_MESSAGE_STOP_BGRB		      0x00040008
 
#define ARCMSR_MESSAGE_FLUSH_CACHE                    0x00050008
 
#define ARCMSR_MESSAGE_START_BGRB		      0x00060008
#define ARCMSR_MESSAGE_SYNC_TIMER		      0x00080008
#define ARCMSR_MESSAGE_START_DRIVER_MODE	      0x000E0008
#define ARCMSR_MESSAGE_SET_POST_WINDOW		      0x000F0008
#define ARCMSR_MESSAGE_ACTIVE_EOI_MODE		      0x00100008
 
#define ARCMSR_MESSAGE_FIRMWARE_OK		      0x80000000
 
#define ARCMSR_DRV2IOP_DATA_WRITE_OK                  0x00000001
 
#define ARCMSR_DRV2IOP_DATA_READ_OK                   0x00000002
#define ARCMSR_DRV2IOP_CDB_POSTED                     0x00000004
#define ARCMSR_DRV2IOP_MESSAGE_CMD_POSTED             0x00000008
#define ARCMSR_DRV2IOP_END_OF_INTERRUPT	              0x00000010

 
 
#define ARCMSR_MESSAGE_WBUFFER			      0x0000fe00
 
#define ARCMSR_MESSAGE_RBUFFER			      0x0000ff00
 
#define ARCMSR_MESSAGE_RWBUFFER			      0x0000fa00

#define MEM_BASE0(x)	(u32 __iomem *)((unsigned long)acb->mem_base0 + x)
#define MEM_BASE1(x)	(u32 __iomem *)((unsigned long)acb->mem_base1 + x)
 
#define ARCMSR_HBC_ISR_THROTTLING_LEVEL		12
#define ARCMSR_HBC_ISR_MAX_DONE_QUEUE		20
 
#define ARCMSR_HBCMU_UTILITY_A_ISR_MASK		0x00000001  
#define ARCMSR_HBCMU_OUTBOUND_DOORBELL_ISR_MASK	0x00000004  
#define ARCMSR_HBCMU_OUTBOUND_POSTQUEUE_ISR_MASK	0x00000008  
#define ARCMSR_HBCMU_ALL_INTMASKENABLE		0x0000000D  
 
#define ARCMSR_HBCMU_UTILITY_A_ISR		0x00000001
	 
#define ARCMSR_HBCMU_OUTBOUND_DOORBELL_ISR	0x00000004
	 
#define ARCMSR_HBCMU_OUTBOUND_POSTQUEUE_ISR	0x00000008
	 
#define ARCMSR_HBCMU_SAS_ALL_INT		0x00000010
	 
	 
#define ARCMSR_HBCMU_DRV2IOP_DATA_WRITE_OK			0x00000002
#define ARCMSR_HBCMU_DRV2IOP_DATA_READ_OK			0x00000004
	 
#define ARCMSR_HBCMU_DRV2IOP_MESSAGE_CMD_DONE			0x00000008
	 
#define ARCMSR_HBCMU_DRV2IOP_POSTQUEUE_THROTTLING		0x00000010
#define ARCMSR_HBCMU_IOP2DRV_DATA_WRITE_OK			0x00000002
	 
#define ARCMSR_HBCMU_IOP2DRV_DATA_WRITE_DOORBELL_CLEAR		0x00000002
#define ARCMSR_HBCMU_IOP2DRV_DATA_READ_OK			0x00000004
	 
#define ARCMSR_HBCMU_IOP2DRV_DATA_READ_DOORBELL_CLEAR		0x00000004
	 
#define ARCMSR_HBCMU_IOP2DRV_MESSAGE_CMD_DONE			0x00000008
	 
#define ARCMSR_HBCMU_IOP2DRV_MESSAGE_CMD_DONE_DOORBELL_CLEAR	0x00000008
	 
#define ARCMSR_HBCMU_MESSAGE_FIRMWARE_OK			0x80000000
 
#define ARCMSR_ARC1214_CHIP_ID				0x00004
#define ARCMSR_ARC1214_CPU_MEMORY_CONFIGURATION		0x00008
#define ARCMSR_ARC1214_I2_HOST_INTERRUPT_MASK		0x00034
#define ARCMSR_ARC1214_SAMPLE_RESET			0x00100
#define ARCMSR_ARC1214_RESET_REQUEST			0x00108
#define ARCMSR_ARC1214_MAIN_INTERRUPT_STATUS		0x00200
#define ARCMSR_ARC1214_PCIE_F0_INTERRUPT_ENABLE		0x0020C
#define ARCMSR_ARC1214_INBOUND_MESSAGE0			0x00400
#define ARCMSR_ARC1214_INBOUND_MESSAGE1			0x00404
#define ARCMSR_ARC1214_OUTBOUND_MESSAGE0		0x00420
#define ARCMSR_ARC1214_OUTBOUND_MESSAGE1		0x00424
#define ARCMSR_ARC1214_INBOUND_DOORBELL			0x00460
#define ARCMSR_ARC1214_OUTBOUND_DOORBELL		0x00480
#define ARCMSR_ARC1214_OUTBOUND_DOORBELL_ENABLE		0x00484
#define ARCMSR_ARC1214_INBOUND_LIST_BASE_LOW		0x01000
#define ARCMSR_ARC1214_INBOUND_LIST_BASE_HIGH		0x01004
#define ARCMSR_ARC1214_INBOUND_LIST_WRITE_POINTER	0x01018
#define ARCMSR_ARC1214_OUTBOUND_LIST_BASE_LOW		0x01060
#define ARCMSR_ARC1214_OUTBOUND_LIST_BASE_HIGH		0x01064
#define ARCMSR_ARC1214_OUTBOUND_LIST_COPY_POINTER	0x0106C
#define ARCMSR_ARC1214_OUTBOUND_LIST_READ_POINTER	0x01070
#define ARCMSR_ARC1214_OUTBOUND_INTERRUPT_CAUSE		0x01088
#define ARCMSR_ARC1214_OUTBOUND_INTERRUPT_ENABLE	0x0108C
#define ARCMSR_ARC1214_MESSAGE_WBUFFER			0x02000
#define ARCMSR_ARC1214_MESSAGE_RBUFFER			0x02100
#define ARCMSR_ARC1214_MESSAGE_RWBUFFER			0x02200
 
#define ARCMSR_ARC1214_ALL_INT_ENABLE			0x00001010
#define ARCMSR_ARC1214_ALL_INT_DISABLE			0x00000000
 
#define ARCMSR_ARC1214_OUTBOUND_DOORBELL_ISR		0x00001000
#define ARCMSR_ARC1214_OUTBOUND_POSTQUEUE_ISR		0x00000010
 
#define ARCMSR_ARC1214_DRV2IOP_DATA_IN_READY		0x00000001
#define ARCMSR_ARC1214_DRV2IOP_DATA_OUT_READ		0x00000002
 
#define ARCMSR_ARC1214_IOP2DRV_DATA_WRITE_OK		0x00000001
 
#define ARCMSR_ARC1214_IOP2DRV_DATA_READ_OK		0x00000002
 
#define ARCMSR_ARC1214_IOP2DRV_MESSAGE_CMD_DONE		0x02000000
 
 
#define ARCMSR_ARC1214_MESSAGE_FIRMWARE_OK		0x80000000
#define ARCMSR_ARC1214_OUTBOUND_LIST_INTERRUPT_CLEAR	0x00000001
 
#define ARCMSR_SIGNATURE_1884			0x188417D3

#define ARCMSR_HBEMU_DRV2IOP_DATA_WRITE_OK	0x00000002
#define ARCMSR_HBEMU_DRV2IOP_DATA_READ_OK	0x00000004
#define ARCMSR_HBEMU_DRV2IOP_MESSAGE_CMD_DONE	0x00000008

#define ARCMSR_HBEMU_IOP2DRV_DATA_WRITE_OK	0x00000002
#define ARCMSR_HBEMU_IOP2DRV_DATA_READ_OK	0x00000004
#define ARCMSR_HBEMU_IOP2DRV_MESSAGE_CMD_DONE	0x00000008

#define ARCMSR_HBEMU_MESSAGE_FIRMWARE_OK	0x80000000

#define ARCMSR_HBEMU_OUTBOUND_DOORBELL_ISR	0x00000001
#define ARCMSR_HBEMU_OUTBOUND_POSTQUEUE_ISR	0x00000008
#define ARCMSR_HBEMU_ALL_INTMASKENABLE		0x00000009

 
#define ARCMSR_HBEMU_DOORBELL_SYNC		0x100
#define ARCMSR_ARC188X_RESET_ADAPTER		0x00000004
#define ARCMSR_ARC1884_DiagWrite_ENABLE		0x00000080

 
#define ARCMSR_SIGNATURE_1886			0x188617D3

 
#define ARCMSR_HBFMU_DOORBELL_SYNC		0x100

#define ARCMSR_HBFMU_DOORBELL_SYNC1		0x300
#define ARCMSR_HBFMU_MESSAGE_FIRMWARE_OK	0x80000000
#define ARCMSR_HBFMU_MESSAGE_NO_VOLUME_CHANGE	0x20000000

 
struct ARCMSR_CDB
{
	uint8_t		Bus;
	uint8_t		TargetID;
	uint8_t		LUN;
	uint8_t		Function;
	uint8_t		CdbLength;
	uint8_t		sgcount;
	uint8_t		Flags;
#define ARCMSR_CDB_FLAG_SGL_BSIZE          0x01
#define ARCMSR_CDB_FLAG_BIOS               0x02
#define ARCMSR_CDB_FLAG_WRITE              0x04
#define ARCMSR_CDB_FLAG_SIMPLEQ            0x00
#define ARCMSR_CDB_FLAG_HEADQ              0x08
#define ARCMSR_CDB_FLAG_ORDEREDQ           0x10

	uint8_t		msgPages;
	uint32_t	msgContext;
	uint32_t	DataLength;
	uint8_t		Cdb[16];
	uint8_t		DeviceStatus;
#define ARCMSR_DEV_CHECK_CONDITION	    0x02
#define ARCMSR_DEV_SELECT_TIMEOUT	    0xF0
#define ARCMSR_DEV_ABORTED		    0xF1
#define ARCMSR_DEV_INIT_FAIL		    0xF2

	uint8_t		SenseData[15];
	union
	{
		struct SG32ENTRY	sg32entry[1];
		struct SG64ENTRY	sg64entry[1];
	} u;
};
 
struct MessageUnit_A
{
	uint32_t	resrved0[4];			 
	uint32_t	inbound_msgaddr0;		 
	uint32_t	inbound_msgaddr1;		 
	uint32_t	outbound_msgaddr0;		 
	uint32_t	outbound_msgaddr1;		 
	uint32_t	inbound_doorbell;		 
	uint32_t	inbound_intstatus;		 
	uint32_t	inbound_intmask;		 
	uint32_t	outbound_doorbell;		 
	uint32_t	outbound_intstatus;		 
	uint32_t	outbound_intmask;		 
	uint32_t	reserved1[2];			 
	uint32_t	inbound_queueport;		 
	uint32_t	outbound_queueport;     	 
	uint32_t	reserved2[2];			 
	uint32_t	reserved3[492];			 
	uint32_t	reserved4[128];			 
	uint32_t	message_rwbuffer[256];		 
	uint32_t	message_wbuffer[32];		 
	uint32_t	reserved5[32];			 
	uint32_t	message_rbuffer[32];		 
	uint32_t	reserved6[32];			 
};

struct MessageUnit_B
{
	uint32_t	post_qbuffer[ARCMSR_MAX_HBB_POSTQUEUE];
	uint32_t	done_qbuffer[ARCMSR_MAX_HBB_POSTQUEUE];
	uint32_t	postq_index;
	uint32_t	doneq_index;
	uint32_t	__iomem *drv2iop_doorbell;
	uint32_t	__iomem *drv2iop_doorbell_mask;
	uint32_t	__iomem *iop2drv_doorbell;
	uint32_t	__iomem *iop2drv_doorbell_mask;
	uint32_t	__iomem *message_rwbuffer;
	uint32_t	__iomem *message_wbuffer;
	uint32_t	__iomem *message_rbuffer;
};
 
struct MessageUnit_C{
	uint32_t	message_unit_status;			 
	uint32_t	slave_error_attribute;			 
	uint32_t	slave_error_address;			 
	uint32_t	posted_outbound_doorbell;		 
	uint32_t	master_error_attribute;			 
	uint32_t	master_error_address_low;		 
	uint32_t	master_error_address_high;		 
	uint32_t	hcb_size;				 
	uint32_t	inbound_doorbell;			 
	uint32_t	diagnostic_rw_data;			 
	uint32_t	diagnostic_rw_address_low;		 
	uint32_t	diagnostic_rw_address_high;		 
	uint32_t	host_int_status;			 
	uint32_t	host_int_mask;				 
	uint32_t	dcr_data;				 
	uint32_t	dcr_address;				 
	uint32_t	inbound_queueport;			 
	uint32_t	outbound_queueport;			 
	uint32_t	hcb_pci_address_low;			 
	uint32_t	hcb_pci_address_high;			 
	uint32_t	iop_int_status;				 
	uint32_t	iop_int_mask;				 
	uint32_t	iop_inbound_queue_port;			 
	uint32_t	iop_outbound_queue_port;		 
	uint32_t	inbound_free_list_index;		 
	uint32_t	inbound_post_list_index;		 
	uint32_t	outbound_free_list_index;		 
	uint32_t	outbound_post_list_index;		 
	uint32_t	inbound_doorbell_clear;			 
	uint32_t	i2o_message_unit_control;		 
	uint32_t	last_used_message_source_address_low;	 
	uint32_t	last_used_message_source_address_high;	 
	uint32_t	pull_mode_data_byte_count[4];		 
	uint32_t	message_dest_address_index;		 
	uint32_t	done_queue_not_empty_int_counter_timer;	 
	uint32_t	utility_A_int_counter_timer;		 
	uint32_t	outbound_doorbell;			 
	uint32_t	outbound_doorbell_clear;		 
	uint32_t	message_source_address_index;		 
	uint32_t	message_done_queue_index;		 
	uint32_t	reserved0;				 
	uint32_t	inbound_msgaddr0;			 
	uint32_t	inbound_msgaddr1;			 
	uint32_t	outbound_msgaddr0;			 
	uint32_t	outbound_msgaddr1;			 
	uint32_t	inbound_queueport_low;			 
	uint32_t	inbound_queueport_high;			 
	uint32_t	outbound_queueport_low;			 
	uint32_t	outbound_queueport_high;		 
	uint32_t	iop_inbound_queue_port_low;		 
	uint32_t	iop_inbound_queue_port_high;		 
	uint32_t	iop_outbound_queue_port_low;		 
	uint32_t	iop_outbound_queue_port_high;		 
	uint32_t	message_dest_queue_port_low;		 
	uint32_t	message_dest_queue_port_high;		 
	uint32_t	last_used_message_dest_address_low;	 
	uint32_t	last_used_message_dest_address_high;	 
	uint32_t	message_done_queue_base_address_low;	 
	uint32_t	message_done_queue_base_address_high;	 
	uint32_t	host_diagnostic;			 
	uint32_t	write_sequence;				 
	uint32_t	reserved1[34];				 
	uint32_t	reserved2[1950];			 
	uint32_t	message_wbuffer[32];			 
	uint32_t	reserved3[32];				 
	uint32_t	message_rbuffer[32];			 
	uint32_t	reserved4[32];				 
	uint32_t	msgcode_rwbuffer[256];			 
};
 
struct InBound_SRB {
	uint32_t addressLow;  
	uint32_t addressHigh;
	uint32_t length;  
	uint32_t reserved0;
};

struct OutBound_SRB {
	uint32_t addressLow;  
	uint32_t addressHigh;
};

struct MessageUnit_D {
	struct InBound_SRB	post_qbuffer[ARCMSR_MAX_ARC1214_POSTQUEUE];
	volatile struct OutBound_SRB
				done_qbuffer[ARCMSR_MAX_ARC1214_DONEQUEUE];
	u16 postq_index;
	volatile u16 doneq_index;
	u32 __iomem *chip_id;			 
	u32 __iomem *cpu_mem_config;		 
	u32 __iomem *i2o_host_interrupt_mask;	 
	u32 __iomem *sample_at_reset;		 
	u32 __iomem *reset_request;		 
	u32 __iomem *host_int_status;		 
	u32 __iomem *pcief0_int_enable;		 
	u32 __iomem *inbound_msgaddr0;		 
	u32 __iomem *inbound_msgaddr1;		 
	u32 __iomem *outbound_msgaddr0;		 
	u32 __iomem *outbound_msgaddr1;		 
	u32 __iomem *inbound_doorbell;		 
	u32 __iomem *outbound_doorbell;		 
	u32 __iomem *outbound_doorbell_enable;	 
	u32 __iomem *inboundlist_base_low;	 
	u32 __iomem *inboundlist_base_high;	 
	u32 __iomem *inboundlist_write_pointer;	 
	u32 __iomem *outboundlist_base_low;	 
	u32 __iomem *outboundlist_base_high;	 
	u32 __iomem *outboundlist_copy_pointer;	 
	u32 __iomem *outboundlist_read_pointer;	 
	u32 __iomem *outboundlist_interrupt_cause;	 
	u32 __iomem *outboundlist_interrupt_enable;	 
	u32 __iomem *message_wbuffer;		 
	u32 __iomem *message_rbuffer;		 
	u32 __iomem *msgcode_rwbuffer;		 
};
 
struct MessageUnit_E{
	uint32_t	iobound_doorbell;			 
	uint32_t	write_sequence_3xxx;			 
	uint32_t	host_diagnostic_3xxx;			 
	uint32_t	posted_outbound_doorbell;		 
	uint32_t	master_error_attribute;			 
	uint32_t	master_error_address_low;		 
	uint32_t	master_error_address_high;		 
	uint32_t	hcb_size;				 
	uint32_t	inbound_doorbell;			 
	uint32_t	diagnostic_rw_data;			 
	uint32_t	diagnostic_rw_address_low;		 
	uint32_t	diagnostic_rw_address_high;		 
	uint32_t	host_int_status;			 
	uint32_t	host_int_mask;				 
	uint32_t	dcr_data;				 
	uint32_t	dcr_address;				 
	uint32_t	inbound_queueport;			 
	uint32_t	outbound_queueport;			 
	uint32_t	hcb_pci_address_low;			 
	uint32_t	hcb_pci_address_high;			 
	uint32_t	iop_int_status;				 
	uint32_t	iop_int_mask;				 
	uint32_t	iop_inbound_queue_port;			 
	uint32_t	iop_outbound_queue_port;		 
	uint32_t	inbound_free_list_index;		 
	uint32_t	inbound_post_list_index;		 
	uint32_t	reply_post_producer_index;		 
	uint32_t	reply_post_consumer_index;		 
	uint32_t	inbound_doorbell_clear;			 
	uint32_t	i2o_message_unit_control;		 
	uint32_t	last_used_message_source_address_low;	 
	uint32_t	last_used_message_source_address_high;	 
	uint32_t	pull_mode_data_byte_count[4];		 
	uint32_t	message_dest_address_index;		 
	uint32_t	done_queue_not_empty_int_counter_timer;	 
	uint32_t	utility_A_int_counter_timer;		 
	uint32_t	outbound_doorbell;			 
	uint32_t	outbound_doorbell_clear;		 
	uint32_t	message_source_address_index;		 
	uint32_t	message_done_queue_index;		 
	uint32_t	reserved0;				 
	uint32_t	inbound_msgaddr0;			 
	uint32_t	inbound_msgaddr1;			 
	uint32_t	outbound_msgaddr0;			 
	uint32_t	outbound_msgaddr1;			 
	uint32_t	inbound_queueport_low;			 
	uint32_t	inbound_queueport_high;			 
	uint32_t	outbound_queueport_low;			 
	uint32_t	outbound_queueport_high;		 
	uint32_t	iop_inbound_queue_port_low;		 
	uint32_t	iop_inbound_queue_port_high;		 
	uint32_t	iop_outbound_queue_port_low;		 
	uint32_t	iop_outbound_queue_port_high;		 
	uint32_t	message_dest_queue_port_low;		 
	uint32_t	message_dest_queue_port_high;		 
	uint32_t	last_used_message_dest_address_low;	 
	uint32_t	last_used_message_dest_address_high;	 
	uint32_t	message_done_queue_base_address_low;	 
	uint32_t	message_done_queue_base_address_high;	 
	uint32_t	host_diagnostic;			 
	uint32_t	write_sequence;				 
	uint32_t	reserved1[34];				 
	uint32_t	reserved2[1950];			 
	uint32_t	message_wbuffer[32];			 
	uint32_t	reserved3[32];				 
	uint32_t	message_rbuffer[32];			 
	uint32_t	reserved4[32];				 
	uint32_t	msgcode_rwbuffer[256];			 
};

 
struct MessageUnit_F {
	uint32_t	iobound_doorbell;			 
	uint32_t	write_sequence_3xxx;			 
	uint32_t	host_diagnostic_3xxx;			 
	uint32_t	posted_outbound_doorbell;		 
	uint32_t	master_error_attribute;			 
	uint32_t	master_error_address_low;		 
	uint32_t	master_error_address_high;		 
	uint32_t	hcb_size;				 
	uint32_t	inbound_doorbell;			 
	uint32_t	diagnostic_rw_data;			 
	uint32_t	diagnostic_rw_address_low;		 
	uint32_t	diagnostic_rw_address_high;		 
	uint32_t	host_int_status;			 
	uint32_t	host_int_mask;				 
	uint32_t	dcr_data;				 
	uint32_t	dcr_address;				 
	uint32_t	inbound_queueport;			 
	uint32_t	outbound_queueport;			 
	uint32_t	hcb_pci_address_low;			 
	uint32_t	hcb_pci_address_high;			 
	uint32_t	iop_int_status;				 
	uint32_t	iop_int_mask;				 
	uint32_t	iop_inbound_queue_port;			 
	uint32_t	iop_outbound_queue_port;		 
	uint32_t	inbound_free_list_index;		 
	uint32_t	inbound_post_list_index;		 
	uint32_t	reply_post_producer_index;		 
	uint32_t	reply_post_consumer_index;		 
	uint32_t	inbound_doorbell_clear;			 
	uint32_t	i2o_message_unit_control;		 
	uint32_t	last_used_message_source_address_low;	 
	uint32_t	last_used_message_source_address_high;	 
	uint32_t	pull_mode_data_byte_count[4];		 
	uint32_t	message_dest_address_index;		 
	uint32_t	done_queue_not_empty_int_counter_timer;	 
	uint32_t	utility_A_int_counter_timer;		 
	uint32_t	outbound_doorbell;			 
	uint32_t	outbound_doorbell_clear;		 
	uint32_t	message_source_address_index;		 
	uint32_t	message_done_queue_index;		 
	uint32_t	reserved0;				 
	uint32_t	inbound_msgaddr0;			 
	uint32_t	inbound_msgaddr1;			 
	uint32_t	outbound_msgaddr0;			 
	uint32_t	outbound_msgaddr1;			 
	uint32_t	inbound_queueport_low;			 
	uint32_t	inbound_queueport_high;			 
	uint32_t	outbound_queueport_low;			 
	uint32_t	outbound_queueport_high;		 
	uint32_t	iop_inbound_queue_port_low;		 
	uint32_t	iop_inbound_queue_port_high;		 
	uint32_t	iop_outbound_queue_port_low;		 
	uint32_t	iop_outbound_queue_port_high;		 
	uint32_t	message_dest_queue_port_low;		 
	uint32_t	message_dest_queue_port_high;		 
	uint32_t	last_used_message_dest_address_low;	 
	uint32_t	last_used_message_dest_address_high;	 
	uint32_t	message_done_queue_base_address_low;	 
	uint32_t	message_done_queue_base_address_high;	 
	uint32_t	host_diagnostic;			 
	uint32_t	write_sequence;				 
	uint32_t	reserved1[46];				 
	uint32_t	reply_post_producer_index1;		 
	uint32_t	reply_post_consumer_index1;		 
};

#define	MESG_RW_BUFFER_SIZE	(256 * 3)

typedef struct deliver_completeQ {
	uint16_t	cmdFlag;
	uint16_t	cmdSMID;
	uint16_t	cmdLMID;        
	uint16_t	cmdFlag2;       
} DeliverQ, CompletionQ, *pDeliver_Q, *pCompletion_Q;
 
struct AdapterControlBlock
{
	uint32_t		adapter_type;		 
#define ACB_ADAPTER_TYPE_A		0x00000000	 
#define ACB_ADAPTER_TYPE_B		0x00000001	 
#define ACB_ADAPTER_TYPE_C		0x00000002	 
#define ACB_ADAPTER_TYPE_D		0x00000003	 
#define ACB_ADAPTER_TYPE_E		0x00000004	 
#define ACB_ADAPTER_TYPE_F		0x00000005	 
	u32			ioqueue_size;
	struct pci_dev *	pdev;
	struct Scsi_Host *	host;
	unsigned long		vir2phy_offset;
	 
	uint32_t		outbound_int_enable;
	uint32_t		cdb_phyaddr_hi32;
	uint32_t		reg_mu_acc_handle0;
	uint64_t		cdb_phyadd_hipart;
	spinlock_t		eh_lock;
	spinlock_t		ccblist_lock;
	spinlock_t		postq_lock;
	spinlock_t		doneq_lock;
	spinlock_t		rqbuffer_lock;
	spinlock_t		wqbuffer_lock;
	union {
		struct MessageUnit_A __iomem *pmuA;
		struct MessageUnit_B 	*pmuB;
		struct MessageUnit_C __iomem *pmuC;
		struct MessageUnit_D 	*pmuD;
		struct MessageUnit_E __iomem *pmuE;
		struct MessageUnit_F __iomem *pmuF;
	};
	 
	void __iomem		*mem_base0;
	void __iomem		*mem_base1;
	
	uint32_t		*message_wbuffer;
	
	uint32_t		*message_rbuffer;
	uint32_t		*msgcode_rwbuffer;	
	uint32_t		acb_flags;
	u16			dev_id;
	uint8_t			adapter_index;
#define ACB_F_SCSISTOPADAPTER         	0x0001
#define ACB_F_MSG_STOP_BGRB     	0x0002
 
#define ACB_F_MSG_START_BGRB          	0x0004
 
#define ACB_F_IOPDATA_OVERFLOW        	0x0008
 
#define ACB_F_MESSAGE_WQBUFFER_CLEARED	0x0010
 
#define ACB_F_MESSAGE_RQBUFFER_CLEARED  0x0020
 
#define ACB_F_MESSAGE_WQBUFFER_READED   0x0040
#define ACB_F_BUS_RESET               	0x0080

#define ACB_F_IOP_INITED              	0x0100
 
#define ACB_F_ABORT			0x0200
#define ACB_F_FIRMWARE_TRAP           	0x0400
#define ACB_F_ADAPTER_REMOVED		0x0800
#define ACB_F_MSG_GET_CONFIG		0x1000
	struct CommandControlBlock *	pccb_pool[ARCMSR_MAX_FREECCB_NUM];
	 
	struct list_head	ccb_free_list;
	 

	atomic_t		ccboutstandingcount;
	 

	void *			dma_coherent;
	 
	dma_addr_t		dma_coherent_handle;
	 
	dma_addr_t		dma_coherent_handle2;
	void			*dma_coherent2;
	unsigned int		uncache_size;
	uint8_t			rqbuffer[ARCMSR_MAX_QBUFFER];
	 
	int32_t			rqbuf_getIndex;
	 
	int32_t			rqbuf_putIndex;
	 
	uint8_t			wqbuffer[ARCMSR_MAX_QBUFFER];
	 
	int32_t			wqbuf_getIndex;
	 
	int32_t			wqbuf_putIndex;
	 
	uint8_t			devstate[ARCMSR_MAX_TARGETID][ARCMSR_MAX_TARGETLUN];
	 
#define ARECA_RAID_GONE			0x55
#define ARECA_RAID_GOOD			0xaa
	uint32_t		num_resets;
	uint32_t		num_aborts;
	uint32_t		signature;
	uint32_t		firm_request_len;
	uint32_t		firm_numbers_queue;
	uint32_t		firm_sdram_size;
	uint32_t		firm_hd_channels;
	uint32_t		firm_cfg_version;
	char			firm_model[12];
	char			firm_version[20];
	char			device_map[20];			 
	struct work_struct 	arcmsr_do_message_isr_bh;
	struct timer_list	eternal_timer;
	unsigned short		fw_flag;
#define	FW_NORMAL			0x0000
#define	FW_BOG				0x0001
#define	FW_DEADLOCK			0x0010
	uint32_t		maxOutstanding;
	int			vector_count;
	uint32_t		maxFreeCCB;
	struct timer_list	refresh_timer;
	uint32_t		doneq_index;
	uint32_t		ccbsize;
	uint32_t		in_doorbell;
	uint32_t		out_doorbell;
	uint32_t		completionQ_entry;
	pCompletion_Q		pCompletionQ;
	uint32_t		completeQ_size;
}; 
 
struct CommandControlBlock{
	 
	struct list_head		list;		 
	struct scsi_cmnd		*pcmd;		 
	struct AdapterControlBlock	*acb;		 
	unsigned long			cdb_phyaddr;	 
	uint32_t			arc_cdb_size;	 
	uint16_t			ccb_flags;	 
#define	CCB_FLAG_READ		0x0000
#define	CCB_FLAG_WRITE		0x0001
#define	CCB_FLAG_ERROR		0x0002
#define	CCB_FLAG_FLUSHCACHE	0x0004
#define	CCB_FLAG_MASTER_ABORTED	0x0008
	uint16_t                        startdone;	 
#define	ARCMSR_CCB_DONE		0x0000
#define	ARCMSR_CCB_START	0x55AA
#define	ARCMSR_CCB_ABORTED	0xAA55
#define	ARCMSR_CCB_ILLEGAL	0xFFFF
	uint32_t			smid;
#if BITS_PER_LONG == 64
	 
		uint32_t		reserved[3];	 
#else
	 
		uint32_t		reserved[8];	 
#endif
	 
	struct ARCMSR_CDB		arcmsr_cdb;
};
 
struct SENSE_DATA
{
	uint8_t				ErrorCode:7;
#define SCSI_SENSE_CURRENT_ERRORS	0x70
#define SCSI_SENSE_DEFERRED_ERRORS	0x71
	uint8_t				Valid:1;
	uint8_t				SegmentNumber;
	uint8_t				SenseKey:4;
	uint8_t				Reserved:1;
	uint8_t				IncorrectLength:1;
	uint8_t				EndOfMedia:1;
	uint8_t				FileMark:1;
	uint8_t				Information[4];
	uint8_t				AdditionalSenseLength;
	uint8_t				CommandSpecificInformation[4];
	uint8_t				AdditionalSenseCode;
	uint8_t				AdditionalSenseCodeQualifier;
	uint8_t				FieldReplaceableUnitCode;
	uint8_t				SenseKeySpecific[3];
};
 
#define	ARCMSR_MU_OUTBOUND_INTERRUPT_STATUS_REG	0x30
#define	ARCMSR_MU_OUTBOUND_PCI_INT		0x10
#define	ARCMSR_MU_OUTBOUND_POSTQUEUE_INT	0x08
#define	ARCMSR_MU_OUTBOUND_DOORBELL_INT		0x04
#define	ARCMSR_MU_OUTBOUND_MESSAGE1_INT		0x02
#define	ARCMSR_MU_OUTBOUND_MESSAGE0_INT		0x01
#define	ARCMSR_MU_OUTBOUND_HANDLE_INT                     \
                    (ARCMSR_MU_OUTBOUND_MESSAGE0_INT      \
                     |ARCMSR_MU_OUTBOUND_MESSAGE1_INT     \
                     |ARCMSR_MU_OUTBOUND_DOORBELL_INT     \
                     |ARCMSR_MU_OUTBOUND_POSTQUEUE_INT    \
                     |ARCMSR_MU_OUTBOUND_PCI_INT)
 
#define	ARCMSR_MU_OUTBOUND_INTERRUPT_MASK_REG		0x34
#define	ARCMSR_MU_OUTBOUND_PCI_INTMASKENABLE		0x10
#define	ARCMSR_MU_OUTBOUND_POSTQUEUE_INTMASKENABLE	0x08
#define	ARCMSR_MU_OUTBOUND_DOORBELL_INTMASKENABLE	0x04
#define	ARCMSR_MU_OUTBOUND_MESSAGE1_INTMASKENABLE	0x02
#define	ARCMSR_MU_OUTBOUND_MESSAGE0_INTMASKENABLE	0x01
#define	ARCMSR_MU_OUTBOUND_ALL_INTMASKENABLE		0x1F

extern void arcmsr_write_ioctldata2iop(struct AdapterControlBlock *);
extern uint32_t arcmsr_Read_iop_rqbuffer_data(struct AdapterControlBlock *,
	struct QBUFFER __iomem *);
extern void arcmsr_clear_iop2drv_rqueue_buffer(struct AdapterControlBlock *);
extern struct QBUFFER __iomem *arcmsr_get_iop_rqbuffer(struct AdapterControlBlock *);
extern const struct attribute_group *arcmsr_host_groups[];
extern int arcmsr_alloc_sysfs_attr(struct AdapterControlBlock *);
void arcmsr_free_sysfs_attr(struct AdapterControlBlock *acb);
