#ifndef _ASM_POWERPC_PS3FB_H_
#define _ASM_POWERPC_PS3FB_H_
#include <linux/types.h>
#include <linux/ioctl.h>
#define PS3FB_IOCTL_SETMODE       _IOW('r',  1, int)  
#define PS3FB_IOCTL_GETMODE       _IOR('r',  2, int)  
#define PS3FB_IOCTL_SCREENINFO    _IOR('r',  3, int)  
#define PS3FB_IOCTL_ON            _IO('r', 4)         
#define PS3FB_IOCTL_OFF           _IO('r', 5)         
#define PS3FB_IOCTL_FSEL          _IOW('r', 6, int)   
#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC         _IOW('F', 0x20, __u32)  
#endif
struct ps3fb_ioctl_res {
	__u32 xres;  
	__u32 yres;  
	__u32 xoff;  
	__u32 yoff;  
	__u32 num_frames;  
};
#endif  
