 

 
#if !defined _LIBC && defined __GNU_LIBRARY__ && __GNU_LIBRARY__ > 1
# include <gnu-versions.h>
# if (_GNU_OBSTACK_INTERFACE_VERSION == _OBSTACK_INTERFACE_VERSION	      \
      || (_GNU_OBSTACK_INTERFACE_VERSION == 1				      \
          && _OBSTACK_INTERFACE_VERSION == 2				      \
          && defined SIZEOF_INT && defined SIZEOF_SIZE_T		      \
          && SIZEOF_INT == SIZEOF_SIZE_T))
#  define _OBSTACK_ELIDE_CODE
# endif
#endif

#ifndef _OBSTACK_ELIDE_CODE
 
# if !defined __GNUC__ && !defined __alignof__
#  include <alignof.h>
#  define __alignof__(type) alignof_type (type)
# endif
# include <stdlib.h>
# include <stdint.h>

# ifndef MAX
#  define MAX(a,b) ((a) > (b) ? (a) : (b))
# endif

 

 
#define DEFAULT_ALIGNMENT MAX (__alignof__ (long double),		      \
                               MAX (__alignof__ (uintmax_t),		      \
                                    __alignof__ (void *)))
#define DEFAULT_ROUNDING MAX (sizeof (long double),			      \
                               MAX (sizeof (uintmax_t),			      \
                                    sizeof (void *)))

 

static void *
call_chunkfun (struct obstack *h, size_t size)
{
  if (h->use_extra_arg)
    return h->chunkfun.extra (h->extra_arg, size);
  else
    return h->chunkfun.plain (size);
}

static void
call_freefun (struct obstack *h, void *old_chunk)
{
  if (h->use_extra_arg)
    h->freefun.extra (h->extra_arg, old_chunk);
  else
    h->freefun.plain (old_chunk);
}


 

static int
_obstack_begin_worker (struct obstack *h,
                       _OBSTACK_SIZE_T size, _OBSTACK_SIZE_T alignment)
{
  struct _obstack_chunk *chunk;  

  if (alignment == 0)
    alignment = DEFAULT_ALIGNMENT;
  if (size == 0)
     
    {
       
      int extra = ((((12 + DEFAULT_ROUNDING - 1) & ~(DEFAULT_ROUNDING - 1))
                    + 4 + DEFAULT_ROUNDING - 1)
                   & ~(DEFAULT_ROUNDING - 1));
      size = 4096 - extra;
    }

  h->chunk_size = size;
  h->alignment_mask = alignment - 1;

  chunk = h->chunk = call_chunkfun (h, h->chunk_size);
  if (!chunk)
    (*obstack_alloc_failed_handler) ();
  h->next_free = h->object_base = __PTR_ALIGN ((char *) chunk, chunk->contents,
                                               alignment - 1);
  h->chunk_limit = chunk->limit = (char *) chunk + h->chunk_size;
  chunk->prev = 0;
   
  h->maybe_empty_object = 0;
  h->alloc_failed = 0;
  return 1;
}

int
_obstack_begin (struct obstack *h,
                _OBSTACK_SIZE_T size, _OBSTACK_SIZE_T alignment,
                void *(*chunkfun) (size_t),
                void (*freefun) (void *))
{
  h->chunkfun.plain = chunkfun;
  h->freefun.plain = freefun;
  h->use_extra_arg = 0;
  return _obstack_begin_worker (h, size, alignment);
}

int
_obstack_begin_1 (struct obstack *h,
                  _OBSTACK_SIZE_T size, _OBSTACK_SIZE_T alignment,
                  void *(*chunkfun) (void *, size_t),
                  void (*freefun) (void *, void *),
                  void *arg)
{
  h->chunkfun.extra = chunkfun;
  h->freefun.extra = freefun;
  h->extra_arg = arg;
  h->use_extra_arg = 1;
  return _obstack_begin_worker (h, size, alignment);
}

 

