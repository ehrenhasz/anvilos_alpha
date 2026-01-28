#ifndef _SH_CSS_DEFS_H_
#define _SH_CSS_DEFS_H_
#include "isp.h"
#include "math_support.h"	 
#define IA_CSS_REFCOUNT_PARAM_SET_POOL  0xCAFE0001
#define IA_CSS_REFCOUNT_PARAM_BUFFER    0xCAFE0002
#define SH_CSS_DIS_DECI_FACTOR_LOG2       6
#define SH_CSS_UV_OFFSET_IS_0             0
#define SH_CSS_BAYER_BITS                 13
#define SH_CSS_BAYER_MAXVAL               ((1U << SH_CSS_BAYER_BITS) - 1)
#define SH_CSS_ISP_YUV_BITS               8
#define SH_CSS_DP_GAIN_SHIFT              5
#define SH_CSS_BNR_GAIN_SHIFT             13
#define SH_CSS_YNR_GAIN_SHIFT             13
#define SH_CSS_AE_YCOEF_SHIFT             13
#define SH_CSS_AF_FIR_SHIFT               13
#define SH_CSS_YEE_DETAIL_GAIN_SHIFT      8   
#define SH_CSS_YEE_SCALE_SHIFT            8
#define SH_CSS_TNR_COEF_SHIFT             13
#define SH_CSS_MACC_COEF_SHIFT            11  
#define SH_CSS_MACC2_COEF_SHIFT           13  
#define SH_CSS_DIS_COEF_SHIFT             13
#define SH_CSS_BDS_FACTOR_1_00	(0)
#define SH_CSS_BDS_FACTOR_1_25	(1)
#define SH_CSS_BDS_FACTOR_1_50	(2)
#define SH_CSS_BDS_FACTOR_2_00	(3)
#define SH_CSS_BDS_FACTOR_2_25	(4)
#define SH_CSS_BDS_FACTOR_2_50	(5)
#define SH_CSS_BDS_FACTOR_3_00	(6)
#define SH_CSS_BDS_FACTOR_4_00	(7)
#define SH_CSS_BDS_FACTOR_4_50	(8)
#define SH_CSS_BDS_FACTOR_5_00	(9)
#define SH_CSS_BDS_FACTOR_6_00	(10)
#define SH_CSS_BDS_FACTOR_8_00	(11)
#define NUM_BDS_FACTORS		(12)
#define PACK_BDS_FACTOR(factor)	(1 << (factor))
#define SH_CSS_ISP_PIPE_VERSION_1	1
#define SH_CSS_ISP_PIPE_VERSION_2_2	2
#define SH_CSS_ISP_PIPE_VERSION_2_6_1	3
#define SH_CSS_ISP_PIPE_VERSION_2_7	4
#define SH_CSS_RGB_GAMMA_INPUT_BITS       12  
#define SH_CSS_RGB_GAMMA_OUTPUT_BITS      13  
#define SH_CSS_RGB_GAMMA_FRAC_BITS        \
	(SH_CSS_RGB_GAMMA_INPUT_BITS - SH_CSS_ISP_RGB_GAMMA_TABLE_SIZE_LOG2)
#define SH_CSS_RGB_GAMMA_ONE              BIT(SH_CSS_RGB_GAMMA_FRAC_BITS)
#define SH_CSS_YUV2RGB_CCM_INPUT_BITS     SH_CSS_BAYER_BITS
#define SH_CSS_YUV2RGB_CCM_OUTPUT_BITS    SH_CSS_RGB_GAMMA_INPUT_BITS
#define SH_CSS_YUV2RGB_CCM_MAX_OUTPUT     \
	((1 << SH_CSS_YUV2RGB_CCM_OUTPUT_BITS) - 1)
