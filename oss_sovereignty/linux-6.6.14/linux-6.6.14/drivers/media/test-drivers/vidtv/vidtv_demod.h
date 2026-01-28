#ifndef VIDTV_DEMOD_H
#define VIDTV_DEMOD_H
#include <linux/dvb/frontend.h>
#include <media/dvb_frontend.h>
struct vidtv_demod_cnr_to_qual_s {
	u32 modulation;
	u32 fec;
	u32 cnr_ok;
	u32 cnr_good;
};
struct vidtv_demod_config {
	u8 drop_tslock_prob_on_low_snr;
	u8 recover_tslock_prob_on_good_snr;
};
struct vidtv_demod_state {
	struct dvb_frontend frontend;
	struct vidtv_demod_config config;
	enum fe_status status;
	u16 tuner_cnr;
};
#endif  
