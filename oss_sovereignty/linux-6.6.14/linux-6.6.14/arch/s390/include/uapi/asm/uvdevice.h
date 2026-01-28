#ifndef __S390_ASM_UVDEVICE_H
#define __S390_ASM_UVDEVICE_H
#include <linux/types.h>
struct uvio_ioctl_cb {
	__u32 flags;
	__u16 uv_rc;			 
	__u16 uv_rrc;			 
	__u64 argument_addr;		 
	__u32 argument_len;
	__u8  reserved14[0x40 - 0x14];	 
};
#define UVIO_ATT_USER_DATA_LEN		0x100
#define UVIO_ATT_UID_LEN		0x10
struct uvio_attest {
	__u64 arcb_addr;				 
	__u64 meas_addr;				 
	__u64 add_data_addr;				 
	__u8  user_data[UVIO_ATT_USER_DATA_LEN];	 
	__u8  config_uid[UVIO_ATT_UID_LEN];		 
	__u32 arcb_len;					 
	__u32 meas_len;					 
	__u32 add_data_len;				 
	__u16 user_data_len;				 
	__u16 reserved136;				 
};
struct uvio_uvdev_info {
	__u64 supp_uvio_cmds;
	__u64 supp_uv_cmds;
};
#define UVIO_ATT_ARCB_MAX_LEN		0x100000
#define UVIO_ATT_MEASUREMENT_MAX_LEN	0x8000
#define UVIO_ATT_ADDITIONAL_MAX_LEN	0x8000
#define UVIO_ADD_SECRET_MAX_LEN		0x100000
#define UVIO_LIST_SECRETS_LEN		0x1000
#define UVIO_DEVICE_NAME "uv"
#define UVIO_TYPE_UVC 'u'
enum UVIO_IOCTL_NR {
	UVIO_IOCTL_UVDEV_INFO_NR = 0x00,
	UVIO_IOCTL_ATT_NR,
	UVIO_IOCTL_ADD_SECRET_NR,
	UVIO_IOCTL_LIST_SECRETS_NR,
	UVIO_IOCTL_LOCK_SECRETS_NR,
	UVIO_IOCTL_NUM_IOCTLS
};
#define UVIO_IOCTL(nr)		_IOWR(UVIO_TYPE_UVC, nr, struct uvio_ioctl_cb)
#define UVIO_IOCTL_UVDEV_INFO	UVIO_IOCTL(UVIO_IOCTL_UVDEV_INFO_NR)
#define UVIO_IOCTL_ATT		UVIO_IOCTL(UVIO_IOCTL_ATT_NR)
#define UVIO_IOCTL_ADD_SECRET	UVIO_IOCTL(UVIO_IOCTL_ADD_SECRET_NR)
#define UVIO_IOCTL_LIST_SECRETS	UVIO_IOCTL(UVIO_IOCTL_LIST_SECRETS_NR)
#define UVIO_IOCTL_LOCK_SECRETS	UVIO_IOCTL(UVIO_IOCTL_LOCK_SECRETS_NR)
#define UVIO_SUPP_CALL(nr)	(1ULL << (nr))
#define UVIO_SUPP_UDEV_INFO	UVIO_SUPP_CALL(UVIO_IOCTL_UDEV_INFO_NR)
#define UVIO_SUPP_ATT		UVIO_SUPP_CALL(UVIO_IOCTL_ATT_NR)
#define UVIO_SUPP_ADD_SECRET	UVIO_SUPP_CALL(UVIO_IOCTL_ADD_SECRET_NR)
#define UVIO_SUPP_LIST_SECRETS	UVIO_SUPP_CALL(UVIO_IOCTL_LIST_SECRETS_NR)
#define UVIO_SUPP_LOCK_SECRETS	UVIO_SUPP_CALL(UVIO_IOCTL_LOCK_SECRETS_NR)
#endif  
