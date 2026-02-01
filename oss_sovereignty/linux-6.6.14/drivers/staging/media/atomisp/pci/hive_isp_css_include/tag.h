 
 

#ifndef __TAG_H_INCLUDED__
#define __TAG_H_INCLUDED__

 

#include "tag_local.h"

#ifndef __INLINE_TAG__
#define STORAGE_CLASS_TAG_H extern
#define STORAGE_CLASS_TAG_C
#include "tag_public.h"
#else   
#define STORAGE_CLASS_TAG_H static inline
#define STORAGE_CLASS_TAG_C static inline
#include "tag_private.h"
#endif  

#endif  
