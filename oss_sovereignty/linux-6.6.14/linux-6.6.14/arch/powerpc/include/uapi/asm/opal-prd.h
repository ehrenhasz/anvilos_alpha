#ifndef _UAPI_ASM_POWERPC_OPAL_PRD_H_
#define _UAPI_ASM_POWERPC_OPAL_PRD_H_
#include <linux/types.h>
#define OPAL_PRD_KERNEL_VERSION		1
#define OPAL_PRD_GET_INFO		_IOR('o', 0x01, struct opal_prd_info)
#define OPAL_PRD_SCOM_READ		_IOR('o', 0x02, struct opal_prd_scom)
#define OPAL_PRD_SCOM_WRITE		_IOW('o', 0x03, struct opal_prd_scom)
#ifndef __ASSEMBLY__
struct opal_prd_info {
	__u64	version;
	__u64	reserved[3];
};
struct opal_prd_scom {
	__u64	chip;
	__u64	addr;
	__u64	data;
	__s64	rc;
};
#endif  
#endif  
