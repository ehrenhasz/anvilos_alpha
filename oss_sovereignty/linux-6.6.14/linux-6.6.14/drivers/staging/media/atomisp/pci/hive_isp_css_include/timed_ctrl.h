#ifndef __TIMED_CTRL_H_INCLUDED__
#define __TIMED_CTRL_H_INCLUDED__
#include "system_local.h"
#include "timed_ctrl_local.h"
#ifndef __INLINE_TIMED_CTRL__
#define STORAGE_CLASS_TIMED_CTRL_H extern
#define STORAGE_CLASS_TIMED_CTRL_C
#include "timed_ctrl_public.h"
#else   
#define STORAGE_CLASS_TIMED_CTRL_H static inline
#define STORAGE_CLASS_TIMED_CTRL_C static inline
#include "timed_ctrl_private.h"
#endif  
#endif  
