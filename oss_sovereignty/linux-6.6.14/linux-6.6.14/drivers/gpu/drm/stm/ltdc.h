#ifndef _LTDC_H_
#define _LTDC_H_
struct ltdc_caps {
	u32 hw_version;		 
	u32 nb_layers;		 
	u32 layer_ofs;		 
	const u32 *layer_regs;	 
	u32 bus_width;		 
	const u32 *pix_fmt_hw;	 
	const u32 *pix_fmt_drm;	 
	int pix_fmt_nb;		 
	bool pix_fmt_flex;	 
	bool non_alpha_only_l1;  
	int pad_max_freq_hz;	 
	int nb_irq;		 
	bool ycbcr_input;	 
	bool ycbcr_output;	 
	bool plane_reg_shadow;	 
	bool crc;		 
	bool dynamic_zorder;	 
	bool plane_rotation;	 
	bool fifo_threshold;	 
};
#define LTDC_MAX_LAYER	4
struct fps_info {
	unsigned int counter;
	ktime_t last_timestamp;
};
struct ltdc_device {
	void __iomem *regs;
	struct regmap *regmap;
	struct clk *pixel_clk;	 
	struct mutex err_lock;	 
	struct ltdc_caps caps;
	u32 irq_status;
	u32 fifo_err;		 
	u32 fifo_warn;		 
	u32 fifo_threshold;	 
	u32 transfer_err;	 
	struct fps_info plane_fpsi[LTDC_MAX_LAYER];
	struct drm_atomic_state *suspend_state;
	int crc_skip_count;
	bool crc_active;
};
int ltdc_load(struct drm_device *ddev);
void ltdc_unload(struct drm_device *ddev);
void ltdc_suspend(struct drm_device *ddev);
int ltdc_resume(struct drm_device *ddev);
#endif
