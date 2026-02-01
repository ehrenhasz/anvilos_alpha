 

#ifndef _GL_DYNARRAY_H
#define _GL_DYNARRAY_H

 

 
#if 0
static void
       DYNARRAY_PREFIX##init (struct DYNARRAY_STRUCT *list);
#endif

 
#if 0
static void
       DYNARRAY_PREFIX##free (struct DYNARRAY_STRUCT *list);
#endif

 
#if 0
static bool
       DYNARRAY_PREFIX##has_failed (const struct DYNARRAY_STRUCT *list);
#endif

 
#if 0
static void
       DYNARRAY_PREFIX##mark_failed (struct DYNARRAY_STRUCT *list);
#endif

 
#if 0
static size_t
       DYNARRAY_PREFIX##size (const struct DYNARRAY_STRUCT *list);
#endif

 
#if 0
static DYNARRAY_ELEMENT *
       DYNARRAY_PREFIX##begin (const struct DYNARRAY_STRUCT *list);
#endif

 
#if 0
static DYNARRAY_ELEMENT *
       DYNARRAY_PREFIX##end (const struct DYNARRAY_STRUCT *list);
#endif

 
#if 0
static DYNARRAY_ELEMENT *
       DYNARRAY_PREFIX##at (struct DYNARRAY_STRUCT *list, size_t index);
#endif

 
#if 0
static void
       DYNARRAY_PREFIX##add (struct DYNARRAY_STRUCT *list,
                             DYNARRAY_ELEMENT item);
#endif

 
#if 0
static DYNARRAY_ELEMENT *
       DYNARRAY_PREFIX##emplace (struct DYNARRAY_STRUCT *list);
#endif

 
#if 0
static bool
       DYNARRAY_PREFIX##resize (struct DYNARRAY_STRUCT *list, size_t size);
#endif

 
#if 0
static void
       DYNARRAY_PREFIX##remove_last (struct DYNARRAY_STRUCT *list);
#endif

 
#if 0
static void
       DYNARRAY_PREFIX##clear (struct DYNARRAY_STRUCT *list);
#endif

#if defined DYNARRAY_FINAL_TYPE
 
#if 0
static bool
       DYNARRAY_PREFIX##finalize (struct DYNARRAY_STRUCT *list,
                                  DYNARRAY_FINAL_TYPE *result);
#endif
#else  
 
#if 0
static DYNARRAY_ELEMENT *
       DYNARRAY_PREFIX##finalize (struct DYNARRAY_STRUCT *list,
                                  size_t *lengthp);
#endif
#endif

 


 

 
#define __libc_dynarray_at_failure gl_dynarray_at_failure
#define __libc_dynarray_emplace_enlarge gl_dynarray_emplace_enlarge
#define __libc_dynarray_finalize gl_dynarray_finalize
#define __libc_dynarray_resize_clear gl_dynarray_resize_clear
#define __libc_dynarray_resize gl_dynarray_resize

#if defined DYNARRAY_STRUCT || defined DYNARRAY_ELEMENT || defined DYNARRAY_PREFIX

# ifndef _GL_LIKELY
 
#  define _GL_LIKELY(cond) __builtin_expect ((cond), 1)
#  define _GL_UNLIKELY(cond) __builtin_expect ((cond), 0)
# endif

 
# include <malloc/dynarray.gl.h>

 
# include <malloc/dynarray-skeleton.gl.h>

#else

 
# include <malloc/dynarray.h>

#endif

#endif  
