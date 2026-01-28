#ifndef BOOT_LOADER_EXPORT_H_
#define BOOT_LOADER_EXPORT_H_
struct bl_dedicated_registers_v1 {
	__le32	boot_loader_ready;		 
	__le32	boot_loader_struct_version;	 
	__le16	rf_type;			 
	__le16	rf_status;			 
	__le32	baseband_type;			 
	u8	mac_address[6];			 
	u8	bl_version_major;		 
	u8	bl_version_minor;		 
	__le16	bl_version_subminor;		 
	__le16	bl_version_build;		 
	__le32  bl_assert_code;          
	__le32  bl_assert_blink;         
	__le32  bl_shutdown_handshake;   
	__le32  bl_reserved[21];         
	__le32  bl_magic_number;         
} __packed;
struct bl_dedicated_registers_v0 {
	__le32	boot_loader_ready;		 
#define BL_READY (1)	 
	__le32	boot_loader_struct_version;	 
	__le32	rf_type;			 
	__le32	baseband_type;			 
	u8	mac_address[6];			 
} __packed;
#define BL_SHUTDOWN_HS_GRTD		BIT(0)
#define BL_SHUTDOWN_HS_RTD		BIT(1)
#define BL_SHUTDOWN_HS_PROT_VER(x) WIL_GET_BITS(x, 28, 31)
#endif  
