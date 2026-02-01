 

#include <malloc/dynarray.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifndef DYNARRAY_STRUCT
# error "DYNARRAY_STRUCT must be defined"
#endif

#ifndef DYNARRAY_ELEMENT
# error "DYNARRAY_ELEMENT must be defined"
#endif

#ifndef DYNARRAY_PREFIX
# error "DYNARRAY_PREFIX must be defined"
#endif

#ifdef DYNARRAY_INITIAL_SIZE
# if DYNARRAY_INITIAL_SIZE < 0
#  error "DYNARRAY_INITIAL_SIZE must be non-negative"
# endif
# if DYNARRAY_INITIAL_SIZE > 0
#  define DYNARRAY_HAVE_SCRATCH 1
# else
#  define DYNARRAY_HAVE_SCRATCH 0
# endif
#else
 
# define DYNARRAY_INITIAL_SIZE \
  (sizeof (DYNARRAY_ELEMENT) > 64 ? 2 : 128 / sizeof (DYNARRAY_ELEMENT))
# define DYNARRAY_HAVE_SCRATCH 1
#endif

 

 
struct DYNARRAY_STRUCT
{
  union
  {
    struct dynarray_header dynarray_abstract;
    struct
    {
       
      size_t used;
      size_t allocated;
      DYNARRAY_ELEMENT *array;
    } dynarray_header;
  } u;

#if DYNARRAY_HAVE_SCRATCH
   
