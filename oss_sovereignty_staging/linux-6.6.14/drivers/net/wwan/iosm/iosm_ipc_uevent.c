
 

#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/slab.h>

#include "iosm_ipc_uevent.h"

 
static void ipc_uevent_work(struct work_struct *data)
{
	struct ipc_uevent_info *info;
	char *envp[2] = { NULL, NULL };

	info = container_of(data, struct ipc_uevent_info, work);

	envp[0] = info->uevent;

	if (kobject_uevent_env(&info->dev->kobj, KOBJ_CHANGE, envp))
		pr_err("uevent %s failed to sent", info->uevent);

	kfree(info);
}

void ipc_uevent_send(struct device *dev, char *uevent)
{
	struct ipc_uevent_info *info = kzalloc(sizeof(*info), GFP_ATOMIC);

	if (!info)
		return;

	 
	INIT_WORK(&info->work, ipc_uevent_work);

	 
	info->dev = dev;
	snprintf(info->uevent, MAX_UEVENT_LEN, "IOSM_EVENT=%s", uevent);

	 
	schedule_work(&info->work);
}
