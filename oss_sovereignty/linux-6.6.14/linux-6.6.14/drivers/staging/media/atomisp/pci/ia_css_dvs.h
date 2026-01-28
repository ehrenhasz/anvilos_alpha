#ifndef __IA_CSS_DVS_H
#define __IA_CSS_DVS_H
#include <type_support.h>
#include "ia_css_types.h"
#include "ia_css_err.h"
#include "ia_css_stream_public.h"
enum dvs_statistics_type {
	DVS_STATISTICS,
	DVS2_STATISTICS,
	SKC_DVS_STATISTICS
};
struct ia_css_isp_dvs_statistics {
	ia_css_ptr hor_proj;
	ia_css_ptr ver_proj;
	u32   hor_size;
	u32   ver_size;
	u32   exp_id;    
	ia_css_ptr data_ptr;  
	u32   size;      
};
struct ia_css_isp_skc_dvs_statistics;
#define SIZE_OF_IA_CSS_ISP_DVS_STATISTICS_STRUCT			\
	((3 * SIZE_OF_IA_CSS_PTR) +					\
	 (4 * sizeof(uint32_t)))
struct ia_css_isp_dvs_statistics_map {
	void    *data_ptr;
	s32 *hor_proj;
	s32 *ver_proj;
	u32 size;		  
	u32 data_allocated;  
};
union ia_css_dvs_statistics_isp {
	struct ia_css_isp_dvs_statistics *p_dvs_statistics_isp;
	struct ia_css_isp_skc_dvs_statistics *p_skc_dvs_statistics_isp;
};
union ia_css_dvs_statistics_host {
	struct ia_css_dvs_statistics *p_dvs_statistics_host;
	struct ia_css_dvs2_statistics *p_dvs2_statistics_host;
	struct ia_css_skc_dvs_statistics *p_skc_dvs_statistics_host;
};
int
ia_css_get_dvs_statistics(struct ia_css_dvs_statistics *host_stats,
			  const struct ia_css_isp_dvs_statistics *isp_stats);
void
ia_css_translate_dvs_statistics(
    struct ia_css_dvs_statistics *host_stats,
    const struct ia_css_isp_dvs_statistics_map *isp_stats);
int
ia_css_get_dvs2_statistics(struct ia_css_dvs2_statistics *host_stats,
			   const struct ia_css_isp_dvs_statistics *isp_stats);
void
ia_css_translate_dvs2_statistics(
    struct ia_css_dvs2_statistics	   *host_stats,
    const struct ia_css_isp_dvs_statistics_map *isp_stats);
void
ia_css_dvs_statistics_get(enum dvs_statistics_type type,
			  union ia_css_dvs_statistics_host  *host_stats,
			  const union ia_css_dvs_statistics_isp *isp_stats);
struct ia_css_isp_dvs_statistics *
ia_css_isp_dvs_statistics_allocate(const struct ia_css_dvs_grid_info *grid);
void
ia_css_isp_dvs_statistics_free(struct ia_css_isp_dvs_statistics *me);
struct ia_css_isp_dvs_statistics *
ia_css_isp_dvs2_statistics_allocate(const struct ia_css_dvs_grid_info *grid);
void
ia_css_isp_dvs2_statistics_free(struct ia_css_isp_dvs_statistics *me);
struct ia_css_dvs_statistics *
ia_css_dvs_statistics_allocate(const struct ia_css_dvs_grid_info *grid);
void
ia_css_dvs_statistics_free(struct ia_css_dvs_statistics *me);
struct ia_css_dvs_coefficients *
ia_css_dvs_coefficients_allocate(const struct ia_css_dvs_grid_info *grid);
void
ia_css_dvs_coefficients_free(struct ia_css_dvs_coefficients *me);
struct ia_css_dvs2_statistics *
ia_css_dvs2_statistics_allocate(const struct ia_css_dvs_grid_info *grid);
void
ia_css_dvs2_statistics_free(struct ia_css_dvs2_statistics *me);
struct ia_css_dvs2_coefficients *
ia_css_dvs2_coefficients_allocate(const struct ia_css_dvs_grid_info *grid);
void
ia_css_dvs2_coefficients_free(struct ia_css_dvs2_coefficients *me);
struct ia_css_dvs_6axis_config *
ia_css_dvs2_6axis_config_allocate(const struct ia_css_stream *stream);
void
ia_css_dvs2_6axis_config_free(struct ia_css_dvs_6axis_config *dvs_6axis_config);
struct ia_css_isp_dvs_statistics_map *
ia_css_isp_dvs_statistics_map_allocate(
    const struct ia_css_isp_dvs_statistics *isp_stats,
    void *data_ptr);
void
ia_css_isp_dvs_statistics_map_free(struct ia_css_isp_dvs_statistics_map *me);
struct ia_css_isp_skc_dvs_statistics *ia_css_skc_dvs_statistics_allocate(void);
#endif  
