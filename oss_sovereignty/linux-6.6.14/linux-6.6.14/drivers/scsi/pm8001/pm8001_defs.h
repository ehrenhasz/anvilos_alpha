#ifndef _PM8001_DEFS_H_
#define _PM8001_DEFS_H_
enum chip_flavors {
	chip_8001,
	chip_8008,
	chip_8009,
	chip_8018,
	chip_8019,
	chip_8074,
	chip_8076,
	chip_8077,
	chip_8006,
	chip_8070,
	chip_8072
};
enum phy_speed {
	PHY_SPEED_15 = 0x01,
	PHY_SPEED_30 = 0x02,
	PHY_SPEED_60 = 0x04,
	PHY_SPEED_120 = 0x08,
};
enum data_direction {
	DATA_DIR_NONE = 0x0,	 
	DATA_DIR_IN = 0x01,	 
	DATA_DIR_OUT = 0x02,	 
	DATA_DIR_BYRECIPIENT = 0x04,  
};
enum port_type {
	PORT_TYPE_SAS = (1L << 1),
	PORT_TYPE_SATA = (1L << 0),
};
#define	PM8001_MAX_CCB		 1024	 
#define PM8001_MPI_QUEUE         1024    
#define	PM8001_MAX_INB_NUM	 64
#define	PM8001_MAX_OUTB_NUM	 64
#define	PM8001_CAN_QUEUE	 508	 
#define IOMB_SIZE_SPC		64
#define IOMB_SIZE_SPCV		128
#define	PM8001_MAX_PHYS		 16	 
#define	PM8001_MAX_PORTS	 16	 
#define	PM8001_MAX_DEVICES	 2048	 
#define	PM8001_MAX_MSIX_VEC	 64	 
#define	PM8001_RESERVE_SLOT	 8
#define	CONFIG_SCSI_PM8001_MAX_DMA_SG	528
#define PM8001_MAX_DMA_SG	CONFIG_SCSI_PM8001_MAX_DMA_SG
enum memory_region_num {
	AAP1 = 0x0,  
	IOP,	     
	NVMD,	     
	FW_FLASH,     
	FORENSIC_MEM,   
	USI_MAX_MEMCNT_BASE
};
#define	PM8001_EVENT_LOG_SIZE	 (128 * 1024)
#define USI_MAX_MEMCNT	(USI_MAX_MEMCNT_BASE + ((2 * PM8001_MAX_INB_NUM) \
			+ (2 * PM8001_MAX_OUTB_NUM)))
enum mpi_err {
	MPI_IO_STATUS_SUCCESS = 0x0,
	MPI_IO_STATUS_BUSY = 0x01,
	MPI_IO_STATUS_FAIL = 0x02,
};
enum phy_control_type {
	PHY_LINK_RESET = 0x01,
	PHY_HARD_RESET = 0x02,
	PHY_NOTIFY_ENABLE_SPINUP = 0x10,
};
enum pm8001_hba_info_flags {
	PM8001F_INIT_TIME	= (1U << 0),
	PM8001F_RUN_TIME	= (1U << 1),
};
#define PHY_LINK_DISABLE	0x00
#define PHY_LINK_DOWN		0x01
#define PHY_STATE_LINK_UP_SPCV	0x2
#define PHY_STATE_LINK_UP_SPC	0x1
#endif