void
_obstack_newchunk (struct obstack *h, _OBSTACK_SIZE_T length)
{
  struct _obstack_chunk *old_chunk = h->chunk;
  struct _obstack_chunk *new_chunk = 0;
  size_t obj_size = h->next_free - h->object_base;
  char *object_base;

   
  size_t sum1 = obj_size + length;
  size_t sum2 = sum1 + h->alignment_mask;
  size_t new_size = sum2 + (obj_size >> 3) + 100;
  if (new_size < sum2)
    new_size = sum2;
  if (new_size < h->chunk_size)
    new_size = h->chunk_size;

   
  if (obj_size <= sum1 && sum1 <= sum2)
    new_chunk = call_chunkfun (h, new_size);
  if (!new_chunk)
    (*obstack_alloc_failed_handler)();
  h->chunk = new_chunk;
  new_chunk->prev = old_chunk;
  new_chunk->limit = h->chunk_limit = (char *) new_chunk + new_size;

   
  object_base =
    __PTR_ALIGN ((char *) new_chunk, new_chunk->contents, h->alignment_mask);

   
  memcpy (object_base, h->object_base, obj_size);

   
  if (!h->maybe_empty_object
      && (h->object_base
          == __PTR_ALIGN ((char *) old_chunk, old_chunk->contents,
                          h->alignment_mask)))
    {
      new_chunk->prev = old_chunk->prev;
      call_freefun (h, old_chunk);
    }

  h->object_base = object_base;
  h->next_free = h->object_base + obj_size;
   
  h->maybe_empty_object = 0;
}

 

 
int _obstack_allocated_p (struct obstack *h, void *obj) __attribute_pure__;

int
_obstack_allocated_p (struct obstack *h, void *obj)
{
  struct _obstack_chunk *lp;     
  struct _obstack_chunk *plp;    

  lp = (h)->chunk;
   
  while (lp != 0 && ((void *) lp >= obj || (void *) (lp)->limit < obj))
    {
      plp = lp->prev;
      lp = plp;
    }
  return lp != 0;
}

 

void
_obstack_free (struct obstack *h, void *obj)
{
  struct _obstack_chunk *lp;     
  struct _obstack_chunk *plp;    

  lp = h->chunk;
   
  while (lp != 0 && ((void *) lp >= obj || (void *) (lp)->limit < obj))
    {
      plp = lp->prev;
      call_freefun (h, lp);
      lp = plp;
       
      h->maybe_empty_object = 1;
    }
  if (lp)
    {
      h->object_base = h->next_free = (char *) (obj);
      h->chunk_limit = lp->limit;
      h->chunk = lp;
    }
  else if (obj != 0)
     
    abort ();
}

_OBSTACK_SIZE_T
_obstack_memory_used (struct obstack *h)
{
  struct _obstack_chunk *lp;
  _OBSTACK_SIZE_T nbytes = 0;

  for (lp = h->chunk; lp != 0; lp = lp->prev)
    {
      nbytes += lp->limit - (char *) lp;
    }
  return nbytes;
}

# ifndef _OBSTACK_NO_ERROR_HANDLER
 
#  include <stdio.h>

 
#  ifdef _LIBC
int obstack_exit_failure = EXIT_FAILURE;
#  else
#   include "exitfail.h"
#   define obstack_exit_failure exit_failure
#  endif

#  ifdef _LIBC
#   include <libintl.h>
#  else
#   include "gettext.h"
#  endif
#  ifndef _
#   define _(msgid) gettext (msgid)
#  endif

#  ifdef _LIBC
#   include <libio/iolibio.h>
#  endif

static __attribute_noreturn__ void
print_and_abort (void)
{
   
#  ifdef _LIBC
  (void) __fxprintf (NULL, "%s\n", _("memory exhausted"));
#  else
  fprintf (stderr, "%s\n", _("memory exhausted"));
#  endif
  exit (obstack_exit_failure);
}

 
__attribute_noreturn__ void (*obstack_alloc_failed_handler) (void)
  = print_and_abort;
# endif  
#endif  
