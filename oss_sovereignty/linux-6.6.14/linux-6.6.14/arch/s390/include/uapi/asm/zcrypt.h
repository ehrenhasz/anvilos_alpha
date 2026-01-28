#ifndef __ASM_S390_ZCRYPT_H
#define __ASM_S390_ZCRYPT_H
#define ZCRYPT_VERSION 2
#define ZCRYPT_RELEASE 2
#define ZCRYPT_VARIANT 1
#include <linux/ioctl.h>
#include <linux/compiler.h>
#include <linux/types.h>
#define ZCRYPT_NAME "zcrypt"
struct ica_rsa_modexpo {
	__u8 __user  *inputdata;
	__u32	      inputdatalength;
	__u8 __user  *outputdata;
	__u32	      outputdatalength;
	__u8 __user  *b_key;
	__u8 __user  *n_modulus;
};
struct ica_rsa_modexpo_crt {
	__u8 __user  *inputdata;
	__u32	      inputdatalength;
	__u8 __user  *outputdata;
	__u32	      outputdatalength;
	__u8 __user  *bp_key;
	__u8 __user  *bq_key;
	__u8 __user  *np_prime;
	__u8 __user  *nq_prime;
	__u8 __user  *u_mult_inv;
};
struct CPRBX {
	__u16	     cprb_len;		 
	__u8	     cprb_ver_id;	 
	__u8	     ctfm;		 
	__u8	     pad_000[2];	 
	__u8	     func_id[2];	 
	__u8	     cprb_flags[4];	 
	__u32	     req_parml;		 
	__u32	     req_datal;		 
	__u32	     rpl_msgbl;		 
	__u32	     rpld_parml;	 
	__u32	     rpl_datal;		 
	__u32	     rpld_datal;	 
	__u32	     req_extbl;		 
	__u8	     _pad_001[4];	 
	__u32	     rpld_extbl;	 
	__u8	     _pad_002[16 - sizeof(__u8 *)];
	__u8 __user *req_parmb;		 
	__u8	     _pad_003[16 - sizeof(__u8 *)];
	__u8 __user *req_datab;		 
	__u8	     _pad_004[16 - sizeof(__u8 *)];
	__u8 __user *rpl_parmb;		 
	__u8	     _pad_005[16 - sizeof(__u8 *)];
	__u8 __user *rpl_datab;		 
	__u8	     _pad_006[16 - sizeof(__u8 *)];
	__u8 __user *req_extb;		 
	__u8	     _pad_007[16 - sizeof(__u8 *)];
	__u8 __user *rpl_extb;		 
	__u16	     ccp_rtcode;	 
	__u16	     ccp_rscode;	 
	__u32	     mac_data_len;	 
	__u8	     logon_id[8];	 
	__u8	     mac_value[8];	 
	__u8	     mac_content_flgs;	 
	__u8	     _pad_008;		 
	__u16	     domain;		 
	__u8	     _pad_009[12];	 
	__u8	     _pad_010[36];	 
} __attribute__((packed));
struct ica_xcRB {
	__u16	      agent_ID;
	__u32	      user_defined;
	__u16	      request_ID;
	__u32	      request_control_blk_length;
	__u8	      _padding1[16 - sizeof(__u8 *)];
	__u8 __user  *request_control_blk_addr;
	__u32	      request_data_length;
	__u8	      _padding2[16 - sizeof(__u8 *)];
	__u8 __user  *request_data_address;
	__u32	      reply_control_blk_length;
	__u8	      _padding3[16 - sizeof(__u8 *)];
	__u8 __user  *reply_control_blk_addr;
	__u32	      reply_data_length;
	__u8	      __padding4[16 - sizeof(__u8 *)];
	__u8 __user  *reply_data_addr;
	__u16	      priority_window;
	__u32	      status;
} __attribute__((packed));
struct ep11_cprb {
	__u16	cprb_len;
	__u8	cprb_ver_id;
	__u8	pad_000[2];
	__u8	flags;
	__u8	func_id[2];
	__u32	source_id;
	__u32	target_id;
	__u32	ret_code;
	__u32	reserved1;
	__u32	reserved2;
	__u32	payload_len;
} __attribute__((packed));
struct ep11_target_dev {
	__u16 ap_id;
	__u16 dom_id;
};
struct ep11_urb {
	__u16		targets_num;
	__u8 __user    *targets;
	__u64		weight;
	__u64		req_no;
	__u64		req_len;
	__u8 __user    *req;
	__u64		resp_len;
	__u8 __user    *resp;
} __attribute__((packed));
struct zcrypt_device_status_ext {
	unsigned int hwtype:8;
	unsigned int qid:16;
	unsigned int online:1;
	unsigned int functions:6;
	unsigned int reserved:1;
};
#define MAX_ZDEV_CARDIDS_EXT 256
#define MAX_ZDEV_DOMAINS_EXT 256
#define MAX_ZDEV_ENTRIES_EXT (MAX_ZDEV_CARDIDS_EXT * MAX_ZDEV_DOMAINS_EXT)
struct zcrypt_device_matrix_ext {
	struct zcrypt_device_status_ext device[MAX_ZDEV_ENTRIES_EXT];
};
#define AUTOSELECT  0xFFFFFFFF
#define AUTOSEL_AP  ((__u16)0xFFFF)
#define AUTOSEL_DOM ((__u16)0xFFFF)
#define ZCRYPT_IOCTL_MAGIC 'z'
#define ICARSAMODEXPO  _IOC(_IOC_READ | _IOC_WRITE, ZCRYPT_IOCTL_MAGIC, 0x05, 0)
#define ICARSACRT      _IOC(_IOC_READ | _IOC_WRITE, ZCRYPT_IOCTL_MAGIC, 0x06, 0)
#define ZSECSENDCPRB   _IOC(_IOC_READ | _IOC_WRITE, ZCRYPT_IOCTL_MAGIC, 0x81, 0)
#define ZSENDEP11CPRB  _IOC(_IOC_READ | _IOC_WRITE, ZCRYPT_IOCTL_MAGIC, 0x04, 0)
#define ZCRYPT_DEVICE_STATUS _IOC(_IOC_READ | _IOC_WRITE, ZCRYPT_IOCTL_MAGIC, 0x5f, 0)
#define ZCRYPT_STATUS_MASK   _IOR(ZCRYPT_IOCTL_MAGIC, 0x58, char[MAX_ZDEV_CARDIDS_EXT])
#define ZCRYPT_QDEPTH_MASK   _IOR(ZCRYPT_IOCTL_MAGIC, 0x59, char[MAX_ZDEV_CARDIDS_EXT])
#define ZCRYPT_PERDEV_REQCNT _IOR(ZCRYPT_IOCTL_MAGIC, 0x5a, int[MAX_ZDEV_CARDIDS_EXT])
#define ZCRYPT_MAX_MINOR_NODES 256
#define MAX_ZDEV_IOCTLS (1 << _IOC_NRBITS)
#define MAX_ZDEV_CARDIDS 64
#define MAX_ZDEV_DOMAINS 256
#define MAX_ZDEV_ENTRIES (MAX_ZDEV_CARDIDS * MAX_ZDEV_DOMAINS)
struct zcrypt_device_status {
	unsigned int hwtype:8;
	unsigned int qid:14;
	unsigned int online:1;
	unsigned int functions:6;
	unsigned int reserved:3;
};
struct zcrypt_device_matrix {
	struct zcrypt_device_status device[MAX_ZDEV_ENTRIES];
};
#define ZDEVICESTATUS _IOC(_IOC_READ | _IOC_WRITE, ZCRYPT_IOCTL_MAGIC, 0x4f, 0)
#define Z90STAT_STATUS_MASK _IOR(ZCRYPT_IOCTL_MAGIC, 0x48, char[64])
#define Z90STAT_QDEPTH_MASK _IOR(ZCRYPT_IOCTL_MAGIC, 0x49, char[64])
#define Z90STAT_PERDEV_REQCNT _IOR(ZCRYPT_IOCTL_MAGIC, 0x4a, int[64])
#define Z90STAT_REQUESTQ_COUNT	_IOR(ZCRYPT_IOCTL_MAGIC, 0x44, int)
#define Z90STAT_PENDINGQ_COUNT	_IOR(ZCRYPT_IOCTL_MAGIC, 0x45, int)
#define Z90STAT_TOTALOPEN_COUNT _IOR(ZCRYPT_IOCTL_MAGIC, 0x46, int)
#define Z90STAT_DOMAIN_INDEX	_IOR(ZCRYPT_IOCTL_MAGIC, 0x47, int)
#endif  
