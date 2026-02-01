 
#ifndef _LIQUIDIO_IMAGE_H_
#define _LIQUIDIO_IMAGE_H_

#define LIO_MAX_FW_TYPE_LEN     (8)
#define LIO_MAX_FW_FILENAME_LEN (256)
#define LIO_FW_DIR              "liquidio/"
#define LIO_FW_BASE_NAME        "lio_"
#define LIO_FW_NAME_SUFFIX      ".bin"
#define LIO_FW_NAME_TYPE_NIC    "nic"
#define LIO_FW_NAME_TYPE_AUTO   "auto"
#define LIO_FW_NAME_TYPE_NONE   "none"
#define LIO_MAX_FIRMWARE_VERSION_LEN 16

#define LIO_MAX_BOOTCMD_LEN 1024
#define LIO_MAX_IMAGES 16
#define LIO_NIC_MAGIC 0x434E4943      
struct octeon_firmware_desc {
	__be64 addr;
	__be32 len;
	__be32 crc32;          
};

 
struct octeon_firmware_file_header {
	__be32 magic;
	char version[LIO_MAX_FIRMWARE_VERSION_LEN];
	char bootcmd[LIO_MAX_BOOTCMD_LEN];
	__be32 num_images;
	struct octeon_firmware_desc desc[LIO_MAX_IMAGES];
	__be32 pad;
	__be32 crc32;          
};

#endif  
