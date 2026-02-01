 
 

#ifndef __IA_CSS_SPCTRL_H__
#define __IA_CSS_SPCTRL_H__

#include <system_global.h>
#include <ia_css_err.h>
#include "ia_css_spctrl_comm.h"

typedef struct {
	u32        ddr_data_offset;        
	u32        dmem_data_addr;         
	u32        dmem_bss_addr;          
	u32        data_size;              
	u32        bss_size;               
	u32        spctrl_config_dmem_addr;  
	u32        spctrl_state_dmem_addr;   
	unsigned int    sp_entry;                 
	const void      *code;                    
	u32         code_size;
	char      *program_name;     
} ia_css_spctrl_cfg;

 
ia_css_ptr get_sp_code_addr(sp_ID_t  sp_id);

 
int ia_css_spctrl_load_fw(sp_ID_t sp_id,
				      ia_css_spctrl_cfg *spctrl_cfg);

 
 
void sh_css_spctrl_reload_fw(sp_ID_t sp_id);

 
int ia_css_spctrl_unload_fw(sp_ID_t sp_id);

 
int ia_css_spctrl_start(sp_ID_t sp_id);

 
int ia_css_spctrl_stop(sp_ID_t sp_id);

 
ia_css_spctrl_sp_sw_state ia_css_spctrl_get_state(sp_ID_t sp_id);

 
int ia_css_spctrl_is_idle(sp_ID_t sp_id);

#endif  
