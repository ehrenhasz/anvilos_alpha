 

#ifndef _OS_TYPES_H_
#define _OS_TYPES_H_

#include <linux/slab.h>
#include <linux/kgdb.h>
#include <linux/kref.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/mm.h>

#include <asm/byteorder.h>

#include <drm/display/drm_dp_helper.h>
#include <drm/drm_print.h>

#include "cgs_common.h"

#if defined(__BIG_ENDIAN) && !defined(BIGENDIAN_CPU)
#define BIGENDIAN_CPU
#elif defined(__LITTLE_ENDIAN) && !defined(LITTLEENDIAN_CPU)
#define LITTLEENDIAN_CPU
#endif

#undef FRAME_SIZE

#define dm_output_to_console(fmt, ...) DRM_DEBUG_KMS(fmt, ##__VA_ARGS__)

#define dm_error(fmt, ...) DRM_ERROR(fmt, ##__VA_ARGS__)

#if defined(CONFIG_DRM_AMD_DC_FP)
#include "amdgpu_dm/dc_fpu.h"
#define DC_FP_START() dc_fpu_begin(__func__, __LINE__)
#define DC_FP_END() dc_fpu_end(__func__, __LINE__)
#endif  

 
#ifdef CONFIG_DEBUG_KERNEL_DC
#define dc_breakpoint()		kgdb_breakpoint()
#else
#define dc_breakpoint()		do {} while (0)
#endif

#define ASSERT_CRITICAL(expr) do {		\
		if (WARN_ON(!(expr)))		\
			dc_breakpoint();	\
	} while (0)

#define ASSERT(expr) do {			\
		if (WARN_ON_ONCE(!(expr)))	\
			dc_breakpoint();	\
	} while (0)

#define BREAK_TO_DEBUGGER() \
	do { \
		DRM_DEBUG_DRIVER("%s():%d\n", __func__, __LINE__); \
		dc_breakpoint(); \
	} while (0)

#define DC_ERR(...)  do { \
	dm_error(__VA_ARGS__); \
	BREAK_TO_DEBUGGER(); \
} while (0)

#endif  
