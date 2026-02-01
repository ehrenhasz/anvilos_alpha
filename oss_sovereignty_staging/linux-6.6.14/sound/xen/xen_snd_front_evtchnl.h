 

 

#ifndef __XEN_SND_FRONT_EVTCHNL_H
#define __XEN_SND_FRONT_EVTCHNL_H

#include <xen/interface/io/sndif.h>

struct xen_snd_front_info;

 
#define VSND_WAIT_BACK_MS	3000

enum xen_snd_front_evtchnl_state {
	EVTCHNL_STATE_DISCONNECTED,
	EVTCHNL_STATE_CONNECTED,
};

enum xen_snd_front_evtchnl_type {
	EVTCHNL_TYPE_REQ,
	EVTCHNL_TYPE_EVT,
};

struct xen_snd_front_evtchnl {
	struct xen_snd_front_info *front_info;
	int gref;
	int port;
	int irq;
	int index;
	 
	enum xen_snd_front_evtchnl_state state;
	enum xen_snd_front_evtchnl_type type;
	 
	u16 evt_id;
	 
	u16 evt_next_id;
	 
	struct mutex ring_io_lock;
	union {
		struct {
			struct xen_sndif_front_ring ring;
			struct completion completion;
			 
			struct mutex req_io_lock;

			 
			int resp_status;
			union {
				struct xensnd_query_hw_param hw_param;
			} resp;
		} req;
		struct {
			struct xensnd_event_page *page;
			 
			struct snd_pcm_substream *substream;
		} evt;
	} u;
};

struct xen_snd_front_evtchnl_pair {
	struct xen_snd_front_evtchnl req;
	struct xen_snd_front_evtchnl evt;
};

int xen_snd_front_evtchnl_create_all(struct xen_snd_front_info *front_info,
				     int num_streams);

void xen_snd_front_evtchnl_free_all(struct xen_snd_front_info *front_info);

int xen_snd_front_evtchnl_publish_all(struct xen_snd_front_info *front_info);

void xen_snd_front_evtchnl_flush(struct xen_snd_front_evtchnl *evtchnl);

void xen_snd_front_evtchnl_pair_set_connected(struct xen_snd_front_evtchnl_pair *evt_pair,
					      bool is_connected);

void xen_snd_front_evtchnl_pair_clear(struct xen_snd_front_evtchnl_pair *evt_pair);

#endif  