#define SH_CSS_NUM_INPUT_BUF_LINES        4
#define SH_CSS_MAX_LEFT_CROPPING          12
#define SH_CSS_MAX_TOP_CROPPING           12
#define	SH_CSS_SP_MAX_WIDTH               1280
#define SH_CSS_MAX_BQ_GRID_WIDTH          80
#define SH_CSS_MAX_BQ_GRID_HEIGHT         60
#define SH_CSS_MIN_DVS_ENVELOPE           12U
#define SH_CSS_MAX_SENSOR_WIDTH           4608
#define SH_CSS_MAX_SENSOR_HEIGHT          3450
#if ISP_VMEM_DEPTH >= 3072
#define SH_CSS_MAX_CONTINUOUS_SENSOR_WIDTH  SH_CSS_MAX_SENSOR_WIDTH
#define SH_CSS_MAX_CONTINUOUS_SENSOR_HEIGHT SH_CSS_MAX_SENSOR_HEIGHT
#else
#define SH_CSS_MAX_CONTINUOUS_SENSOR_WIDTH  3264
#define SH_CSS_MAX_CONTINUOUS_SENSOR_HEIGHT 2448
#endif
#define SH_CSS_MAX_CONTINUOUS_SENSOR_WIDTH_DEC  SH_CSS_MAX_SENSOR_WIDTH
#define SH_CSS_MAX_CONTINUOUS_SENSOR_HEIGHT_DEC SH_CSS_MAX_SENSOR_HEIGHT
#define SH_CSS_MIN_SENSOR_WIDTH           2
#define SH_CSS_MIN_SENSOR_HEIGHT          2
#define SH_CSS_MAX_VF_WIDTH_DEC               SH_CSS_MAX_VF_WIDTH
#define SH_CSS_MAX_VF_HEIGHT_DEC              SH_CSS_MAX_VF_HEIGHT
#define SH_CSS_MORPH_TABLE_GRID               ISP_VEC_NELEMS
#define SH_CSS_MORPH_TABLE_ELEM_BYTES         2
#define SH_CSS_MORPH_TABLE_ELEMS_PER_DDR_WORD \
	(HIVE_ISP_DDR_WORD_BYTES / SH_CSS_MORPH_TABLE_ELEM_BYTES)
#define SH_CSS_MAX_SCTBL_WIDTH_PER_COLOR   (SH_CSS_MAX_BQ_GRID_WIDTH + 1)
#define SH_CSS_MAX_SCTBL_HEIGHT_PER_COLOR   (SH_CSS_MAX_BQ_GRID_HEIGHT + 1)
#define SH_CSS_MAX_SCTBL_ALIGNED_WIDTH_PER_COLOR \
	CEIL_MUL(SH_CSS_MAX_SCTBL_WIDTH_PER_COLOR, ISP_VEC_NELEMS)
#define SH_CSS_MAX_S3ATBL_WIDTH              SH_CSS_MAX_BQ_GRID_WIDTH
#define MAX_DVS_FRAME_DELAY		2
#define MAX_NUM_VIDEO_DELAY_FRAMES	(MAX_DVS_FRAME_DELAY + 1)
#define NUM_VIDEO_TNR_FRAMES		2
#define _ISP_BQS(num)  ((num) / 2)
#define _ISP_VECS(width) CEIL_DIV(width, ISP_VEC_NELEMS)
#define ISP_BQ_GRID_WIDTH(elements_per_line, deci_factor_log2) \
	CEIL_SHIFT(elements_per_line / 2,  deci_factor_log2)
#define ISP_BQ_GRID_HEIGHT(lines_per_frame, deci_factor_log2) \
	CEIL_SHIFT(lines_per_frame / 2,  deci_factor_log2)
#define ISP_C_VECTORS_PER_LINE(elements_per_line) \
	_ISP_VECS(elements_per_line / 2)
#define _ISP_MORPH_TABLE_WIDTH(int_width) \
	(CEIL_DIV(int_width, SH_CSS_MORPH_TABLE_GRID) + 1)
#define _ISP_MORPH_TABLE_HEIGHT(int_height) \
	(CEIL_DIV(int_height, SH_CSS_MORPH_TABLE_GRID) + 1)
#define _ISP_MORPH_TABLE_ALIGNED_WIDTH(width) \
	CEIL_MUL(_ISP_MORPH_TABLE_WIDTH(width), \
		 SH_CSS_MORPH_TABLE_ELEMS_PER_DDR_WORD)
