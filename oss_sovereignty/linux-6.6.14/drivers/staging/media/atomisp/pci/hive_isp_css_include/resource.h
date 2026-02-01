 
 

#ifndef __RESOURCE_H_INCLUDED__
#define __RESOURCE_H_INCLUDED__

 

#include "system_local.h"
#include "resource_local.h"

#ifndef __INLINE_RESOURCE__
#define STORAGE_CLASS_RESOURCE_H extern
#define STORAGE_CLASS_RESOURCE_C
#include "resource_public.h"
#else   
#define STORAGE_CLASS_RESOURCE_H static inline
#define STORAGE_CLASS_RESOURCE_C static inline
#include "resource_private.h"
#endif  

#endif  
