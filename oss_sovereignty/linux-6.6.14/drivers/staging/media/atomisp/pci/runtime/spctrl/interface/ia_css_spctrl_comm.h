 
 

#ifndef __IA_CSS_SPCTRL_COMM_H__
#define __IA_CSS_SPCTRL_COMM_H__

#include <type_support.h>

 
typedef enum {
	IA_CSS_SP_SW_TERMINATED = 0,
	IA_CSS_SP_SW_INITIALIZED,
	IA_CSS_SP_SW_CONNECTED,
	IA_CSS_SP_SW_RUNNING
} ia_css_spctrl_sp_sw_state;

 
struct ia_css_sp_init_dmem_cfg {
	ia_css_ptr      ddr_data_addr;   
	u32        dmem_data_addr;  
	u32        dmem_bss_addr;   
	u32        data_size;       
	u32        bss_size;        
	sp_ID_t         sp_id;           
};

#define SIZE_OF_IA_CSS_SP_INIT_DMEM_CFG_STRUCT	\
	(1 * SIZE_OF_IA_CSS_PTR) +		\
	(4 * sizeof(uint32_t)) +		\
	(1 * sizeof(sp_ID_t))

#endif  
