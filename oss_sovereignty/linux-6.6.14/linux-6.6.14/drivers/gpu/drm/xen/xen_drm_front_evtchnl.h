#ifndef __XEN_DRM_FRONT_EVTCHNL_H_
#define __XEN_DRM_FRONT_EVTCHNL_H_
#include <linux/completion.h>
#include <linux/types.h>
#include <xen/interface/io/ring.h>
#include <xen/interface/io/displif.h>
#define GENERIC_OP_EVT_CHNL	0
enum xen_drm_front_evtchnl_state {
	EVTCHNL_STATE_DISCONNECTED,
	EVTCHNL_STATE_CONNECTED,
};
enum xen_drm_front_evtchnl_type {
	EVTCHNL_TYPE_REQ,
	EVTCHNL_TYPE_EVT,
};
struct xen_drm_front_drm_info;
struct xen_drm_front_evtchnl {
	struct xen_drm_front_info *front_info;
	int gref;
	int port;
	int irq;
	int index;
	enum xen_drm_front_evtchnl_state state;
	enum xen_drm_front_evtchnl_type type;
	u16 evt_id;
	u16 evt_next_id;
	union {
		struct {
			struct xen_displif_front_ring ring;
			struct completion completion;
			int resp_status;
			struct mutex req_io_lock;
		} req;
		struct {
			struct xendispl_event_page *page;
		} evt;
	} u;
};
struct xen_drm_front_evtchnl_pair {
	struct xen_drm_front_evtchnl req;
	struct xen_drm_front_evtchnl evt;
};
int xen_drm_front_evtchnl_create_all(struct xen_drm_front_info *front_info);
int xen_drm_front_evtchnl_publish_all(struct xen_drm_front_info *front_info);
void xen_drm_front_evtchnl_flush(struct xen_drm_front_evtchnl *evtchnl);
void xen_drm_front_evtchnl_set_state(struct xen_drm_front_info *front_info,
				     enum xen_drm_front_evtchnl_state state);
void xen_drm_front_evtchnl_free_all(struct xen_drm_front_info *front_info);
#endif  
