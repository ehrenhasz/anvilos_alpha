 
 

#ifndef MMAL_MSG_COMMON_H
#define MMAL_MSG_COMMON_H

#include <linux/types.h>

enum mmal_msg_status {
	MMAL_MSG_STATUS_SUCCESS = 0,  
	MMAL_MSG_STATUS_ENOMEM,       
	MMAL_MSG_STATUS_ENOSPC,       
	MMAL_MSG_STATUS_EINVAL,       
	MMAL_MSG_STATUS_ENOSYS,       
	MMAL_MSG_STATUS_ENOENT,       
	MMAL_MSG_STATUS_ENXIO,        
	MMAL_MSG_STATUS_EIO,          
	MMAL_MSG_STATUS_ESPIPE,       
	MMAL_MSG_STATUS_ECORRUPT,     
	MMAL_MSG_STATUS_ENOTREADY,    
	MMAL_MSG_STATUS_ECONFIG,      
	MMAL_MSG_STATUS_EISCONN,      
	MMAL_MSG_STATUS_ENOTCONN,     
	MMAL_MSG_STATUS_EAGAIN,       
	MMAL_MSG_STATUS_EFAULT,       
};

struct mmal_rect {
	s32 x;       
	s32 y;       
	s32 width;   
	s32 height;  
};

#endif  
