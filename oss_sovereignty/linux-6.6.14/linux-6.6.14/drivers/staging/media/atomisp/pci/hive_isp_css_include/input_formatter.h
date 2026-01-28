#ifndef __INPUT_FORMATTER_H_INCLUDED__
#define __INPUT_FORMATTER_H_INCLUDED__
#include "system_local.h"
#include "input_formatter_local.h"
#ifndef __INLINE_INPUT_FORMATTER__
#define STORAGE_CLASS_INPUT_FORMATTER_H extern
#define STORAGE_CLASS_INPUT_FORMATTER_C
#include "input_formatter_public.h"
#else   
#define STORAGE_CLASS_INPUT_FORMATTER_H static inline
#define STORAGE_CLASS_INPUT_FORMATTER_C static inline
#include "input_formatter_private.h"
#endif  
#endif  
