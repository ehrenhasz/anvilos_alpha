#ifndef __SH_CSS_FRAC_H
#define __SH_CSS_FRAC_H
#include <math_support.h>
#define sISP_REG_BIT		      ISP_VEC_ELEMBITS
#define uISP_REG_BIT		      ((unsigned int)(sISP_REG_BIT - 1))
#define sSHIFT				    (16 - sISP_REG_BIT)
#define uSHIFT				    ((unsigned int)(16 - uISP_REG_BIT))
#define sFRACTION_BITS_FITTING(a) (a - sSHIFT)
#define uFRACTION_BITS_FITTING(a) ((unsigned int)(a - uSHIFT))
#define sISP_VAL_MIN		      (-(1 << uISP_REG_BIT))
#define sISP_VAL_MAX		      ((1 << uISP_REG_BIT) - 1)
#define uISP_VAL_MIN		      (0U)
#define uISP_VAL_MAX		      ((unsigned int)((1 << uISP_REG_BIT) - 1))
#define sDIGIT_FITTING(v, a, b) \
	min_t(int, max_t(int, (((v) >> sSHIFT) >> max(sFRACTION_BITS_FITTING(a) - (b), 0)), \
	  sISP_VAL_MIN), sISP_VAL_MAX)
#define uDIGIT_FITTING(v, a, b) \
	min((unsigned int)max((unsigned)(((v) >> uSHIFT) \
	>> max((int)(uFRACTION_BITS_FITTING(a) - (b)), 0)), \
	  uISP_VAL_MIN), uISP_VAL_MAX)
#endif  
