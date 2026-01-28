#ifndef __GP_TIMER_H_INCLUDED__
#define __GP_TIMER_H_INCLUDED__
#include "system_local.h"     
#include "gp_timer_local.h"   
#ifndef __INLINE_GP_TIMER__
#define STORAGE_CLASS_GP_TIMER_H extern
#define STORAGE_CLASS_GP_TIMER_C
#include "gp_timer_public.h"    
#else   
#define STORAGE_CLASS_GP_TIMER_H static inline
#define STORAGE_CLASS_GP_TIMER_C static inline
#include "gp_timer_private.h"   
#endif  
#endif  
