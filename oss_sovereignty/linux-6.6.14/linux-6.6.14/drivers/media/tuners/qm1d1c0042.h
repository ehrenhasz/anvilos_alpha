#ifndef QM1D1C0042_H
#define QM1D1C0042_H
#include <media/dvb_frontend.h>
struct qm1d1c0042_config {
	struct dvb_frontend *fe;
	u32  xtal_freq;       
	bool lpf;           
	bool fast_srch;     
	u32  lpf_wait;          
	u32  fast_srch_wait;    
	u32  normal_srch_wait;  
};
#define QM1D1C0042_CFG_XTAL_DFLT 0
#define QM1D1C0042_CFG_WAIT_DFLT 0
#endif  
