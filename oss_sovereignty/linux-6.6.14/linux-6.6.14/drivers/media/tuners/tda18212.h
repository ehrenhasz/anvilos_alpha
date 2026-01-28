#ifndef TDA18212_H
#define TDA18212_H
#include <media/dvb_frontend.h>
struct tda18212_config {
	u16 if_dvbt_6;
	u16 if_dvbt_7;
	u16 if_dvbt_8;
	u16 if_dvbt2_5;
	u16 if_dvbt2_6;
	u16 if_dvbt2_7;
	u16 if_dvbt2_8;
	u16 if_dvbc;
	u16 if_atsc_vsb;
	u16 if_atsc_qam;
	struct dvb_frontend *fe;
};
#endif
