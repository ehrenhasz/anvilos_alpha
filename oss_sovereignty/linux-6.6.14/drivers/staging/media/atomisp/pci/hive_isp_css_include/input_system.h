 
 

#ifndef __INPUT_SYSTEM_H_INCLUDED__
#define __INPUT_SYSTEM_H_INCLUDED__

 

#include "system_local.h"
#include "input_system_local.h"

#ifndef __INLINE_INPUT_SYSTEM__
#define STORAGE_CLASS_INPUT_SYSTEM_H extern
#define STORAGE_CLASS_INPUT_SYSTEM_C
#include "input_system_public.h"
#else   
#define STORAGE_CLASS_INPUT_SYSTEM_H static inline
#define STORAGE_CLASS_INPUT_SYSTEM_C static inline
#include "input_system_private.h"
#endif  

#endif  