#define _ISP_SCTBL_WIDTH_PER_COLOR(input_width, deci_factor_log2) \
	(ISP_BQ_GRID_WIDTH(input_width, deci_factor_log2) + 1)
#define _ISP_SCTBL_HEIGHT(input_height, deci_factor_log2) \
	(ISP_BQ_GRID_HEIGHT(input_height, deci_factor_log2) + 1)
#define _ISP_SCTBL_ALIGNED_WIDTH_PER_COLOR(input_width, deci_factor_log2) \
	CEIL_MUL(_ISP_SCTBL_WIDTH_PER_COLOR(input_width, deci_factor_log2), \
		 ISP_VEC_NELEMS)
#define SH_CSS_SCTBL_CENTERING_MARGIN	1
#define SH_CSS_SCTBL_LAST_GRID_COUNT	1
#define _ISP2401_SCTBL_WIDTH_PER_COLOR(input_width, deci_factor_log2) \
	(ISP_BQ_GRID_WIDTH(input_width, deci_factor_log2) + \
	SH_CSS_SCTBL_CENTERING_MARGIN + SH_CSS_SCTBL_LAST_GRID_COUNT)
#define _ISP2401_SCTBL_HEIGHT(input_height, deci_factor_log2) \
	(ISP_BQ_GRID_HEIGHT(input_height, deci_factor_log2) + \
	SH_CSS_SCTBL_CENTERING_MARGIN + SH_CSS_SCTBL_LAST_GRID_COUNT)
#define _ISP_SCTBL_LEGACY_WIDTH_PER_COLOR(input_width, deci_factor_log2) \
	(ISP_BQ_GRID_WIDTH(input_width, deci_factor_log2) + SH_CSS_SCTBL_LAST_GRID_COUNT)
#define _ISP_SCTBL_LEGACY_HEIGHT(input_height, deci_factor_log2) \
	(ISP_BQ_GRID_HEIGHT(input_height, deci_factor_log2) + SH_CSS_SCTBL_LAST_GRID_COUNT)
#define _ISP_S3ATBL_WIDTH(in_width, deci_factor_log2) \
	(_ISP_BQS(in_width) >> deci_factor_log2)
#define _ISP_S3ATBL_HEIGHT(in_height, deci_factor_log2) \
	(_ISP_BQS(in_height) >> deci_factor_log2)
#define _ISP_S3A_ELEMS_ISP_WIDTH(width, left_crop) \
	(width - ((left_crop) ? 2 * ISP_VEC_NELEMS : 0))
#define _ISP_S3ATBL_ISP_WIDTH(in_width, deci_factor_log2) \
	CEIL_SHIFT(_ISP_BQS(in_width), deci_factor_log2)
#define _ISP_S3ATBL_ISP_HEIGHT(in_height, deci_factor_log2) \
	CEIL_SHIFT(_ISP_BQS(in_height), deci_factor_log2)
#define ISP_S3ATBL_VECTORS \
	_ISP_VECS(SH_CSS_MAX_S3ATBL_WIDTH * \
		  (sizeof(struct ia_css_3a_output) / sizeof(int32_t)))
#define ISP_S3ATBL_HI_LO_STRIDE \
	(ISP_S3ATBL_VECTORS * ISP_VEC_NELEMS)
#define ISP_S3ATBL_HI_LO_STRIDE_BYTES \
	(sizeof(unsigned short) * ISP_S3ATBL_HI_LO_STRIDE)
#define __ISP_MAX_VF_OUTPUT_WIDTH(width, left_crop) \
	(width - 2 * ISP_VEC_NELEMS + ((left_crop) ? 2 * ISP_VEC_NELEMS : 0))
#define __ISP_VF_OUTPUT_WIDTH_VECS(out_width, vf_log_downscale) \
	(_ISP_VECS((out_width) >> (vf_log_downscale)))
#define _ISP_VF_OUTPUT_WIDTH(vf_out_vecs) ((vf_out_vecs) * ISP_VEC_NELEMS)
#define _ISP_VF_OUTPUT_HEIGHT(out_height, vf_log_ds) \
	((out_height) >> (vf_log_ds))
