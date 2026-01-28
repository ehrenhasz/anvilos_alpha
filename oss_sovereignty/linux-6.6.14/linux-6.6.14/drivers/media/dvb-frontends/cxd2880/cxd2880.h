#ifndef CXD2880_H
#define CXD2880_H
struct cxd2880_config {
	struct spi_device *spi;
	struct mutex *spi_mutex;  
};
#if IS_REACHABLE(CONFIG_DVB_CXD2880)
extern struct dvb_frontend *cxd2880_attach(struct dvb_frontend *fe,
					struct cxd2880_config *cfg);
#else
static inline struct dvb_frontend *cxd2880_attach(struct dvb_frontend *fe,
					struct cxd2880_config *cfg)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
#endif  
