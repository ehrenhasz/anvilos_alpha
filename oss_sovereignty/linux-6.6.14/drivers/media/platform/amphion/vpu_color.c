
 

#include <linux/init.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <media/v4l2-device.h>
#include "vpu.h"
#include "vpu_helpers.h"

static const u8 colorprimaries[] = {
	V4L2_COLORSPACE_LAST,
	V4L2_COLORSPACE_REC709,          
	0,
	0,
	V4L2_COLORSPACE_470_SYSTEM_M,    
	V4L2_COLORSPACE_470_SYSTEM_BG,   
	V4L2_COLORSPACE_SMPTE170M,       
	V4L2_COLORSPACE_SMPTE240M,       
	0,                               
	V4L2_COLORSPACE_BT2020,          
	0,                               
};

static const u8 colortransfers[] = {
	V4L2_XFER_FUNC_LAST,
	V4L2_XFER_FUNC_709,              
	0,
	0,
	0,                               
	0,                               
	V4L2_XFER_FUNC_709,              
	V4L2_XFER_FUNC_SMPTE240M,        
	V4L2_XFER_FUNC_NONE,             
	0,
	0,
	0,                               
	0,                               
	V4L2_XFER_FUNC_SRGB,             
	V4L2_XFER_FUNC_709,              
	V4L2_XFER_FUNC_709,              
	V4L2_XFER_FUNC_SMPTE2084,        
	0,                               
	0                                
};

static const u8 colormatrixcoefs[] = {
	V4L2_YCBCR_ENC_LAST,
	V4L2_YCBCR_ENC_709,               
	0,
	0,
	0,                                
	V4L2_YCBCR_ENC_601,               
	V4L2_YCBCR_ENC_601,               
	V4L2_YCBCR_ENC_SMPTE240M,         
	0,
	V4L2_YCBCR_ENC_BT2020,            
	V4L2_YCBCR_ENC_BT2020_CONST_LUM   
};

u32 vpu_color_cvrt_primaries_v2i(u32 primaries)
{
	return vpu_helper_find_in_array_u8(colorprimaries, ARRAY_SIZE(colorprimaries), primaries);
}

u32 vpu_color_cvrt_primaries_i2v(u32 primaries)
{
	return primaries < ARRAY_SIZE(colorprimaries) ? colorprimaries[primaries] : 0;
}

u32 vpu_color_cvrt_transfers_v2i(u32 transfers)
{
	return vpu_helper_find_in_array_u8(colortransfers, ARRAY_SIZE(colortransfers), transfers);
}

u32 vpu_color_cvrt_transfers_i2v(u32 transfers)
{
	return transfers < ARRAY_SIZE(colortransfers) ? colortransfers[transfers] : 0;
}

u32 vpu_color_cvrt_matrix_v2i(u32 matrix)
{
	return vpu_helper_find_in_array_u8(colormatrixcoefs, ARRAY_SIZE(colormatrixcoefs), matrix);
}

u32 vpu_color_cvrt_matrix_i2v(u32 matrix)
{
	return matrix < ARRAY_SIZE(colormatrixcoefs) ? colormatrixcoefs[matrix] : 0;
}

u32 vpu_color_cvrt_full_range_v2i(u32 full_range)
{
	return (full_range == V4L2_QUANTIZATION_FULL_RANGE);
}

u32 vpu_color_cvrt_full_range_i2v(u32 full_range)
{
	if (full_range)
		return V4L2_QUANTIZATION_FULL_RANGE;

	return V4L2_QUANTIZATION_LIM_RANGE;
}

int vpu_color_check_primaries(u32 primaries)
{
	return vpu_color_cvrt_primaries_v2i(primaries) ? 0 : -EINVAL;
}

int vpu_color_check_transfers(u32 transfers)
{
	return vpu_color_cvrt_transfers_v2i(transfers) ? 0 : -EINVAL;
}

int vpu_color_check_matrix(u32 matrix)
{
	return vpu_color_cvrt_matrix_v2i(matrix) ? 0 : -EINVAL;
}

int vpu_color_check_full_range(u32 full_range)
{
	int ret = -EINVAL;

	switch (full_range) {
	case V4L2_QUANTIZATION_FULL_RANGE:
	case V4L2_QUANTIZATION_LIM_RANGE:
		ret = 0;
		break;
	default:
		break;
	}

	return ret;
}

int vpu_color_get_default(u32 primaries, u32 *ptransfers, u32 *pmatrix, u32 *pfull_range)
{
	u32 transfers;
	u32 matrix;
	u32 full_range;

	switch (primaries) {
	case V4L2_COLORSPACE_REC709:
		transfers = V4L2_XFER_FUNC_709;
		matrix = V4L2_YCBCR_ENC_709;
		break;
	case V4L2_COLORSPACE_470_SYSTEM_M:
	case V4L2_COLORSPACE_470_SYSTEM_BG:
	case V4L2_COLORSPACE_SMPTE170M:
		transfers = V4L2_XFER_FUNC_709;
		matrix = V4L2_YCBCR_ENC_601;
		break;
	case V4L2_COLORSPACE_SMPTE240M:
		transfers = V4L2_XFER_FUNC_SMPTE240M;
		matrix = V4L2_YCBCR_ENC_SMPTE240M;
		break;
	case V4L2_COLORSPACE_BT2020:
		transfers = V4L2_XFER_FUNC_709;
		matrix = V4L2_YCBCR_ENC_BT2020;
		break;
	default:
		transfers = V4L2_XFER_FUNC_DEFAULT;
		matrix = V4L2_YCBCR_ENC_DEFAULT;
		break;
	}
	full_range = V4L2_QUANTIZATION_LIM_RANGE;

	if (ptransfers)
		*ptransfers = transfers;
	if (pmatrix)
		*pmatrix = matrix;
	if (pfull_range)
		*pfull_range = full_range;

	return 0;
}
