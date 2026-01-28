#ifndef _UAPI_ASM_S390_CHPID_H
#define _UAPI_ASM_S390_CHPID_H
#include <linux/string.h>
#include <linux/types.h>
#define __MAX_CHPID 255
struct chp_id {
	__u8 reserved1;
	__u8 cssid;
	__u8 reserved2;
	__u8 id;
} __attribute__((packed));
#endif  
