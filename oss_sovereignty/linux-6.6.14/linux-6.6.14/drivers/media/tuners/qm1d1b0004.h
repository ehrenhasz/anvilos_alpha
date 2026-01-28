#ifndef QM1D1B0004_H
#define QM1D1B0004_H
#include <media/dvb_frontend.h>
struct qm1d1b0004_config {
	struct dvb_frontend *fe;
	u32 lpf_freq;    
	bool half_step;  
};
#define QM1D1B0004_CFG_PLL_DFLT 0
#define QM1D1B0004_CFG_LPF_DFLT 0
#endif
