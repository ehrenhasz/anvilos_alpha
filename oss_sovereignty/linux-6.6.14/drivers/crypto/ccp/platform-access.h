 
 

#ifndef __PSP_PLATFORM_ACCESS_H__
#define __PSP_PLATFORM_ACCESS_H__

#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/psp-platform-access.h>

#include "psp-dev.h"

struct psp_platform_access_device {
	struct device *dev;
	struct psp_device *psp;

	struct platform_access_vdata *vdata;

	struct mutex mailbox_mutex;
	struct mutex doorbell_mutex;

	void *platform_access_data;
};

void platform_access_dev_destroy(struct psp_device *psp);
int platform_access_dev_init(struct psp_device *psp);

#endif  