#define _ISP_LOG_VECTOR_STEP(mode) \
	((mode) == IA_CSS_BINARY_MODE_CAPTURE_PP ? 2 : 1)
#define MAX_PREFERRED_YUV_DS_PER_STEP	2
#define _ISP_LEFT_CROP_EXTRA(left_crop) ((left_crop) > 0 ? 2 * ISP_VEC_NELEMS : 0)
#define __ISP_MIN_INTERNAL_WIDTH(num_chunks, pipelining, mode) \
	((num_chunks) * (pipelining) * (1 << _ISP_LOG_VECTOR_STEP(mode)) * \
	 ISP_VEC_NELEMS)
#define __ISP_PADDED_OUTPUT_WIDTH(out_width, dvs_env_width, left_crop) \
	((out_width) + MAX(dvs_env_width, _ISP_LEFT_CROP_EXTRA(left_crop)))
#define __ISP_CHUNK_STRIDE_ISP(mode) \
	((1 << _ISP_LOG_VECTOR_STEP(mode)) * ISP_VEC_NELEMS)
#define __ISP_CHUNK_STRIDE_DDR(c_subsampling, num_chunks) \
	((c_subsampling) * (num_chunks) * HIVE_ISP_DDR_WORD_BYTES)
#define __ISP_INTERNAL_WIDTH(out_width, \
			     dvs_env_width, \
			     left_crop, \
			     mode, \
			     c_subsampling, \
			     num_chunks, \
			     pipelining) \
	CEIL_MUL2(CEIL_MUL2(MAX(__ISP_PADDED_OUTPUT_WIDTH(out_width, \
							    dvs_env_width, \
							    left_crop), \
				  __ISP_MIN_INTERNAL_WIDTH(num_chunks, \
							   pipelining, \
							   mode) \
				 ), \
			  __ISP_CHUNK_STRIDE_ISP(mode) \
			 ), \
		 __ISP_CHUNK_STRIDE_DDR(c_subsampling, num_chunks) \
		)
#define __ISP_INTERNAL_HEIGHT(out_height, dvs_env_height, top_crop) \
	((out_height) + (dvs_env_height) + top_crop)
#define _ISP_MAX_INPUT_WIDTH(max_internal_width, enable_ds, enable_fixed_bayer_ds, enable_raw_bin, \
				enable_continuous) \
	((enable_ds) ? \
	   SH_CSS_MAX_SENSOR_WIDTH :\
	 (enable_fixed_bayer_ds) ? \
	   CEIL_MUL(SH_CSS_MAX_CONTINUOUS_SENSOR_WIDTH_DEC, 4 * ISP_VEC_NELEMS) : \
	 (enable_raw_bin) ? \
	   CEIL_MUL(SH_CSS_MAX_CONTINUOUS_SENSOR_WIDTH, 4 * ISP_VEC_NELEMS) : \
	 (enable_continuous) ? \
	   SH_CSS_MAX_CONTINUOUS_SENSOR_WIDTH \
	   : max_internal_width)
#define _ISP_INPUT_WIDTH(internal_width, ds_input_width, enable_ds) \
	((enable_ds) ? (ds_input_width) : (internal_width))
#define _ISP_MAX_INPUT_HEIGHT(max_internal_height, enable_ds, enable_fixed_bayer_ds, enable_raw_bin, \
				enable_continuous) \
	((enable_ds) ? \
	   SH_CSS_MAX_SENSOR_HEIGHT :\
	 (enable_fixed_bayer_ds) ? \
	   SH_CSS_MAX_CONTINUOUS_SENSOR_HEIGHT_DEC : \
	 (enable_raw_bin || enable_continuous) ? \
	   SH_CSS_MAX_CONTINUOUS_SENSOR_HEIGHT \
	   : max_internal_height)
#define _ISP_INPUT_HEIGHT(internal_height, ds_input_height, enable_ds) \
	((enable_ds) ? (ds_input_height) : (internal_height))
#define SH_CSS_MAX_STAGES 8  
#define _ISP_EXTRA_PADDING_VECS 0
#endif  
