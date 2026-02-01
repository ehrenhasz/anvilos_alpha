 

#ifndef _GL_SCRATCH_BUFFER_H
#define _GL_SCRATCH_BUFFER_H

 

 
struct scratch_buffer;

 
#if 0
extern void scratch_buffer_init (struct scratch_buffer *buffer);
#endif

 
#if 0
extern void scratch_buffer_free (struct scratch_buffer *buffer);
#endif

 
#if 0
extern bool scratch_buffer_grow (struct scratch_buffer *buffer);
#endif

 
#if 0
extern bool scratch_buffer_grow_preserve (struct scratch_buffer *buffer);
#endif

 
#if 0
extern bool scratch_buffer_set_array_size (struct scratch_buffer *buffer,
                                           size_t nelem, size_t size);
#endif


 

 
#define __libc_scratch_buffer_grow gl_scratch_buffer_grow
#define __libc_scratch_buffer_grow_preserve gl_scratch_buffer_grow_preserve
#define __libc_scratch_buffer_set_array_size gl_scratch_buffer_set_array_size

#ifndef _GL_LIKELY
 
# define _GL_LIKELY(cond) __builtin_expect ((cond), 1)
# define _GL_UNLIKELY(cond) __builtin_expect ((cond), 0)
#endif

#include <malloc/scratch_buffer.gl.h>

#endif  
