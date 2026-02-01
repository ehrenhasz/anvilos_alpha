 
 

#ifndef __RCAR_DU_PLANE_H__
#define __RCAR_DU_PLANE_H__

#include <drm/drm_plane.h>

struct rcar_du_format_info;
struct rcar_du_group;

 
#define RCAR_DU_NUM_KMS_PLANES		9
#define RCAR_DU_NUM_HW_PLANES		8

enum rcar_du_plane_source {
	RCAR_DU_PLANE_MEMORY,
	RCAR_DU_PLANE_VSPD0,
	RCAR_DU_PLANE_VSPD1,
};

struct rcar_du_plane {
	struct drm_plane plane;
	struct rcar_du_group *group;
};

static inline struct rcar_du_plane *to_rcar_plane(struct drm_plane *plane)
{
	return container_of(plane, struct rcar_du_plane, plane);
}

 
struct rcar_du_plane_state {
	struct drm_plane_state state;

	const struct rcar_du_format_info *format;
	int hwindex;
	enum rcar_du_plane_source source;

	unsigned int colorkey;
};

static inline struct rcar_du_plane_state *
to_rcar_plane_state(struct drm_plane_state *state)
{
	return container_of(state, struct rcar_du_plane_state, state);
}

int rcar_du_atomic_check_planes(struct drm_device *dev,
				struct drm_atomic_state *state);

int __rcar_du_plane_atomic_check(struct drm_plane *plane,
				 struct drm_plane_state *state,
				 const struct rcar_du_format_info **format);

int rcar_du_planes_init(struct rcar_du_group *rgrp);

void __rcar_du_plane_setup(struct rcar_du_group *rgrp,
			   const struct rcar_du_plane_state *state);

static inline void rcar_du_plane_setup(struct rcar_du_plane *plane)
{
	struct rcar_du_plane_state *state =
		to_rcar_plane_state(plane->plane.state);

	return __rcar_du_plane_setup(plane->group, state);
}

#endif  
