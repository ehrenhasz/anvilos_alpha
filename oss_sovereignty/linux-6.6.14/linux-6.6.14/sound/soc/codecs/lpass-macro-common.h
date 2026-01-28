#ifndef __LPASS_MACRO_COMMON_H__
#define __LPASS_MACRO_COMMON_H__
#define LPASS_MACRO_FLAG_HAS_NPL_CLOCK		BIT(0)
struct lpass_macro {
	struct device *macro_pd;
	struct device *dcodec_pd;
};
struct lpass_macro *lpass_macro_pds_init(struct device *dev);
void lpass_macro_pds_exit(struct lpass_macro *pds);
#endif  