  DYNARRAY_ELEMENT scratch[DYNARRAY_INITIAL_SIZE];
#endif
};

 

 
#define DYNARRAY_CONCAT0(prefix, name) prefix##name
#define DYNARRAY_CONCAT1(prefix, name) DYNARRAY_CONCAT0(prefix, name)
#define DYNARRAY_NAME(name) DYNARRAY_CONCAT1(DYNARRAY_PREFIX, name)

 
#define DYNARRAY_FREE DYNARRAY_CONCAT1 (DYNARRAY_NAME (f), ree)

 
#if DYNARRAY_HAVE_SCRATCH
# define DYNARRAY_SCRATCH(list) (list)->scratch
#else
# define DYNARRAY_SCRATCH(list) NULL
#endif

 

 
static inline void
DYNARRAY_NAME (free__elements__) (DYNARRAY_ELEMENT *__dynarray_array,
                                  size_t __dynarray_used)
{
#ifdef DYNARRAY_ELEMENT_FREE
  for (size_t __dynarray_i = 0; __dynarray_i < __dynarray_used; ++__dynarray_i)
    DYNARRAY_ELEMENT_FREE (&__dynarray_array[__dynarray_i]);
#endif  
}

 
static inline void
DYNARRAY_NAME (free__array__) (struct DYNARRAY_STRUCT *list)
{
#if DYNARRAY_HAVE_SCRATCH
  if (list->u.dynarray_header.array != list->scratch)
    free (list->u.dynarray_header.array);
#else
  free (list->u.dynarray_header.array);
#endif
}

 

 
__attribute_nonnull__ ((1))
static void
DYNARRAY_NAME (init) (struct DYNARRAY_STRUCT *list)
{
  list->u.dynarray_header.used = 0;
  list->u.dynarray_header.allocated = DYNARRAY_INITIAL_SIZE;
  list->u.dynarray_header.array = DYNARRAY_SCRATCH (list);
}

 
__attribute_maybe_unused__ __attribute_nonnull__ ((1))
static void
DYNARRAY_FREE (struct DYNARRAY_STRUCT *list)
{
  DYNARRAY_NAME (free__elements__)
    (list->u.dynarray_header.array, list->u.dynarray_header.used);
  DYNARRAY_NAME (free__array__) (list);
  DYNARRAY_NAME (init) (list);
}

 
__attribute_nonnull__ ((1))
static inline bool
DYNARRAY_NAME (has_failed) (const struct DYNARRAY_STRUCT *list)
{
  return list->u.dynarray_header.allocated == __dynarray_error_marker ();
}

 
__attribute_nonnull__ ((1))
static void
DYNARRAY_NAME (mark_failed) (struct DYNARRAY_STRUCT *list)
{
  DYNARRAY_NAME (free__elements__)
    (list->u.dynarray_header.array, list->u.dynarray_header.used);
  DYNARRAY_NAME (free__array__) (list);
  list->u.dynarray_header.array = DYNARRAY_SCRATCH (list);
  list->u.dynarray_header.used = 0;
  list->u.dynarray_header.allocated = __dynarray_error_marker ();
}

 
__attribute_nonnull__ ((1))
static inline size_t
DYNARRAY_NAME (size) (const struct DYNARRAY_STRUCT *list)
{
  return list->u.dynarray_header.used;
}

 
__attribute_nonnull__ ((1))
static inline DYNARRAY_ELEMENT *
DYNARRAY_NAME (at) (struct DYNARRAY_STRUCT *list, size_t index)
{
  if (__glibc_unlikely (index >= DYNARRAY_NAME (size) (list)))
    __libc_dynarray_at_failure (DYNARRAY_NAME (size) (list), index);
  return list->u.dynarray_header.array + index;
}

 
__attribute_nonnull__ ((1))
static inline DYNARRAY_ELEMENT *
DYNARRAY_NAME (begin) (struct DYNARRAY_STRUCT *list)
{
  return list->u.dynarray_header.array;
}

 
__attribute_nonnull__ ((1))
static inline DYNARRAY_ELEMENT *
DYNARRAY_NAME (end) (struct DYNARRAY_STRUCT *list)
{
  return list->u.dynarray_header.array + list->u.dynarray_header.used;
}

 
static void
DYNARRAY_NAME (add__) (struct DYNARRAY_STRUCT *list, DYNARRAY_ELEMENT item)
{
  if (__glibc_unlikely
      (!__libc_dynarray_emplace_enlarge (&list->u.dynarray_abstract,
                                         DYNARRAY_SCRATCH (list),
                                         sizeof (DYNARRAY_ELEMENT))))
    {
      DYNARRAY_NAME (mark_failed) (list);
      return;
    }

   
  list->u.dynarray_header.array[list->u.dynarray_header.used++] = item;
}

 
__attribute_nonnull__ ((1))
static inline void
DYNARRAY_NAME (add) (struct DYNARRAY_STRUCT *list, DYNARRAY_ELEMENT item)
{
   
  if (DYNARRAY_NAME (has_failed) (list))
    return;

   
  if (__glibc_unlikely (list->u.dynarray_header.used
                        == list->u.dynarray_header.allocated))
    {
      DYNARRAY_NAME (add__) (list, item);
      return;
    }

   
  list->u.dynarray_header.array[list->u.dynarray_header.used++] = item;
}

 
static inline DYNARRAY_ELEMENT *
DYNARRAY_NAME (emplace__tail__) (struct DYNARRAY_STRUCT *list)
{
  DYNARRAY_ELEMENT *result
    = &list->u.dynarray_header.array[list->u.dynarray_header.used];
  ++list->u.dynarray_header.used;
#if defined (DYNARRAY_ELEMENT_INIT)
  DYNARRAY_ELEMENT_INIT (result);
#elif defined (DYNARRAY_ELEMENT_FREE)
  memset (result, 0, sizeof (*result));
#endif
  return result;
}

 
static DYNARRAY_ELEMENT *
DYNARRAY_NAME (emplace__) (struct DYNARRAY_STRUCT *list)
{
  if (__glibc_unlikely
      (!__libc_dynarray_emplace_enlarge (&list->u.dynarray_abstract,
                                         DYNARRAY_SCRATCH (list),
                                         sizeof (DYNARRAY_ELEMENT))))
    {
      DYNARRAY_NAME (mark_failed) (list);
      return NULL;
    }
  return DYNARRAY_NAME (emplace__tail__) (list);
}

 
__attribute_maybe_unused__ __attribute_warn_unused_result__
__attribute_nonnull__ ((1))
static
 
