 
 

#ifndef __ISP_H_INCLUDED__
#define __ISP_H_INCLUDED__

 

#include "system_local.h"
#include "isp_local.h"

#ifndef __INLINE_ISP__
#define STORAGE_CLASS_ISP_H extern
#define STORAGE_CLASS_ISP_C
#include "isp_public.h"
#else   
#define STORAGE_CLASS_ISP_H static inline
#define STORAGE_CLASS_ISP_C static inline
#include "isp_private.h"
#endif  

#endif  
