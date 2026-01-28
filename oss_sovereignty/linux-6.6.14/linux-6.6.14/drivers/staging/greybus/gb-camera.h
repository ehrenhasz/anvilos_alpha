#ifndef __GB_CAMERA_H
#define __GB_CAMERA_H
#include <linux/v4l2-mediabus.h>
#define GB_CAMERA_IN_FLAG_TEST		(1 << 0)
#define GB_CAMERA_OUT_FLAG_ADJUSTED	(1 << 0)
struct gb_camera_stream {
	unsigned int width;
	unsigned int height;
	enum v4l2_mbus_pixelcode pixel_code;
	unsigned int vc;
	unsigned int dt[2];
	unsigned int max_size;
};
struct gb_camera_csi_params {
	unsigned int num_lanes;
	unsigned int clk_freq;
};
struct gb_camera_ops {
	ssize_t (*capabilities)(void *priv, char *buf, size_t len);
	int (*configure_streams)(void *priv, unsigned int *nstreams,
			unsigned int *flags, struct gb_camera_stream *streams,
			struct gb_camera_csi_params *csi_params);
	int (*capture)(void *priv, u32 request_id,
			unsigned int streams, unsigned int num_frames,
			size_t settings_size, const void *settings);
	int (*flush)(void *priv, u32 *request_id);
};
struct gb_camera_module {
	void *priv;
	const struct gb_camera_ops *ops;
	unsigned int interface_id;
	struct kref refcount;
	void (*release)(struct kref *kref);
	struct list_head list;  
};
#define gb_camera_call(f, op, args...)      \
	(!(f) ? -ENODEV : (((f)->ops->op) ?  \
	(f)->ops->op((f)->priv, ##args) : -ENOIOCTLCMD))
int gb_camera_register(struct gb_camera_module *module);
int gb_camera_unregister(struct gb_camera_module *module);
#endif  
