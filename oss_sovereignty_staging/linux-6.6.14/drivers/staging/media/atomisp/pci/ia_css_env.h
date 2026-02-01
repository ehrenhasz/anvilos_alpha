 
 

#ifndef __IA_CSS_ENV_H
#define __IA_CSS_ENV_H

#include <type_support.h>
#include <linux/stdarg.h>  
#include <linux/bits.h>
#include "ia_css_types.h"
#include "ia_css_acc_types.h"

 

 
enum ia_css_mem_attr {
	IA_CSS_MEM_ATTR_CACHED     = BIT(0),
	IA_CSS_MEM_ATTR_ZEROED     = BIT(1),
	IA_CSS_MEM_ATTR_PAGEALIGN  = BIT(2),
	IA_CSS_MEM_ATTR_CONTIGUOUS = BIT(3),
};

 
struct ia_css_cpu_mem_env {
	void (*flush)(struct ia_css_acc_fw *fw);
	 
};

 
struct ia_css_hw_access_env {
	void (*store_8)(hrt_address addr, uint8_t data);
	 
	void (*store_16)(hrt_address addr, uint16_t data);
	 
	void (*store_32)(hrt_address addr, uint32_t data);
	 
	uint8_t (*load_8)(hrt_address addr);
	 
	uint16_t (*load_16)(hrt_address addr);
	 
	uint32_t (*load_32)(hrt_address addr);
	 
	void (*store)(hrt_address addr, const void *data, uint32_t bytes);
	 
	void (*load)(hrt_address addr, void *data, uint32_t bytes);
	 
};

 
struct ia_css_print_env {
	int  __printf(1, 0) (*debug_print)(const char *fmt, va_list args);
	 
	int  __printf(1, 0) (*error_print)(const char *fmt, va_list args);
	 
};

 
struct ia_css_env {
	struct ia_css_cpu_mem_env   cpu_mem_env;    
	struct ia_css_hw_access_env hw_access_env;  
	struct ia_css_print_env     print_env;      
};

#endif  
