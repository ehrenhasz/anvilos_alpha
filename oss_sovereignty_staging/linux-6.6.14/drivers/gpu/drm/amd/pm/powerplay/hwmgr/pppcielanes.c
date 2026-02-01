 

#include <linux/types.h>
#include "atom-types.h"
#include "atombios.h"
#include "pppcielanes.h"

 

 
static const unsigned char pp_r600_encode_lanes[] = {
	0,           
	1,           
	2,           
	0,           
	3,           
	0,           
	0,           
	0,           
	4,           
	0,           
	0,           
	0,           
	5,           
	0,           
	0,           
	0,           
	6            
};

static const unsigned char pp_r600_decoded_lanes[8] = { 16, 1, 2, 4, 8, 12, 16, };

uint8_t encode_pcie_lane_width(uint32_t num_lanes)
{
	return pp_r600_encode_lanes[num_lanes];
}

uint8_t decode_pcie_lane_width(uint32_t num_lanes)
{
	return pp_r600_decoded_lanes[num_lanes];
}
