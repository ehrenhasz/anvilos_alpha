 
 

#ifndef VIDTV_TUNER_H
#define VIDTV_TUNER_H

#include <linux/types.h>

#include <media/dvb_frontend.h>

#define NUM_VALID_TUNER_FREQS 8

 
struct vidtv_tuner_config {
	struct dvb_frontend *fe;
	u32 mock_power_up_delay_msec;
	u32 mock_tune_delay_msec;
	u32 vidtv_valid_dvb_t_freqs[NUM_VALID_TUNER_FREQS];
	u32 vidtv_valid_dvb_c_freqs[NUM_VALID_TUNER_FREQS];
	u32 vidtv_valid_dvb_s_freqs[NUM_VALID_TUNER_FREQS];
	u8  max_frequency_shift_hz;
};

#endif 
