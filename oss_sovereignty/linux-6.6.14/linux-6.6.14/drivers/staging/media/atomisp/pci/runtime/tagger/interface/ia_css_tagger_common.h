#ifndef __IA_CSS_TAGGER_COMMON_H__
#define __IA_CSS_TAGGER_COMMON_H__
#include <system_local.h>
#include <type_support.h>
#define MAX_CB_ELEMS_FOR_TAGGER 14
typedef struct {
	u32 frame;	 
	u32 param;	 
	u8 mark;	 
	u8 lock;	 
	u8 exp_id;  
} ia_css_tagger_buf_sp_elem_t;
#endif  
