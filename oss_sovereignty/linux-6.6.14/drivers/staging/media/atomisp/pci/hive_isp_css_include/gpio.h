 
 

#ifndef __GPIO_H_INCLUDED__
#define __GPIO_H_INCLUDED__

 

#include "system_local.h"
#include "gpio_local.h"

#ifndef __INLINE_GPIO__
#define STORAGE_CLASS_GPIO_H extern
#define STORAGE_CLASS_GPIO_C
#include "gpio_public.h"
#else   
#define STORAGE_CLASS_GPIO_H static inline
#define STORAGE_CLASS_GPIO_C static inline
#include "gpio_private.h"
#endif  

#endif  
