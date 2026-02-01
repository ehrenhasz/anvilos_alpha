 
 

#ifndef _ZYNQMP_DPSUB_H_
#define _ZYNQMP_DPSUB_H_

struct clk;
struct device;
struct drm_bridge;
struct zynqmp_disp;
struct zynqmp_disp_layer;
struct zynqmp_dp;
struct zynqmp_dpsub_drm;

#define ZYNQMP_DPSUB_NUM_LAYERS				2

enum zynqmp_dpsub_port {
	ZYNQMP_DPSUB_PORT_LIVE_VIDEO,
	ZYNQMP_DPSUB_PORT_LIVE_GFX,
	ZYNQMP_DPSUB_PORT_LIVE_AUDIO,
	ZYNQMP_DPSUB_PORT_OUT_VIDEO,
	ZYNQMP_DPSUB_PORT_OUT_AUDIO,
	ZYNQMP_DPSUB_PORT_OUT_DP,
	ZYNQMP_DPSUB_NUM_PORTS,
};

enum zynqmp_dpsub_format {
	ZYNQMP_DPSUB_FORMAT_RGB,
	ZYNQMP_DPSUB_FORMAT_YCRCB444,
	ZYNQMP_DPSUB_FORMAT_YCRCB422,
	ZYNQMP_DPSUB_FORMAT_YONLY,
};

 
struct zynqmp_dpsub {
	struct device *dev;

	struct clk *apb_clk;
	struct clk *vid_clk;
	bool vid_clk_from_ps;
	struct clk *aud_clk;
	bool aud_clk_from_ps;

	unsigned int connected_ports;
	bool dma_enabled;

	struct zynqmp_dpsub_drm *drm;
	struct drm_bridge *bridge;

	struct zynqmp_disp *disp;
	struct zynqmp_disp_layer *layers[ZYNQMP_DPSUB_NUM_LAYERS];
	struct zynqmp_dp *dp;

	unsigned int dma_align;
};

bool zynqmp_dpsub_audio_enabled(struct zynqmp_dpsub *dpsub);
unsigned int zynqmp_dpsub_get_audio_clk_rate(struct zynqmp_dpsub *dpsub);

void zynqmp_dpsub_release(struct zynqmp_dpsub *dpsub);

#endif  
