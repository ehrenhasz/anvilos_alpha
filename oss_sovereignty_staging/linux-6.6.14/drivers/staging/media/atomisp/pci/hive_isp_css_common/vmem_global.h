 
 

#ifndef __VMEM_GLOBAL_H_INCLUDED__
#define __VMEM_GLOBAL_H_INCLUDED__

#include "isp.h"

#define VMEM_SIZE	ISP_VMEM_DEPTH
#define VMEM_ELEMBITS	ISP_VMEM_ELEMBITS
#define VMEM_ALIGN	ISP_VMEM_ALIGN

#ifndef PIPE_GENERATION
typedef tvector *pvector;
#endif

#endif  
