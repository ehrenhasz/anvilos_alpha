 
 

#ifndef __ISYS_IRQ_LOCAL_H__
#define __ISYS_IRQ_LOCAL_H__

#include <type_support.h>

#if defined(ISP2401)

typedef struct isys_irqc_state_s isys_irqc_state_t;

struct isys_irqc_state_s {
	hrt_data edge;
	hrt_data mask;
	hrt_data status;
	hrt_data enable;
	hrt_data level_no;
	 	 
};

#endif  

#endif	 
