





#include <linux/mii_timestamper.h>

static LIST_HEAD(mii_timestamping_devices);
static DEFINE_MUTEX(tstamping_devices_lock);

struct mii_timestamping_desc {
	struct list_head list;
	struct mii_timestamping_ctrl *ctrl;
	struct device *device;
};

 
int register_mii_tstamp_controller(struct device *device,
				   struct mii_timestamping_ctrl *ctrl)
{
	struct mii_timestamping_desc *desc;

	desc = kzalloc(sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	INIT_LIST_HEAD(&desc->list);
	desc->ctrl = ctrl;
	desc->device = device;

	mutex_lock(&tstamping_devices_lock);
	list_add_tail(&mii_timestamping_devices, &desc->list);
	mutex_unlock(&tstamping_devices_lock);

	return 0;
}
EXPORT_SYMBOL(register_mii_tstamp_controller);

 
void unregister_mii_tstamp_controller(struct device *device)
{
	struct mii_timestamping_desc *desc;
	struct list_head *this, *next;

	mutex_lock(&tstamping_devices_lock);
	list_for_each_safe(this, next, &mii_timestamping_devices) {
		desc = list_entry(this, struct mii_timestamping_desc, list);
		if (desc->device == device) {
			list_del_init(&desc->list);
			kfree(desc);
			break;
		}
	}
	mutex_unlock(&tstamping_devices_lock);
}
EXPORT_SYMBOL(unregister_mii_tstamp_controller);

 
struct mii_timestamper *register_mii_timestamper(struct device_node *node,
						 unsigned int port)
{
	struct mii_timestamper *mii_ts = NULL;
	struct mii_timestamping_desc *desc;
	struct list_head *this;

	mutex_lock(&tstamping_devices_lock);
	list_for_each(this, &mii_timestamping_devices) {
		desc = list_entry(this, struct mii_timestamping_desc, list);
		if (desc->device->of_node == node) {
			mii_ts = desc->ctrl->probe_channel(desc->device, port);
			if (!IS_ERR(mii_ts)) {
				mii_ts->device = desc->device;
				get_device(desc->device);
			}
			break;
		}
	}
	mutex_unlock(&tstamping_devices_lock);

	return mii_ts ? mii_ts : ERR_PTR(-EPROBE_DEFER);
}
EXPORT_SYMBOL(register_mii_timestamper);

 
void unregister_mii_timestamper(struct mii_timestamper *mii_ts)
{
	struct mii_timestamping_desc *desc;
	struct list_head *this;

	if (!mii_ts)
		return;

	 
	if (!mii_ts->device)
		return;

	mutex_lock(&tstamping_devices_lock);
	list_for_each(this, &mii_timestamping_devices) {
		desc = list_entry(this, struct mii_timestamping_desc, list);
		if (desc->device == mii_ts->device) {
			desc->ctrl->release_channel(desc->device, mii_ts);
			put_device(desc->device);
			break;
		}
	}
	mutex_unlock(&tstamping_devices_lock);
}
EXPORT_SYMBOL(unregister_mii_timestamper);
