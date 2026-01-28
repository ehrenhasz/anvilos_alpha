
#ifndef __SYSFS_UPLOAD_H
#define __SYSFS_UPLOAD_H

#include <linux/device.h>

#include "sysfs.h"


enum fw_upload_prog {
	FW_UPLOAD_PROG_IDLE,
	FW_UPLOAD_PROG_RECEIVING,
	FW_UPLOAD_PROG_PREPARING,
	FW_UPLOAD_PROG_TRANSFERRING,
	FW_UPLOAD_PROG_PROGRAMMING,
	FW_UPLOAD_PROG_MAX
};

struct fw_upload_priv {
	struct fw_upload *fw_upload;
	struct module *module;
	const char *name;
	const struct fw_upload_ops *ops;
	struct mutex lock;		  
	struct work_struct work;
	const u8 *data;			  
	u32 remaining_size;		  
	enum fw_upload_prog progress;
	enum fw_upload_prog err_progress; 
	enum fw_upload_err err_code;	  
};

#endif 
