


#ifndef __RPMSG_CHRDEV_H__
#define __RPMSG_CHRDEV_H__

#if IS_ENABLED(CONFIG_RPMSG_CHAR)

int rpmsg_chrdev_eptdev_create(struct rpmsg_device *rpdev, struct device *parent,
			       struct rpmsg_channel_info chinfo);


int rpmsg_chrdev_eptdev_destroy(struct device *dev, void *data);

#else  

static inline int rpmsg_chrdev_eptdev_create(struct rpmsg_device *rpdev, struct device *parent,
					     struct rpmsg_channel_info chinfo)
{
	return -ENXIO;
}

static inline int rpmsg_chrdev_eptdev_destroy(struct device *dev, void *data)
{
	return -ENXIO;
}

#endif 

#endif 
