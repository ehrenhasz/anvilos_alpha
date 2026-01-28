#ifndef _STI_COMPOSITOR_H_
#define _STI_COMPOSITOR_H_
#include <linux/clk.h>
#include <linux/kernel.h>
#include "sti_mixer.h"
#include "sti_plane.h"
#define WAIT_NEXT_VSYNC_MS      50  
#define STI_MAX_MIXER 2
#define STI_MAX_VID   1
enum sti_compositor_subdev_type {
	STI_MIXER_MAIN_SUBDEV,
	STI_MIXER_AUX_SUBDEV,
	STI_GPD_SUBDEV,
	STI_VID_SUBDEV,
	STI_CURSOR_SUBDEV,
};
struct sti_compositor_subdev_descriptor {
	enum sti_compositor_subdev_type type;
	int id;
	unsigned int offset;
};
#define MAX_SUBDEV 9
struct sti_compositor_data {
	unsigned int nb_subdev;
	struct sti_compositor_subdev_descriptor subdev_desc[MAX_SUBDEV];
};
struct sti_compositor {
	struct device *dev;
	void __iomem *regs;
	struct sti_compositor_data data;
	struct clk *clk_compo_main;
	struct clk *clk_compo_aux;
	struct clk *clk_pix_main;
	struct clk *clk_pix_aux;
	struct reset_control *rst_main;
	struct reset_control *rst_aux;
	struct sti_mixer *mixer[STI_MAX_MIXER];
	struct sti_vid *vid[STI_MAX_VID];
	struct sti_vtg *vtg[STI_MAX_MIXER];
	struct notifier_block vtg_vblank_nb[STI_MAX_MIXER];
};
void sti_compositor_debugfs_init(struct sti_compositor *compo,
				 struct drm_minor *minor);
#endif
