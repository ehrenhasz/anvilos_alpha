 
 

#ifndef __FIFO_MONITOR_H_INCLUDED__
#define __FIFO_MONITOR_H_INCLUDED__

 

#include "system_local.h"
#include "fifo_monitor_local.h"

#ifndef __INLINE_FIFO_MONITOR__
#define STORAGE_CLASS_FIFO_MONITOR_H extern
#define STORAGE_CLASS_FIFO_MONITOR_C
#include "fifo_monitor_public.h"
#else   
#define STORAGE_CLASS_FIFO_MONITOR_H static inline
#define STORAGE_CLASS_FIFO_MONITOR_C static inline
#include "fifo_monitor_private.h"
#endif  

#endif  
