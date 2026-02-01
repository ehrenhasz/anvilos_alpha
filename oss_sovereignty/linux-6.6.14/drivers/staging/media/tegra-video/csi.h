 
 

#ifndef __TEGRA_CSI_H__
#define __TEGRA_CSI_H__

#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-subdev.h>

 
#define CSI_PORTS_PER_BRICK	2
#define CSI_LANES_PER_BRICK	4

 
#define GANG_PORTS_MAX	2

 
#define TEGRA_CSI_PADS_NUM	2

enum tegra_csi_cil_port {
	PORT_A = 0,
	PORT_B,
};

enum tegra_csi_block {
	CSI_CIL_AB = 0,
	CSI_CIL_CD,
	CSI_CIL_EF,
};

struct tegra_csi;

 
struct tegra_csi_channel {
	struct list_head list;
	struct v4l2_subdev subdev;
	struct media_pad pads[TEGRA_CSI_PADS_NUM];
	unsigned int numpads;
	struct tegra_csi *csi;
	struct device_node *of_node;
	u8 numgangports;
	unsigned int numlanes;
	u8 csi_port_nums[GANG_PORTS_MAX];
	u8 pg_mode;
	struct v4l2_mbus_framefmt format;
	unsigned int framerate;
	unsigned int h_blank;
	unsigned int v_blank;
	struct tegra_mipi_device *mipi;
	unsigned int pixel_rate;
};

 
struct tpg_framerate {
	struct v4l2_frmsize_discrete frmsize;
	u32 code;
	unsigned int h_blank;
	unsigned int v_blank;
	unsigned int framerate;
};

 
struct tegra_csi_ops {
	int (*csi_start_streaming)(struct tegra_csi_channel *csi_chan);
	void (*csi_stop_streaming)(struct tegra_csi_channel *csi_chan);
	void (*csi_err_recover)(struct tegra_csi_channel *csi_chan);
};

 
struct tegra_csi_soc {
	const struct tegra_csi_ops *ops;
	unsigned int csi_max_channels;
	const char * const *clk_names;
	unsigned int num_clks;
	const struct tpg_framerate *tpg_frmrate_table;
	unsigned int tpg_frmrate_table_size;
};

 
struct tegra_csi {
	struct device *dev;
	struct host1x_client client;
	void __iomem *iomem;
	struct clk_bulk_data *clks;
	const struct tegra_csi_soc *soc;
	const struct tegra_csi_ops *ops;
	struct list_head csi_chans;
};

void tegra_csi_error_recover(struct v4l2_subdev *subdev);
void tegra_csi_calc_settle_time(struct tegra_csi_channel *csi_chan,
				u8 csi_port_num,
				u8 *clk_settle_time,
				u8 *ths_settle_time);
#endif
