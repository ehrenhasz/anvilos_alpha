#ifndef __TEGRA_VIP_H__
#define __TEGRA_VIP_H__
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-subdev.h>
enum {
	TEGRA_VIP_PAD_SINK,
	TEGRA_VIP_PAD_SOURCE,
	TEGRA_VIP_PADS_NUM,
};
struct tegra_vip;
struct tegra_vip_channel {
	struct v4l2_subdev subdev;
	struct media_pad pads[TEGRA_VIP_PADS_NUM];
	struct device_node *of_node;
};
struct tegra_vip_ops {
	int (*vip_start_streaming)(struct tegra_vip_channel *vip_chan);
};
struct tegra_vip_soc {
	const struct tegra_vip_ops *ops;
};
struct tegra_vip {
	struct device *dev;
	struct host1x_client client;
	const struct tegra_vip_soc *soc;
	struct tegra_vip_channel chan;
};
#endif
