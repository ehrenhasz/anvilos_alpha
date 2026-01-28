#ifndef __ISYS_STREAM2MMIO_GLOBAL_H_INCLUDED__
#define __ISYS_STREAM2MMIO_GLOBAL_H_INCLUDED__
#include <type_support.h>
typedef struct stream2mmio_cfg_s stream2mmio_cfg_t;
struct stream2mmio_cfg_s {
	u32				bits_per_pixel;
	u32				enable_blocking;
};
extern const stream2mmio_sid_ID_t N_STREAM2MMIO_SID_PROCS[N_STREAM2MMIO_ID];
#endif  
