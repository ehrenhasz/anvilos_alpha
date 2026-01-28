#ifndef _UAPI_ASM_VMCP_H
#define _UAPI_ASM_VMCP_H
#include <linux/ioctl.h>
#define VMCP_GETCODE	_IOR(0x10, 1, int)
#define VMCP_SETBUF	_IOW(0x10, 2, int)
#define VMCP_GETSIZE	_IOR(0x10, 3, int)
#endif  
