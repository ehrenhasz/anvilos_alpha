 
 

#ifndef __GP_DEVICE_H_INCLUDED__
#define __GP_DEVICE_H_INCLUDED__

 

#include "system_local.h"
#include "gp_device_local.h"

#ifndef __INLINE_GP_DEVICE__
#define STORAGE_CLASS_GP_DEVICE_H extern
#define STORAGE_CLASS_GP_DEVICE_C
#include "gp_device_public.h"
#else   
#define STORAGE_CLASS_GP_DEVICE_H static inline
#define STORAGE_CLASS_GP_DEVICE_C static inline
#include "gp_device_private.h"
#endif  

#endif  
