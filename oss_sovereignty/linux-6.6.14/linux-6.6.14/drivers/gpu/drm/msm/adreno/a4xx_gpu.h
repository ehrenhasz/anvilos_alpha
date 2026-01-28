#ifndef __A4XX_GPU_H__
#define __A4XX_GPU_H__
#include "adreno_gpu.h"
#undef ROP_COPY
#undef ROP_XOR
#include "a4xx.xml.h"
struct a4xx_gpu {
	struct adreno_gpu base;
	struct adreno_ocmem ocmem;
};
#define to_a4xx_gpu(x) container_of(x, struct a4xx_gpu, base)
#endif  
