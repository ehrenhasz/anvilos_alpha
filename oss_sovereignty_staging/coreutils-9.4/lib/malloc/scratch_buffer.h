 

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

 
struct scratch_buffer {
  void *data;     
  size_t length;  
  union { max_align_t __align; char __c[1024]; } __space;
};

 
static inline void
scratch_buffer_init (struct scratch_buffer *buffer)
{
  buffer->data = buffer->__space.__c;
  buffer->length = sizeof (buffer->__space);
}

 
static inline void
scratch_buffer_free (struct scratch_buffer *buffer)
{
  if (buffer->data != buffer->__space.__c)
    free (buffer->data);
}

 
bool __libc_scratch_buffer_grow (struct scratch_buffer *buffer);
libc_hidden_proto (__libc_scratch_buffer_grow)

 
static __always_inline bool
scratch_buffer_grow (struct scratch_buffer *buffer)
{
  return __glibc_likely (__libc_scratch_buffer_grow (buffer));
}

 
bool __libc_scratch_buffer_grow_preserve (struct scratch_buffer *buffer);
libc_hidden_proto (__libc_scratch_buffer_grow_preserve)

 
static __always_inline bool
scratch_buffer_grow_preserve (struct scratch_buffer *buffer)
{
  return __glibc_likely (__libc_scratch_buffer_grow_preserve (buffer));
}

 
bool __libc_scratch_buffer_set_array_size (struct scratch_buffer *buffer,
					   size_t nelem, size_t size);
libc_hidden_proto (__libc_scratch_buffer_set_array_size)

 
static __always_inline bool
scratch_buffer_set_array_size (struct scratch_buffer *buffer,
			       size_t nelem, size_t size)
{
  return __glibc_likely (__libc_scratch_buffer_set_array_size
			 (buffer, nelem, size));
}

#endif  
