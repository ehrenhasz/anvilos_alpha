 
 

#ifndef __QUEUE_H_INCLUDED__
#define __QUEUE_H_INCLUDED__

 

#include "queue_local.h"

#ifndef __INLINE_QUEUE__
#define STORAGE_CLASS_QUEUE_H extern
#define STORAGE_CLASS_QUEUE_C
 
#include "ia_css_queue.h"
#else   
#define STORAGE_CLASS_QUEUE_H static inline
#define STORAGE_CLASS_QUEUE_C static inline
#include "queue_private.h"
#endif  

#endif  
