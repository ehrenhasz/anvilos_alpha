#ifndef __CVMX_HELPER_NPI_H__
#define __CVMX_HELPER_NPI_H__
extern int __cvmx_helper_npi_probe(int interface);
#define __cvmx_helper_npi_enumerate __cvmx_helper_npi_probe
extern int __cvmx_helper_npi_enable(int interface);
#endif
