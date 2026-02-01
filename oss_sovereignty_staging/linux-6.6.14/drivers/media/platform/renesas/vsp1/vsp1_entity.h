 
 
#ifndef __VSP1_ENTITY_H__
#define __VSP1_ENTITY_H__

#include <linux/list.h>
#include <linux/mutex.h>

#include <media/v4l2-subdev.h>

struct vsp1_device;
struct vsp1_dl_body;
struct vsp1_dl_list;
struct vsp1_pipeline;
struct vsp1_partition;
struct vsp1_partition_window;

enum vsp1_entity_type {
	VSP1_ENTITY_BRS,
	VSP1_ENTITY_BRU,
	VSP1_ENTITY_CLU,
	VSP1_ENTITY_HGO,
	VSP1_ENTITY_HGT,
	VSP1_ENTITY_HSI,
	VSP1_ENTITY_HST,
	VSP1_ENTITY_LIF,
	VSP1_ENTITY_LUT,
	VSP1_ENTITY_RPF,
	VSP1_ENTITY_SRU,
	VSP1_ENTITY_UDS,
	VSP1_ENTITY_UIF,
	VSP1_ENTITY_WPF,
};

#define VSP1_ENTITY_MAX_INPUTS		5	 

 
struct vsp1_route {
	enum vsp1_entity_type type;
	unsigned int index;
	unsigned int reg;
	unsigned int inputs[VSP1_ENTITY_MAX_INPUTS];
	unsigned int output;
};

 
struct vsp1_entity_operations {
	void (*destroy)(struct vsp1_entity *);
	void (*configure_stream)(struct vsp1_entity *, struct vsp1_pipeline *,
				 struct vsp1_dl_list *, struct vsp1_dl_body *);
	void (*configure_frame)(struct vsp1_entity *, struct vsp1_pipeline *,
				struct vsp1_dl_list *, struct vsp1_dl_body *);
	void (*configure_partition)(struct vsp1_entity *,
				    struct vsp1_pipeline *,
				    struct vsp1_dl_list *,
				    struct vsp1_dl_body *);
	unsigned int (*max_width)(struct vsp1_entity *, struct vsp1_pipeline *);
	void (*partition)(struct vsp1_entity *, struct vsp1_pipeline *,
			  struct vsp1_partition *, unsigned int,
			  struct vsp1_partition_window *);
};

struct vsp1_entity {
	struct vsp1_device *vsp1;

	const struct vsp1_entity_operations *ops;

	enum vsp1_entity_type type;
	unsigned int index;
	const struct vsp1_route *route;

	struct vsp1_pipeline *pipe;

	struct list_head list_dev;
	struct list_head list_pipe;

	struct media_pad *pads;
	unsigned int source_pad;

	struct vsp1_entity **sources;
	struct vsp1_entity *sink;
	unsigned int sink_pad;

	struct v4l2_subdev subdev;
	struct v4l2_subdev_state *config;

	struct mutex lock;	 
};

static inline struct vsp1_entity *to_vsp1_entity(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct vsp1_entity, subdev);
}

int vsp1_entity_init(struct vsp1_device *vsp1, struct vsp1_entity *entity,
		     const char *name, unsigned int num_pads,
		     const struct v4l2_subdev_ops *ops, u32 function);
void vsp1_entity_destroy(struct vsp1_entity *entity);

int vsp1_entity_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote, u32 flags);

struct v4l2_subdev_state *
vsp1_entity_get_pad_config(struct vsp1_entity *entity,
			   struct v4l2_subdev_state *sd_state,
			   enum v4l2_subdev_format_whence which);
struct v4l2_mbus_framefmt *
vsp1_entity_get_pad_format(struct vsp1_entity *entity,
			   struct v4l2_subdev_state *sd_state,
			   unsigned int pad);
struct v4l2_rect *
vsp1_entity_get_pad_selection(struct vsp1_entity *entity,
			      struct v4l2_subdev_state *sd_state,
			      unsigned int pad, unsigned int target);
int vsp1_entity_init_cfg(struct v4l2_subdev *subdev,
			 struct v4l2_subdev_state *sd_state);

void vsp1_entity_route_setup(struct vsp1_entity *entity,
			     struct vsp1_pipeline *pipe,
			     struct vsp1_dl_body *dlb);

void vsp1_entity_configure_stream(struct vsp1_entity *entity,
				  struct vsp1_pipeline *pipe,
				  struct vsp1_dl_list *dl,
				  struct vsp1_dl_body *dlb);

void vsp1_entity_configure_frame(struct vsp1_entity *entity,
				 struct vsp1_pipeline *pipe,
				 struct vsp1_dl_list *dl,
				 struct vsp1_dl_body *dlb);

void vsp1_entity_configure_partition(struct vsp1_entity *entity,
				     struct vsp1_pipeline *pipe,
				     struct vsp1_dl_list *dl,
				     struct vsp1_dl_body *dlb);

struct media_pad *vsp1_entity_remote_pad(struct media_pad *pad);

int vsp1_subdev_get_pad_format(struct v4l2_subdev *subdev,
			       struct v4l2_subdev_state *sd_state,
			       struct v4l2_subdev_format *fmt);
int vsp1_subdev_set_pad_format(struct v4l2_subdev *subdev,
			       struct v4l2_subdev_state *sd_state,
			       struct v4l2_subdev_format *fmt,
			       const unsigned int *codes, unsigned int ncodes,
			       unsigned int min_width, unsigned int min_height,
			       unsigned int max_width, unsigned int max_height);
int vsp1_subdev_enum_mbus_code(struct v4l2_subdev *subdev,
			       struct v4l2_subdev_state *sd_state,
			       struct v4l2_subdev_mbus_code_enum *code,
			       const unsigned int *codes, unsigned int ncodes);
int vsp1_subdev_enum_frame_size(struct v4l2_subdev *subdev,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_frame_size_enum *fse,
				unsigned int min_w, unsigned int min_h,
				unsigned int max_w, unsigned int max_h);

#endif  
