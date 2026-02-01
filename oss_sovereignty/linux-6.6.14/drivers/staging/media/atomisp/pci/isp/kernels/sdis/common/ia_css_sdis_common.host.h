 
 

#ifndef _IA_CSS_SDIS_COMMON_HOST_H
#define _IA_CSS_SDIS_COMMON_HOST_H

#define ISP_MAX_SDIS_HOR_PROJ_NUM_ISP \
	__ISP_SDIS_HOR_PROJ_NUM_ISP(ISP_MAX_INTERNAL_WIDTH, ISP_MAX_INTERNAL_HEIGHT, \
		SH_CSS_DIS_DECI_FACTOR_LOG2, ISP_PIPE_VERSION)
#define ISP_MAX_SDIS_VER_PROJ_NUM_ISP \
	__ISP_SDIS_VER_PROJ_NUM_ISP(ISP_MAX_INTERNAL_WIDTH, \
		SH_CSS_DIS_DECI_FACTOR_LOG2)

#define _ISP_SDIS_HOR_COEF_NUM_VECS \
	__ISP_SDIS_HOR_COEF_NUM_VECS(ISP_INTERNAL_WIDTH)
#define ISP_MAX_SDIS_HOR_COEF_NUM_VECS \
	__ISP_SDIS_HOR_COEF_NUM_VECS(ISP_MAX_INTERNAL_WIDTH)
#define ISP_MAX_SDIS_VER_COEF_NUM_VECS \
	__ISP_SDIS_VER_COEF_NUM_VECS(ISP_MAX_INTERNAL_HEIGHT)

 
 
#define __ISP_SDIS_HOR_COEF_NUM_VECS(in_width)  _ISP_VECS(_ISP_BQS(in_width))
#define __ISP_SDIS_VER_COEF_NUM_VECS(in_height) _ISP_VECS(_ISP_BQS(in_height))

 
#define __ISP_SDIS_HOR_PROJ_NUM_ISP(in_width, in_height, deci_factor_log2, \
	isp_pipe_version) \
	((isp_pipe_version == 1) ? \
		CEIL_SHIFT(_ISP_BQS(in_height), deci_factor_log2) : \
		CEIL_SHIFT(_ISP_BQS(in_width), deci_factor_log2))

#define __ISP_SDIS_VER_PROJ_NUM_ISP(in_width, deci_factor_log2) \
	CEIL_SHIFT(_ISP_BQS(in_width), deci_factor_log2)

#define SH_CSS_DIS_VER_NUM_COEF_TYPES(b) \
  (((b)->info->sp.pipeline.isp_pipe_version == 2) ? \
	IA_CSS_DVS2_NUM_COEF_TYPES : \
	IA_CSS_DVS_NUM_COEF_TYPES)

#ifndef PIPE_GENERATION
#if defined(__ISP) || defined(MK_FIRMWARE)

 
struct sh_css_isp_sdis_hori_proj_tbl {
	s32 tbl[ISP_DVS_NUM_COEF_TYPES * ISP_MAX_SDIS_HOR_PROJ_NUM_ISP];
#if DVS2_PROJ_MARGIN > 0
	s32 margin[DVS2_PROJ_MARGIN];
#endif
};

struct sh_css_isp_sdis_vert_proj_tbl {
	s32 tbl[ISP_DVS_NUM_COEF_TYPES * ISP_MAX_SDIS_VER_PROJ_NUM_ISP];
#if DVS2_PROJ_MARGIN > 0
	s32 margin[DVS2_PROJ_MARGIN];
#endif
};

struct sh_css_isp_sdis_hori_coef_tbl {
	VMEM_ARRAY(tbl[ISP_DVS_NUM_COEF_TYPES],
		   ISP_MAX_SDIS_HOR_COEF_NUM_VECS * ISP_NWAY);
};

struct sh_css_isp_sdis_vert_coef_tbl {
	VMEM_ARRAY(tbl[ISP_DVS_NUM_COEF_TYPES],
		   ISP_MAX_SDIS_VER_COEF_NUM_VECS * ISP_NWAY);
};

#endif  
#endif  

#ifndef PIPE_GENERATION
struct s_sdis_config {
	unsigned int horicoef_vectors;
	unsigned int vertcoef_vectors;
	unsigned int horiproj_num;
	unsigned int vertproj_num;
};

extern struct s_sdis_config sdis_config;
#endif

#endif  
