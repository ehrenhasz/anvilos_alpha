#ifndef __IA_CSS_XNR_TYPES_H
#define __IA_CSS_XNR_TYPES_H
#define IA_CSS_VAMEM_1_XNR_TABLE_SIZE_LOG2      6
#define IA_CSS_VAMEM_1_XNR_TABLE_SIZE           BIT(IA_CSS_VAMEM_1_XNR_TABLE_SIZE_LOG2)
#define IA_CSS_VAMEM_2_XNR_TABLE_SIZE_LOG2      6
#define IA_CSS_VAMEM_2_XNR_TABLE_SIZE		BIT(IA_CSS_VAMEM_2_XNR_TABLE_SIZE_LOG2)
union ia_css_xnr_data {
	u16 vamem_1[IA_CSS_VAMEM_1_XNR_TABLE_SIZE];
	u16 vamem_2[IA_CSS_VAMEM_2_XNR_TABLE_SIZE];
};
struct ia_css_xnr_table {
	enum ia_css_vamem_type vamem_type;
	union ia_css_xnr_data data;
};
struct ia_css_xnr_config {
	u16 threshold;
};
#endif  
