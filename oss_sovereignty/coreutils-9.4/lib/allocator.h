 

#ifndef _GL_ALLOCATOR_H
#define _GL_ALLOCATOR_H

#include <stddef.h>

 

struct allocator
{
   
  void *(*allocate) (size_t);

   
  void *(*reallocate) (void *, size_t);

   
  void (*free) (void *);

   
  void (*die) (size_t);
};

 
extern struct allocator const stdlib_allocator;

#endif  
