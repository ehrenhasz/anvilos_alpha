 
 

#ifndef __IA_CSS_CONTROL_H
#define __IA_CSS_CONTROL_H

 

#include <type_support.h>
#include <ia_css_env.h>
#include <ia_css_firmware.h>
#include <ia_css_irq.h>

 
int ia_css_init(struct device           *dev,
			    const struct ia_css_env *env,
			    const struct ia_css_fw  *fw,
			    u32                     l1_base,
			    enum ia_css_irq_type    irq_type);

 
void
ia_css_uninit(void);

 
int
ia_css_enable_isys_event_queue(bool enable);

 
bool
ia_css_isp_has_started(void);

 
bool
ia_css_sp_has_initialized(void);

 
bool
ia_css_sp_has_terminated(void);

 
int
ia_css_start_sp(void);

 
int
ia_css_stop_sp(void);

#endif  
