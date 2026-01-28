#ifndef __IRQ_H_INCLUDED__
#define __IRQ_H_INCLUDED__
#include "system_local.h"
#include "irq_local.h"
#ifndef __INLINE_IRQ__
#define STORAGE_CLASS_IRQ_H extern
#define STORAGE_CLASS_IRQ_C
#include "irq_public.h"
#else   
#define STORAGE_CLASS_IRQ_H static inline
#define STORAGE_CLASS_IRQ_C static inline
#include "irq_private.h"
#endif  
#endif  
