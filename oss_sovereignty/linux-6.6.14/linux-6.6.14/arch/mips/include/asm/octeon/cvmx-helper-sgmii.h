#ifndef __CVMX_HELPER_SGMII_H__
#define __CVMX_HELPER_SGMII_H__
extern int __cvmx_helper_sgmii_probe(int interface);
extern int __cvmx_helper_sgmii_enumerate(int interface);
extern int __cvmx_helper_sgmii_enable(int interface);
extern union cvmx_helper_link_info __cvmx_helper_sgmii_link_get(int ipd_port);
extern int __cvmx_helper_sgmii_link_set(int ipd_port,
					union cvmx_helper_link_info link_info);
#endif
