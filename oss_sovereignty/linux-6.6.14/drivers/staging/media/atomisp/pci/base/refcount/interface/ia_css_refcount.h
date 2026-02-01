 
 

#ifndef _IA_CSS_REFCOUNT_H_
#define _IA_CSS_REFCOUNT_H_

#include <type_support.h>
#include <system_local.h>
#include <ia_css_err.h>
#include <ia_css_types.h>

typedef void (*clear_func)(ia_css_ptr ptr);

 
int ia_css_refcount_init(uint32_t size);

 
void ia_css_refcount_uninit(void);

 
ia_css_ptr ia_css_refcount_increment(s32 id, ia_css_ptr ptr);

 
bool ia_css_refcount_decrement(s32 id, ia_css_ptr ptr);

 
bool ia_css_refcount_is_single(ia_css_ptr ptr);

 
void ia_css_refcount_clear(s32 id,
			   clear_func clear_func_ptr);

 
bool ia_css_refcount_is_valid(ia_css_ptr ptr);

#endif  
