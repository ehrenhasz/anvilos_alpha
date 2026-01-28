#ifndef VIDTV_CHANNEL_H
#define VIDTV_CHANNEL_H
#include <linux/types.h>
#include "vidtv_encoder.h"
#include "vidtv_mux.h"
#include "vidtv_psi.h"
struct vidtv_channel {
	char *name;
	u16 transport_stream_id;
	struct vidtv_psi_table_sdt_service *service;
	u16 program_num;
	struct vidtv_psi_table_pat_program *program;
	struct vidtv_psi_table_pmt_stream *streams;
	struct vidtv_encoder *encoders;
	struct vidtv_psi_table_eit_event *events;
	struct vidtv_channel *next;
};
int vidtv_channel_si_init(struct vidtv_mux *m);
void vidtv_channel_si_destroy(struct vidtv_mux *m);
int vidtv_channels_init(struct vidtv_mux *m);
struct vidtv_channel
*vidtv_channel_s302m_init(struct vidtv_channel *head, u16 transport_stream_id);
void vidtv_channels_destroy(struct vidtv_mux *m);
#endif  
