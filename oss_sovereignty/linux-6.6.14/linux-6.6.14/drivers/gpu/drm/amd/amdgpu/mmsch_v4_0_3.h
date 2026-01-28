#ifndef __MMSCH_V4_0_3_H__
#define __MMSCH_V4_0_3_H__
#include "amdgpu_vcn.h"
#include "mmsch_v4_0.h"
struct mmsch_v4_0_3_init_header {
	uint32_t version;
	uint32_t total_size;
	struct mmsch_v4_0_table_info vcn0;
	struct mmsch_v4_0_table_info mjpegdec0[4];
	struct mmsch_v4_0_table_info mjpegdec1[4];
};
#endif
