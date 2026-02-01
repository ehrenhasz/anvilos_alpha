 
 

#ifndef __ECHO_H
#define __ECHO_H

 

#include "fir.h"
#include "oslec.h"

 
struct oslec_state {
	int16_t tx;
	int16_t rx;
	int16_t clean;
	int16_t clean_nlp;

	int nonupdate_dwell;
	int curr_pos;
	int taps;
	int log2taps;
	int adaption_mode;

	int cond_met;
	int32_t pstates;
	int16_t adapt;
	int32_t factor;
	int16_t shift;

	 
	int ltxacc;
	int lrxacc;
	int lcleanacc;
	int lclean_bgacc;
	int ltx;
	int lrx;
	int lclean;
	int lclean_bg;
	int lbgn;
	int lbgn_acc;
	int lbgn_upper;
	int lbgn_upper_acc;

	 
	struct fir16_state_t fir_state;
	struct fir16_state_t fir_state_bg;
	int16_t *fir_taps16[2];

	 
	int tx_1;
	int tx_2;
	int rx_1;
	int rx_2;

	 
	int32_t xvtx[5];
	int32_t yvtx[5];
	int32_t xvrx[5];
	int32_t yvrx[5];

	 
	int cng_level;
	int cng_rndnum;
	int cng_filter;

	 
	int16_t *snapshot;
};

#endif  
