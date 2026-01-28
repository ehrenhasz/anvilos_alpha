#ifndef __CVMX_HELPER_RGMII_H__
#define __CVMX_HELPER_RGMII_H__
extern int __cvmx_helper_rgmii_probe(int interface);
#define __cvmx_helper_rgmii_enumerate __cvmx_helper_rgmii_probe
extern void cvmx_helper_rgmii_internal_loopback(int port);
extern int __cvmx_helper_rgmii_enable(int interface);
extern union cvmx_helper_link_info __cvmx_helper_rgmii_link_get(int ipd_port);
extern int __cvmx_helper_rgmii_link_set(int ipd_port,
					union cvmx_helper_link_info link_info);
#endif
