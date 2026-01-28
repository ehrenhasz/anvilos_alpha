#ifndef __CVMX_HELPER_BOARD_H__
#define __CVMX_HELPER_BOARD_H__
#include <asm/octeon/cvmx-helper.h>
enum cvmx_helper_board_usb_clock_types {
	USB_CLOCK_TYPE_REF_12,
	USB_CLOCK_TYPE_REF_24,
	USB_CLOCK_TYPE_REF_48,
	USB_CLOCK_TYPE_CRYSTAL_12,
};
typedef enum {
	set_phy_link_flags_autoneg = 0x1,
	set_phy_link_flags_flow_control_dont_touch = 0x0 << 1,
	set_phy_link_flags_flow_control_enable = 0x1 << 1,
	set_phy_link_flags_flow_control_disable = 0x2 << 1,
	set_phy_link_flags_flow_control_mask = 0x3 << 1,	 
} cvmx_helper_board_set_phy_link_flags_types_t;
#define CVMX_HELPER_BOARD_MGMT_IPD_PORT	    -10
extern int cvmx_helper_board_get_mii_address(int ipd_port);
extern union cvmx_helper_link_info __cvmx_helper_board_link_get(int ipd_port);
extern int __cvmx_helper_board_interface_probe(int interface,
					       int supported_ports);
enum cvmx_helper_board_usb_clock_types __cvmx_helper_board_usb_get_clock_type(void);
#endif  
