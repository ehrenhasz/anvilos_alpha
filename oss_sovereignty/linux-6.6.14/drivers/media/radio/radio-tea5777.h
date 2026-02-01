 
#ifndef __RADIO_TEA5777_H
#define __RADIO_TEA5777_H

 

#include <linux/videodev2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>

#define TEA575X_FMIF	10700
#define TEA575X_AMIF	  450

struct radio_tea5777;

struct radio_tea5777_ops {
	 
	int (*write_reg)(struct radio_tea5777 *tea, u64 val);
	 
	int (*read_reg)(struct radio_tea5777 *tea, u32 *val);
};

struct radio_tea5777 {
	struct v4l2_device *v4l2_dev;
	struct v4l2_file_operations fops;
	struct video_device vd;		 
	bool has_am;			 
	bool write_before_read;		 
	bool needs_write;		 
	u32 band;			 
	u32 freq;			 
	u32 audmode;			 
	u32 seek_rangelow;		 
	u32 seek_rangehigh;
	u32 read_reg;
	u64 write_reg;
	struct mutex mutex;
	const struct radio_tea5777_ops *ops;
	void *private_data;
	u8 card[32];
	u8 bus_info[32];
	struct v4l2_ctrl_handler ctrl_handler;
};

int radio_tea5777_init(struct radio_tea5777 *tea, struct module *owner);
void radio_tea5777_exit(struct radio_tea5777 *tea);
int radio_tea5777_set_freq(struct radio_tea5777 *tea);

#endif  
