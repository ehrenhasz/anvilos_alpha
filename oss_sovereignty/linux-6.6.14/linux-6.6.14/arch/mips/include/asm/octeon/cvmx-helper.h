#ifndef __CVMX_HELPER_H__
#define __CVMX_HELPER_H__
#include <asm/octeon/cvmx-config.h>
#include <asm/octeon/cvmx-fpa.h>
#include <asm/octeon/cvmx-wqe.h>
typedef enum {
	CVMX_HELPER_INTERFACE_MODE_DISABLED,
	CVMX_HELPER_INTERFACE_MODE_RGMII,
	CVMX_HELPER_INTERFACE_MODE_GMII,
	CVMX_HELPER_INTERFACE_MODE_SPI,
	CVMX_HELPER_INTERFACE_MODE_PCIE,
	CVMX_HELPER_INTERFACE_MODE_XAUI,
	CVMX_HELPER_INTERFACE_MODE_SGMII,
	CVMX_HELPER_INTERFACE_MODE_PICMG,
	CVMX_HELPER_INTERFACE_MODE_NPI,
	CVMX_HELPER_INTERFACE_MODE_LOOP,
} cvmx_helper_interface_mode_t;
union cvmx_helper_link_info {
	uint64_t u64;
	struct {
		uint64_t reserved_20_63:44;
		uint64_t link_up:1;	     
		uint64_t full_duplex:1;	     
		uint64_t speed:18;	     
	} s;
};
#include <asm/octeon/cvmx-helper-errata.h>
#include <asm/octeon/cvmx-helper-loop.h>
#include <asm/octeon/cvmx-helper-npi.h>
#include <asm/octeon/cvmx-helper-rgmii.h>
#include <asm/octeon/cvmx-helper-sgmii.h>
#include <asm/octeon/cvmx-helper-spi.h>
#include <asm/octeon/cvmx-helper-util.h>
#include <asm/octeon/cvmx-helper-xaui.h>
extern int cvmx_helper_ipd_and_packet_input_enable(void);
extern int cvmx_helper_initialize_packet_io_global(void);
extern int cvmx_helper_ports_on_interface(int interface);
extern int cvmx_helper_get_number_of_interfaces(void);
extern cvmx_helper_interface_mode_t cvmx_helper_interface_get_mode(int
								   interface);
extern union cvmx_helper_link_info cvmx_helper_link_get(int ipd_port);
extern int cvmx_helper_link_set(int ipd_port,
				union cvmx_helper_link_info link_info);
extern int cvmx_helper_interface_probe(int interface);
extern int cvmx_helper_interface_enumerate(int interface);
#endif  
