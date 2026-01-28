


#ifndef __RPMSG_INTERNAL_H__
#define __RPMSG_INTERNAL_H__

#include <linux/rpmsg.h>
#include <linux/poll.h>

#define to_rpmsg_device(d) container_of(d, struct rpmsg_device, dev)
#define to_rpmsg_driver(d) container_of(d, struct rpmsg_driver, drv)

extern struct class *rpmsg_class;


struct rpmsg_device_ops {
	struct rpmsg_device *(*create_channel)(struct rpmsg_device *rpdev,
					       struct rpmsg_channel_info *chinfo);
	int (*release_channel)(struct rpmsg_device *rpdev,
			       struct rpmsg_channel_info *chinfo);
	struct rpmsg_endpoint *(*create_ept)(struct rpmsg_device *rpdev,
					    rpmsg_rx_cb_t cb, void *priv,
					    struct rpmsg_channel_info chinfo);

	int (*announce_create)(struct rpmsg_device *rpdev);
	int (*announce_destroy)(struct rpmsg_device *rpdev);
};


struct rpmsg_endpoint_ops {
	void (*destroy_ept)(struct rpmsg_endpoint *ept);

	int (*send)(struct rpmsg_endpoint *ept, void *data, int len);
	int (*sendto)(struct rpmsg_endpoint *ept, void *data, int len, u32 dst);
	int (*send_offchannel)(struct rpmsg_endpoint *ept, u32 src, u32 dst,
				  void *data, int len);

	int (*trysend)(struct rpmsg_endpoint *ept, void *data, int len);
	int (*trysendto)(struct rpmsg_endpoint *ept, void *data, int len, u32 dst);
	int (*trysend_offchannel)(struct rpmsg_endpoint *ept, u32 src, u32 dst,
			     void *data, int len);
	__poll_t (*poll)(struct rpmsg_endpoint *ept, struct file *filp,
			     poll_table *wait);
	int (*set_flow_control)(struct rpmsg_endpoint *ept, bool pause, u32 dst);
	ssize_t (*get_mtu)(struct rpmsg_endpoint *ept);
};

struct device *rpmsg_find_device(struct device *parent,
				 struct rpmsg_channel_info *chinfo);

struct rpmsg_device *rpmsg_create_channel(struct rpmsg_device *rpdev,
					  struct rpmsg_channel_info *chinfo);
int rpmsg_release_channel(struct rpmsg_device *rpdev,
			  struct rpmsg_channel_info *chinfo);

static inline int rpmsg_ctrldev_register_device(struct rpmsg_device *rpdev)
{
	return rpmsg_register_device_override(rpdev, "rpmsg_ctrl");
}

#endif
