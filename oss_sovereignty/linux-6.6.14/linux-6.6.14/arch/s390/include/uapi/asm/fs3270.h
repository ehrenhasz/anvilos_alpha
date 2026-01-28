#ifndef __ASM_S390_UAPI_FS3270_H
#define __ASM_S390_UAPI_FS3270_H
#include <linux/types.h>
#include <asm/ioctl.h>
#define TUBICMD		_IO('3', 3)	 
#define TUBOCMD		_IO('3', 4)	 
#define TUBGETI		_IO('3', 7)	 
#define TUBGETO		_IO('3', 8)	 
#define TUBGETMOD	_IO('3', 13)	 
struct raw3270_iocb {
	__u16 model;
	__u16 line_cnt;
	__u16 col_cnt;
	__u16 pf_cnt;
	__u16 re_cnt;
	__u16 map;
};
#endif  
