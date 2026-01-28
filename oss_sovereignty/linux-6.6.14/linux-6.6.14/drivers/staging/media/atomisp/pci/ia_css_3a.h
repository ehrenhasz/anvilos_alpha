#ifndef __IA_CSS_3A_H
#define __IA_CSS_3A_H
#include <type_support.h>
#include "ia_css_types.h"
#include "ia_css_err.h"
#include "system_global.h"
enum ia_css_3a_tables {
	IA_CSS_S3A_TBL_HI,
	IA_CSS_S3A_TBL_LO,
	IA_CSS_RGBY_TBL,
	IA_CSS_NUM_3A_TABLES
};
struct ia_css_isp_3a_statistics {
	union {
		struct {
			ia_css_ptr s3a_tbl;
		} dmem;
		struct {
			ia_css_ptr s3a_tbl_hi;
			ia_css_ptr s3a_tbl_lo;
		} vmem;
	} data;
	struct {
		ia_css_ptr rgby_tbl;
	} data_hmem;
	u32 exp_id;      
	u32 isp_config_id; 
	ia_css_ptr data_ptr;  
	u32   size;      
	u32   dmem_size;
	u32   vmem_size;  
	u32   hmem_size;
};
#define SIZE_OF_DMEM_STRUCT						\
	(SIZE_OF_IA_CSS_PTR)
#define SIZE_OF_VMEM_STRUCT						\
	(2 * SIZE_OF_IA_CSS_PTR)
#define SIZE_OF_DATA_UNION						\
	(MAX(SIZE_OF_DMEM_STRUCT, SIZE_OF_VMEM_STRUCT))
#define SIZE_OF_DATA_HMEM_STRUCT					\
	(SIZE_OF_IA_CSS_PTR)
#define SIZE_OF_IA_CSS_ISP_3A_STATISTICS_STRUCT				\
	(SIZE_OF_DATA_UNION +						\
	 SIZE_OF_DATA_HMEM_STRUCT +					\
	 sizeof(uint32_t) +						\
	 sizeof(uint32_t) +						\
	 SIZE_OF_IA_CSS_PTR +						\
	 4 * sizeof(uint32_t))
struct ia_css_isp_3a_statistics_map {
	void                    *data_ptr;  
	struct ia_css_3a_output *dmem_stats;
	u16                *vmem_stats_hi;
	u16                *vmem_stats_lo;
	struct ia_css_bh_table  *hmem_stats;
	u32                 size;  
	u32                 data_allocated;  
};
int
ia_css_get_3a_statistics(struct ia_css_3a_statistics           *host_stats,
			 const struct ia_css_isp_3a_statistics *isp_stats);
void
ia_css_translate_3a_statistics(
    struct ia_css_3a_statistics               *host_stats,
    const struct ia_css_isp_3a_statistics_map *isp_stats);
struct ia_css_isp_3a_statistics *
ia_css_isp_3a_statistics_allocate(const struct ia_css_3a_grid_info *grid);
void
ia_css_isp_3a_statistics_free(struct ia_css_isp_3a_statistics *me);
struct ia_css_3a_statistics *
ia_css_3a_statistics_allocate(const struct ia_css_3a_grid_info *grid);
void
ia_css_3a_statistics_free(struct ia_css_3a_statistics *me);
struct ia_css_isp_3a_statistics_map *
ia_css_isp_3a_statistics_map_allocate(
    const struct ia_css_isp_3a_statistics *isp_stats,
    void *data_ptr);
void
ia_css_isp_3a_statistics_map_free(struct ia_css_isp_3a_statistics_map *me);
#endif  
