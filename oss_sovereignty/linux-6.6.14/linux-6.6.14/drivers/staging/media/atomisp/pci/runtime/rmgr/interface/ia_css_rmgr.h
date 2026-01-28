#ifndef _IA_CSS_RMGR_H
#define _IA_CSS_RMGR_H
#include <ia_css_err.h>
#ifndef __INLINE_RMGR__
#define STORAGE_CLASS_RMGR_H extern
#define STORAGE_CLASS_RMGR_C
#else				 
#define STORAGE_CLASS_RMGR_H static inline
#define STORAGE_CLASS_RMGR_C static inline
#endif				 
int ia_css_rmgr_init(void);
void ia_css_rmgr_uninit(void);
#include "ia_css_rmgr_vbuf.h"
#endif	 
