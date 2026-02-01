 
 

#ifndef __EVENT_FIFO_H
#define __EVENT_FIFO_H

 

#include "system_local.h"
#include "event_fifo_local.h"

#ifndef __INLINE_EVENT__
#define STORAGE_CLASS_EVENT_H extern
#define STORAGE_CLASS_EVENT_C
#include "event_fifo_public.h"
#else   
#define STORAGE_CLASS_EVENT_H static inline
#define STORAGE_CLASS_EVENT_C static inline
#include "event_fifo_private.h"
#endif  

#endif  
