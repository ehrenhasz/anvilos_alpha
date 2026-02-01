 
 
#ifndef _KOMEDA_PIPELINE_H_
#define _KOMEDA_PIPELINE_H_

#include <linux/types.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include "malidp_utils.h"
#include "komeda_color_mgmt.h"

#define KOMEDA_MAX_PIPELINES		2
#define KOMEDA_PIPELINE_MAX_LAYERS	4
#define KOMEDA_PIPELINE_MAX_SCALERS	2
#define KOMEDA_COMPONENT_N_INPUTS	5

 
enum {
	KOMEDA_COMPONENT_LAYER0		= 0,
	KOMEDA_COMPONENT_LAYER1		= 1,
	KOMEDA_COMPONENT_LAYER2		= 2,
	KOMEDA_COMPONENT_LAYER3		= 3,
	KOMEDA_COMPONENT_WB_LAYER	= 7,  
	KOMEDA_COMPONENT_SCALER0	= 8,
	KOMEDA_COMPONENT_SCALER1	= 9,
	KOMEDA_COMPONENT_SPLITTER	= 12,
	KOMEDA_COMPONENT_MERGER		= 14,
	KOMEDA_COMPONENT_COMPIZ0	= 16,  
	KOMEDA_COMPONENT_COMPIZ1	= 17,
	KOMEDA_COMPONENT_IPS0		= 20,  
	KOMEDA_COMPONENT_IPS1		= 21,
	KOMEDA_COMPONENT_TIMING_CTRLR	= 22,  
};

#define KOMEDA_PIPELINE_LAYERS		(BIT(KOMEDA_COMPONENT_LAYER0) |\
					 BIT(KOMEDA_COMPONENT_LAYER1) |\
					 BIT(KOMEDA_COMPONENT_LAYER2) |\
					 BIT(KOMEDA_COMPONENT_LAYER3))

#define KOMEDA_PIPELINE_SCALERS		(BIT(KOMEDA_COMPONENT_SCALER0) |\
					 BIT(KOMEDA_COMPONENT_SCALER1))

#define KOMEDA_PIPELINE_COMPIZS		(BIT(KOMEDA_COMPONENT_COMPIZ0) |\
					 BIT(KOMEDA_COMPONENT_COMPIZ1))

#define KOMEDA_PIPELINE_IMPROCS		(BIT(KOMEDA_COMPONENT_IPS0) |\
					 BIT(KOMEDA_COMPONENT_IPS1))
struct komeda_component;
struct komeda_component_state;

 
struct komeda_component_funcs {
	 
	int (*validate)(struct komeda_component *c,
			struct komeda_component_state *state);
	 
	void (*update)(struct komeda_component *c,
		       struct komeda_component_state *state);
	 
	void (*disable)(struct komeda_component *c);
	 
	void (*dump_register)(struct komeda_component *c, struct seq_file *seq);
};

 
struct komeda_component {
	 
	struct drm_private_obj obj;
	 
	struct komeda_pipeline *pipeline;
	 
	char name[32];
	 
	u32 __iomem *reg;
	 
	u32 id;
	 
	u32 hw_id;

	 
	u8 max_active_inputs;
	 
	u8 max_active_outputs;
	 
	u32 supported_inputs;
	 
	u32 supported_outputs;

	 
	const struct komeda_component_funcs *funcs;
};

 
struct komeda_component_output {
	 
	struct komeda_component *component;
	 
	u8 output_port;
};

 
struct komeda_component_state {
	 
	struct drm_private_state obj;
	 
	struct komeda_component *component;
	 
	union {
		 
		struct drm_crtc *crtc;
		 
		struct drm_plane *plane;
		 
		struct drm_connector *wb_conn;
		void *binding_user;
	};

	 
	u16 active_inputs;
	 
	u16 changed_active_inputs;
	 
	u16 affected_inputs;
	 
	struct komeda_component_output inputs[KOMEDA_COMPONENT_N_INPUTS];
};

static inline u16 component_disabling_inputs(struct komeda_component_state *st)
{
	return st->affected_inputs ^ st->active_inputs;
}

static inline u16 component_changed_inputs(struct komeda_component_state *st)
{
	return component_disabling_inputs(st) | st->changed_active_inputs;
}

#define for_each_changed_input(st, i)	\
	for ((i) = 0; (i) < (st)->component->max_active_inputs; (i)++)	\
		if (has_bit((i), component_changed_inputs(st)))

#define to_comp(__c)	(((__c) == NULL) ? NULL : &((__c)->base))
#define to_cpos(__c)	((struct komeda_component **)&(__c))

struct komeda_layer {
	struct komeda_component base;
	 
	struct malidp_range hsize_in, vsize_in;
	u32 layer_type;  
	u32 line_sz;
	u32 yuv_line_sz;  
	u32 supported_rots;
	 
	struct komeda_layer *right;
};