#if !(defined (DYNARRAY_ELEMENT_INIT) || defined (DYNARRAY_ELEMENT_FREE))
inline
#endif
DYNARRAY_ELEMENT *
DYNARRAY_NAME (emplace) (struct DYNARRAY_STRUCT *list)
{
   
  if (DYNARRAY_NAME (has_failed) (list))
    return NULL;

   
  if (__glibc_unlikely (list->u.dynarray_header.used
                        == list->u.dynarray_header.allocated))
    return (DYNARRAY_NAME (emplace__) (list));
  return DYNARRAY_NAME (emplace__tail__) (list);
}

 
__attribute_maybe_unused__ __attribute_nonnull__ ((1))
static bool
DYNARRAY_NAME (resize) (struct DYNARRAY_STRUCT *list, size_t size)
{
  if (size > list->u.dynarray_header.used)
    {
      bool ok;
#if defined (DYNARRAY_ELEMENT_INIT)
       
      size_t old_size = list->u.dynarray_header.used;
      ok = __libc_dynarray_resize (&list->u.dynarray_abstract,
                                   size, DYNARRAY_SCRATCH (list),
                                   sizeof (DYNARRAY_ELEMENT));
      if (ok)
        for (size_t i = old_size; i < size; ++i)
          {
            DYNARRAY_ELEMENT_INIT (&list->u.dynarray_header.array[i]);
          }
#elif defined (DYNARRAY_ELEMENT_FREE)
       
      ok = __libc_dynarray_resize_clear
        (&list->u.dynarray_abstract, size,
         DYNARRAY_SCRATCH (list), sizeof (DYNARRAY_ELEMENT));
#else
      ok =  __libc_dynarray_resize (&list->u.dynarray_abstract,
                                    size, DYNARRAY_SCRATCH (list),
                                    sizeof (DYNARRAY_ELEMENT));
#endif
      if (__glibc_unlikely (!ok))
        DYNARRAY_NAME (mark_failed) (list);
      return ok;
    }
  else
    {
       
      DYNARRAY_NAME (free__elements__)
        (list->u.dynarray_header.array + size,
         list->u.dynarray_header.used - size);
      list->u.dynarray_header.used = size;
      return true;
    }
}

 
__attribute_maybe_unused__ __attribute_nonnull__ ((1))
static void
DYNARRAY_NAME (remove_last) (struct DYNARRAY_STRUCT *list)
{
   
  if (list->u.dynarray_header.used > 0)
    {
      size_t new_length = list->u.dynarray_header.used - 1;
#ifdef DYNARRAY_ELEMENT_FREE
      DYNARRAY_ELEMENT_FREE (&list->u.dynarray_header.array[new_length]);
#endif
      list->u.dynarray_header.used = new_length;
    }
}

 
__attribute_maybe_unused__ __attribute_nonnull__ ((1))
static void
DYNARRAY_NAME (clear) (struct DYNARRAY_STRUCT *list)
{
   
  DYNARRAY_NAME (free__elements__)
    (list->u.dynarray_header.array, list->u.dynarray_header.used);
  list->u.dynarray_header.used = 0;
}

#ifdef DYNARRAY_FINAL_TYPE
 
__attribute_maybe_unused__ __attribute_warn_unused_result__
__attribute_nonnull__ ((1, 2))
static bool
DYNARRAY_NAME (finalize) (struct DYNARRAY_STRUCT *list,
                          DYNARRAY_FINAL_TYPE *result)
{
  struct dynarray_finalize_result res;
  if (__libc_dynarray_finalize (&list->u.dynarray_abstract,
                                DYNARRAY_SCRATCH (list),
                                sizeof (DYNARRAY_ELEMENT), &res))
    {
       
      DYNARRAY_NAME (init) (list);
      *result = (DYNARRAY_FINAL_TYPE) { res.array, res.length };
      return true;
    }
  else
    {
       
      DYNARRAY_FREE (list);
      errno = ENOMEM;
      return false;
    }
}
#else  
 
__attribute_maybe_unused__ __attribute_warn_unused_result__
__attribute_nonnull__ ((1))
static DYNARRAY_ELEMENT *
DYNARRAY_NAME (finalize) (struct DYNARRAY_STRUCT *list, size_t *lengthp)
{
  struct dynarray_finalize_result res;
  if (__libc_dynarray_finalize (&list->u.dynarray_abstract,
                                DYNARRAY_SCRATCH (list),
                                sizeof (DYNARRAY_ELEMENT), &res))
    {
       
      DYNARRAY_NAME (init) (list);
      if (lengthp != NULL)
        *lengthp = res.length;
      return res.array;
    }
  else
    {
       
      DYNARRAY_FREE (list);
      errno = ENOMEM;
      return NULL;
    }
}
#endif  

 

#undef DYNARRAY_CONCAT0
#undef DYNARRAY_CONCAT1
#undef DYNARRAY_NAME
#undef DYNARRAY_SCRATCH
#undef DYNARRAY_HAVE_SCRATCH

#undef DYNARRAY_STRUCT
#undef DYNARRAY_ELEMENT
#undef DYNARRAY_PREFIX
#undef DYNARRAY_ELEMENT_FREE
#undef DYNARRAY_ELEMENT_INIT
#undef DYNARRAY_INITIAL_SIZE
#undef DYNARRAY_FINAL_TYPE
