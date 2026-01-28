#ifndef __CVMX_BOOT_VECTOR_H__
#define __CVMX_BOOT_VECTOR_H__
#include <asm/octeon/octeon.h>
#define OCTEON_BOOT_MOVEABLE_MAGIC1 0xdb00110ad358eacdull
struct cvmx_boot_vector_element {
	uint64_t target_ptr;
	uint64_t app0;
	uint64_t app1;
	uint64_t app2;
};
struct cvmx_boot_vector_element *cvmx_boot_vector_get(void);
#endif  
