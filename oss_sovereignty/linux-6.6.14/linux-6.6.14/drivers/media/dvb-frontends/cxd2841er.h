#ifndef CXD2841ER_H
#define CXD2841ER_H
#include <linux/dvb/frontend.h>
#define CXD2841ER_USE_GATECTRL	1	 
#define CXD2841ER_AUTO_IFHZ	2	 
#define CXD2841ER_TS_SERIAL	4	 
#define CXD2841ER_ASCOT		8	 
#define CXD2841ER_EARLY_TUNE	16	 
#define CXD2841ER_NO_WAIT_LOCK	32	 
#define CXD2841ER_NO_AGCNEG	64	 
#define CXD2841ER_TSBITS	128	 
enum cxd2841er_xtal {
	SONY_XTAL_20500,  
	SONY_XTAL_24000,  
	SONY_XTAL_41000  
};
struct cxd2841er_config {
	u8	i2c_addr;
	enum cxd2841er_xtal	xtal;
	u32	flags;
};
#if IS_REACHABLE(CONFIG_DVB_CXD2841ER)
extern struct dvb_frontend *cxd2841er_attach_s(struct cxd2841er_config *cfg,
					       struct i2c_adapter *i2c);
extern struct dvb_frontend *cxd2841er_attach_t_c(struct cxd2841er_config *cfg,
					       struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *cxd2841er_attach_s(
					struct cxd2841er_config *cfg,
					struct i2c_adapter *i2c)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
static inline struct dvb_frontend *cxd2841er_attach_t_c(
		struct cxd2841er_config *cfg, struct i2c_adapter *i2c)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif
#endif