struct komeda_layer_state {
	struct komeda_component_state base;
	 
	u16 hsize, vsize;
	u32 rot;
	u16 afbc_crop_l;
	u16 afbc_crop_r;
	u16 afbc_crop_t;
	u16 afbc_crop_b;
	dma_addr_t addr[3];
};

struct komeda_scaler {
	struct komeda_component base;
	struct malidp_range hsize, vsize;
	u32 max_upscaling;
	u32 max_downscaling;
	u8 scaling_split_overlap;  
	u8 enh_split_overlap;  
};

struct komeda_scaler_state {
	struct komeda_component_state base;
	u16 hsize_in, vsize_in;
	u16 hsize_out, vsize_out;
	u16 total_hsize_in, total_vsize_in;
	u16 total_hsize_out;  
	u16 left_crop, right_crop;
	u8 en_scaling : 1,
	   en_alpha : 1,  
	   en_img_enhancement : 1,
	   en_split : 1,
	   right_part : 1;  
};

struct komeda_compiz {
	struct komeda_component base;
	struct malidp_range hsize, vsize;
};

struct komeda_compiz_input_cfg {
	u16 hsize, vsize;
	u16 hoffset, voffset;
	u8 pixel_blend_mode, layer_alpha;
};

struct komeda_compiz_state {
	struct komeda_component_state base;
	 
	u16 hsize, vsize;
	struct komeda_compiz_input_cfg cins[KOMEDA_COMPONENT_N_INPUTS];
};

struct komeda_merger {
	struct komeda_component base;
	struct malidp_range hsize_merged;
	struct malidp_range vsize_merged;
};

struct komeda_merger_state {
	struct komeda_component_state base;
	u16 hsize_merged;
	u16 vsize_merged;
};

struct komeda_splitter {
	struct komeda_component base;
	struct malidp_range hsize, vsize;
};

struct komeda_splitter_state {
	struct komeda_component_state base;
	u16 hsize, vsize;
	u16 overlap;
};

struct komeda_improc {
	struct komeda_component base;
	u32 supported_color_formats;   
	u32 supported_color_depths;  
	u8 supports_degamma : 1;
	u8 supports_csc : 1;
	u8 supports_gamma : 1;
};

struct komeda_improc_state {
	struct komeda_component_state base;
	u8 color_format, color_depth;
	u16 hsize, vsize;
	u32 fgamma_coeffs[KOMEDA_N_GAMMA_COEFFS];
	u32 ctm_coeffs[KOMEDA_N_CTM_COEFFS];
};

 
struct komeda_timing_ctrlr {
	struct komeda_component base;
	u8 supports_dual_link : 1;
};

struct komeda_timing_ctrlr_state {
	struct komeda_component_state base;
};

 
struct komeda_data_flow_cfg {
	struct komeda_component_output input;
	u16 in_x, in_y, in_w, in_h;
	u32 out_x, out_y, out_w, out_h;
	u16 total_in_h, total_in_w;
	u16 total_out_w;
	u16 left_crop, right_crop, overlap;
	u32 rot;
	int blending_zorder;
	u8 pixel_blend_mode, layer_alpha;
	u8 en_scaling : 1,
	   en_img_enhancement : 1,
	   en_split : 1,
	   is_yuv : 1,
	   right_part : 1;  
};

struct komeda_pipeline_funcs {
	 
	int (*downscaling_clk_check)(struct komeda_pipeline *pipe,
				     struct drm_display_mode *mode,
				     unsigned long aclk_rate,
				     struct komeda_data_flow_cfg *dflow);
	 
	void (*dump_register)(struct komeda_pipeline *pipe,
			      struct seq_file *sf);
};

 
struct komeda_pipeline {
	 
	struct drm_private_obj obj;
	 
	struct komeda_dev *mdev;
	 
	struct clk *pxlclk;
	 
	int id;
	 
	u32 avail_comps;
	 
	u32 standalone_disabled_comps;
	 
	int n_layers;
	 
	struct komeda_layer *layers[KOMEDA_PIPELINE_MAX_LAYERS];
	 
	int n_scalers;
	 
	struct komeda_scaler *scalers[KOMEDA_PIPELINE_MAX_SCALERS];
	 
	struct komeda_compiz *compiz;
	 
	struct komeda_splitter *splitter;
	 
	struct komeda_merger *merger;
	 
	struct komeda_layer  *wb_layer;
	 
	struct komeda_improc *improc;
	 
	struct komeda_timing_ctrlr *ctrlr;
	 
	const struct komeda_pipeline_funcs *funcs;

	 
	struct device_node *of_node;
	 
	struct device_node *of_output_port;
	 
	struct device_node *of_output_links[2];
	 
	bool dual_link;
};

 
struct komeda_pipeline_state {
	 
	struct drm_private_state obj;
	 
	struct komeda_pipeline *pipe;
	 
	struct drm_crtc *crtc;
	 
	u32 active_comps;
};

