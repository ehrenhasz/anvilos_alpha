 
 

#ifndef _CEDRUS_HW_H_
#define _CEDRUS_HW_H_

int cedrus_engine_enable(struct cedrus_ctx *ctx);
void cedrus_engine_disable(struct cedrus_dev *dev);

void cedrus_dst_format_set(struct cedrus_dev *dev,
			   struct v4l2_pix_format *fmt);

int cedrus_hw_suspend(struct device *device);
int cedrus_hw_resume(struct device *device);

int cedrus_hw_probe(struct cedrus_dev *dev);
void cedrus_hw_remove(struct cedrus_dev *dev);

void cedrus_watchdog(struct work_struct *work);

#endif
