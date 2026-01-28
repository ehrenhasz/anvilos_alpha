#include "system_global.h"
#ifndef __IA_CSS_INPUT_PORT_H
#define __IA_CSS_INPUT_PORT_H
#define	IA_CSS_CSI2_PORT_4LANE MIPI_PORT0_ID
#define	IA_CSS_CSI2_PORT_1LANE MIPI_PORT1_ID
#define	IA_CSS_CSI2_PORT_2LANE MIPI_PORT2_ID
enum ia_css_csi2_compression_type {
	IA_CSS_CSI2_COMPRESSION_TYPE_NONE,  
	IA_CSS_CSI2_COMPRESSION_TYPE_1,     
	IA_CSS_CSI2_COMPRESSION_TYPE_2      
};
struct ia_css_csi2_compression {
	enum ia_css_csi2_compression_type type;
	unsigned int                      compressed_bits_per_pixel;
	unsigned int                      uncompressed_bits_per_pixel;
};
struct ia_css_input_port {
	enum mipi_port_id port;  
	unsigned int num_lanes;  
	unsigned int timeout;    
	unsigned int rxcount;    
	struct ia_css_csi2_compression compression;  
};
#endif  