#define to_layer(c)	container_of(c, struct komeda_layer, base)
#define to_compiz(c)	container_of(c, struct komeda_compiz, base)
#define to_scaler(c)	container_of(c, struct komeda_scaler, base)
#define to_splitter(c)	container_of(c, struct komeda_splitter, base)
#define to_merger(c)	container_of(c, struct komeda_merger, base)
#define to_improc(c)	container_of(c, struct komeda_improc, base)
#define to_ctrlr(c)	container_of(c, struct komeda_timing_ctrlr, base)

#define to_layer_st(c)	container_of(c, struct komeda_layer_state, base)
#define to_compiz_st(c)	container_of(c, struct komeda_compiz_state, base)
#define to_scaler_st(c)	container_of(c, struct komeda_scaler_state, base)
#define to_splitter_st(c) container_of(c, struct komeda_splitter_state, base)
#define to_merger_st(c)	container_of(c, struct komeda_merger_state, base)
#define to_improc_st(c)	container_of(c, struct komeda_improc_state, base)
#define to_ctrlr_st(c)	container_of(c, struct komeda_timing_ctrlr_state, base)

#define priv_to_comp_st(o) container_of(o, struct komeda_component_state, obj)
#define priv_to_pipe_st(o) container_of(o, struct komeda_pipeline_state, obj)

 
struct komeda_pipeline *
komeda_pipeline_add(struct komeda_dev *mdev, size_t size,
		    const struct komeda_pipeline_funcs *funcs);
void komeda_pipeline_destroy(struct komeda_dev *mdev,
			     struct komeda_pipeline *pipe);
struct komeda_pipeline *
komeda_pipeline_get_slave(struct komeda_pipeline *master);
int komeda_assemble_pipelines(struct komeda_dev *mdev);
struct komeda_component *
komeda_pipeline_get_component(struct komeda_pipeline *pipe, int id);
struct komeda_component *
komeda_pipeline_get_first_component(struct komeda_pipeline *pipe,
				    u32 comp_mask);

void komeda_pipeline_dump_register(struct komeda_pipeline *pipe,
				   struct seq_file *sf);

 
extern __printf(10, 11)
struct komeda_component *
komeda_component_add(struct komeda_pipeline *pipe,
		     size_t comp_sz, u32 id, u32 hw_id,
		     const struct komeda_component_funcs *funcs,
		     u8 max_active_inputs, u32 supported_inputs,
		     u8 max_active_outputs, u32 __iomem *reg,
		     const char *name_fmt, ...);

void komeda_component_destroy(struct komeda_dev *mdev,
			      struct komeda_component *c);

static inline struct komeda_component *
komeda_component_pickup_output(struct komeda_component *c, u32 avail_comps)
{
	u32 avail_inputs = c->supported_outputs & (avail_comps);

	return komeda_pipeline_get_first_component(c->pipeline, avail_inputs);
}

struct komeda_plane_state;
struct komeda_crtc_state;
struct komeda_crtc;

void pipeline_composition_size(struct komeda_crtc_state *kcrtc_st,
			       u16 *hsize, u16 *vsize);

int komeda_build_layer_data_flow(struct komeda_layer *layer,
				 struct komeda_plane_state *kplane_st,
				 struct komeda_crtc_state *kcrtc_st,
				 struct komeda_data_flow_cfg *dflow);
int komeda_build_wb_data_flow(struct komeda_layer *wb_layer,
			      struct drm_connector_state *conn_st,
			      struct komeda_crtc_state *kcrtc_st,
			      struct komeda_data_flow_cfg *dflow);
int komeda_build_display_data_flow(struct komeda_crtc *kcrtc,
				   struct komeda_crtc_state *kcrtc_st);

int komeda_build_layer_split_data_flow(struct komeda_layer *left,
				       struct komeda_plane_state *kplane_st,
				       struct komeda_crtc_state *kcrtc_st,
				       struct komeda_data_flow_cfg *dflow);
int komeda_build_wb_split_data_flow(struct komeda_layer *wb_layer,
				    struct drm_connector_state *conn_st,
				    struct komeda_crtc_state *kcrtc_st,
				    struct komeda_data_flow_cfg *dflow);

int komeda_release_unclaimed_resources(struct komeda_pipeline *pipe,
				       struct komeda_crtc_state *kcrtc_st);

struct komeda_pipeline_state *
komeda_pipeline_get_old_state(struct komeda_pipeline *pipe,
			      struct drm_atomic_state *state);
bool komeda_pipeline_disable(struct komeda_pipeline *pipe,
			     struct drm_atomic_state *old_state);
void komeda_pipeline_update(struct komeda_pipeline *pipe,
			    struct drm_atomic_state *old_state);

void komeda_complete_data_flow_cfg(struct komeda_layer *layer,
				   struct komeda_data_flow_cfg *dflow,
				   struct drm_framebuffer *fb);

#endif  
