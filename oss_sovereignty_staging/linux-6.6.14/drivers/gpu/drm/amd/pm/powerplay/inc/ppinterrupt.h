 

#ifndef _PP_INTERRUPT_H_
#define _PP_INTERRUPT_H_

enum amd_thermal_irq {
	AMD_THERMAL_IRQ_LOW_TO_HIGH = 0,
	AMD_THERMAL_IRQ_HIGH_TO_LOW,

	AMD_THERMAL_IRQ_LAST
};

 
typedef int (*irq_handler_func_t)(void *private_data,
				unsigned src_id, const uint32_t *iv_entry);

 
struct pp_interrupt_registration_info {
	irq_handler_func_t call_back;  
	void *context;                    
	uint32_t src_id;                
	const uint32_t *iv_entry;
};

#endif  
