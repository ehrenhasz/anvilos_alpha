#ifndef _KOMEDA_KMS_H_
#define _KOMEDA_KMS_H_
#include <linux/list.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_blend.h>
#include <drm/drm_device.h>
#include <drm/drm_writeback.h>
#include <drm/drm_print.h>
struct komeda_plane {
	struct drm_plane base;
	struct komeda_layer *layer;
};
struct komeda_plane_state {
	struct drm_plane_state base;
	struct list_head zlist_node;
	u8 layer_split : 1;
};
struct komeda_wb_connector {
	struct drm_writeback_connector base;
	struct komeda_layer *wb_layer;
};
struct komeda_crtc {
	struct drm_crtc base;
	struct komeda_pipeline *master;
	struct komeda_pipeline *slave;
	u32 slave_planes;
	struct komeda_wb_connector *wb_conn;
	struct completion *disable_done;
	struct drm_encoder encoder;
};
struct komeda_crtc_state {
	struct drm_crtc_state base;
	u32 affected_pipes;
	u32 active_pipes;
	u64 clock_ratio;
	u32 max_slave_zorder;
};
struct komeda_kms_dev {
	struct drm_device base;
	int n_crtcs;
	struct komeda_crtc crtcs[KOMEDA_MAX_PIPELINES];
};
#define to_kplane(p)	container_of(p, struct komeda_plane, base)
#define to_kplane_st(p)	container_of(p, struct komeda_plane_state, base)
#define to_kconn(p)	container_of(p, struct komeda_wb_connector, base)
#define to_kcrtc(p)	container_of(p, struct komeda_crtc, base)
#define to_kcrtc_st(p)	container_of(p, struct komeda_crtc_state, base)
#define to_kdev(p)	container_of(p, struct komeda_kms_dev, base)
#define to_wb_conn(x)	container_of(x, struct drm_writeback_connector, base)
static inline bool is_writeback_only(struct drm_crtc_state *st)
{
	struct komeda_wb_connector *wb_conn = to_kcrtc(st->crtc)->wb_conn;
	struct drm_connector *conn = wb_conn ? &wb_conn->base.base : NULL;
	return conn && (st->connector_mask == BIT(drm_connector_index(conn)));
}
static inline bool
is_only_changed_connector(struct drm_crtc_state *st, struct drm_connector *conn)
{
	struct drm_crtc_state *old_st;
	u32 changed_connectors;
	old_st = drm_atomic_get_old_crtc_state(st->state, st->crtc);
	changed_connectors = st->connector_mask ^ old_st->connector_mask;
	return BIT(drm_connector_index(conn)) == changed_connectors;
}
static inline bool has_flip_h(u32 rot)
{
	u32 rotation = drm_rotation_simplify(rot,
					     DRM_MODE_ROTATE_0 |
					     DRM_MODE_ROTATE_90 |
					     DRM_MODE_REFLECT_MASK);
	if (rotation & DRM_MODE_ROTATE_90)
		return !!(rotation & DRM_MODE_REFLECT_Y);
	else
		return !!(rotation & DRM_MODE_REFLECT_X);
}
void komeda_crtc_get_color_config(struct drm_crtc_state *crtc_st,
				  u32 *color_depths, u32 *color_formats);
unsigned long komeda_crtc_get_aclk(struct komeda_crtc_state *kcrtc_st);
int komeda_kms_setup_crtcs(struct komeda_kms_dev *kms, struct komeda_dev *mdev);
int komeda_kms_add_crtcs(struct komeda_kms_dev *kms, struct komeda_dev *mdev);
int komeda_kms_add_planes(struct komeda_kms_dev *kms, struct komeda_dev *mdev);
int komeda_kms_add_private_objs(struct komeda_kms_dev *kms,
				struct komeda_dev *mdev);
int komeda_kms_add_wb_connectors(struct komeda_kms_dev *kms,
				 struct komeda_dev *mdev);
void komeda_kms_cleanup_private_objs(struct komeda_kms_dev *kms);
void komeda_crtc_handle_event(struct komeda_crtc   *kcrtc,
			      struct komeda_events *evts);
void komeda_crtc_flush_and_wait_for_flip_done(struct komeda_crtc *kcrtc,
					      struct completion *input_flip_done);
struct komeda_kms_dev *komeda_kms_attach(struct komeda_dev *mdev);
void komeda_kms_detach(struct komeda_kms_dev *kms);
#endif  
