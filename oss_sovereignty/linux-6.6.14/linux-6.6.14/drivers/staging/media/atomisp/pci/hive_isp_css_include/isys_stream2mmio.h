#ifndef __ISYS_STREAM2MMIO_H_INCLUDED__
#define __ISYS_STREAM2MMIO_H_INCLUDED__
#include "system_local.h"
#include "isys_stream2mmio_local.h"
#ifndef __INLINE_STREAM2MMIO__
#define STORAGE_CLASS_STREAM2MMIO_H extern
#define STORAGE_CLASS_STREAM2MMIO_C
#include "isys_stream2mmio_public.h"
#else   
#define STORAGE_CLASS_STREAM2MMIO_H static inline
#define STORAGE_CLASS_STREAM2MMIO_C static inline
#include "isys_stream2mmio_private.h"
#endif  
#endif  
