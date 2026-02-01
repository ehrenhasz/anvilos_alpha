 

#ifndef _DYNARRAY_H
#define _DYNARRAY_H

#include <stddef.h>
#include <string.h>

struct dynarray_header
{
  size_t used;
  size_t allocated;
  void *array;
};

 
static inline size_t
__dynarray_error_marker (void)
{
  return -1;
}

 
static inline bool
__dynarray_error (struct dynarray_header *list)
{
  return list->allocated == __dynarray_error_marker ();
}

 
bool __libc_dynarray_emplace_enlarge (struct dynarray_header *,
                                      void *scratch, size_t element_size);

 
bool __libc_dynarray_resize (struct dynarray_header *, size_t size,
                             void *scratch, size_t element_size);

 
bool __libc_dynarray_resize_clear (struct dynarray_header *, size_t size,
                                   void *scratch, size_t element_size);

 
struct dynarray_finalize_result
{
  void *array;
  size_t length;
};

 
bool __libc_dynarray_finalize (struct dynarray_header *list, void *scratch,
                               size_t element_size,
                               struct dynarray_finalize_result *result);


 
_Noreturn void __libc_dynarray_at_failure (size_t size, size_t index);

#ifndef _ISOMAC
libc_hidden_proto (__libc_dynarray_emplace_enlarge)
libc_hidden_proto (__libc_dynarray_resize)
libc_hidden_proto (__libc_dynarray_resize_clear)
libc_hidden_proto (__libc_dynarray_finalize)
libc_hidden_proto (__libc_dynarray_at_failure)
#endif

#endif  
