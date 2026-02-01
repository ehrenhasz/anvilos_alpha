 
 

#ifndef __A3XX_GPU_H__
#define __A3XX_GPU_H__

#include "adreno_gpu.h"

 
#undef ROP_COPY
#undef ROP_XOR

#include "a3xx.xml.h"

struct a3xx_gpu {
	struct adreno_gpu base;

	 
	struct adreno_ocmem ocmem;
};
#define to_a3xx_gpu(x) container_of(x, struct a3xx_gpu, base)

#endif  
