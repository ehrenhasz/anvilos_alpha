 

 

#ifndef _MBUITERF_H
#define _MBUITERF_H 1

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <wchar.h>

#include "mbchar.h"
#include "strnlen1.h"

_GL_INLINE_HEADER_BEGIN
#ifndef MBUITERF_INLINE
# define MBUITERF_INLINE _GL_INLINE _GL_ATTRIBUTE_ALWAYS_INLINE
#endif

struct mbuif_state
{
  #if !GNULIB_MBRTOC32_REGULAR
  bool in_shift;         
                         
  #endif
  mbstate_t state;       
                         
  unsigned int cur_max;  
};

MBUITERF_INLINE mbchar_t
mbuiterf_next (struct mbuif_state *ps, const char *iter)
{
  #if !GNULIB_MBRTOC32_REGULAR
  if (ps->in_shift)
    goto with_shift;
  #endif
   
  if (is_basic (*iter))
    {
       
      return (mbchar_t) { .ptr = iter, .bytes = 1, .wc_valid = true, .wc = *iter };
    }
  else
    {
      assert (mbsinit (&ps->state));
      #if !GNULIB_MBRTOC32_REGULAR
      ps->in_shift = true;
    with_shift:;
      #endif
      size_t bytes;
      char32_t wc;
      bytes = mbrtoc32 (&wc, iter, strnlen1 (iter, ps->cur_max), &ps->state);
      if (bytes == (size_t) -1)
        {
           
           
          #if !GNULIB_MBRTOC32_REGULAR
          ps->in_shift = false;
          #endif
          mbszero (&ps->state);
          return (mbchar_t) { .ptr = iter, .bytes = 1, .wc_valid = false };
        }
      else if (bytes == (size_t) -2)
        {
           
           
          return (mbchar_t) { .ptr = iter, .bytes = strlen (iter), .wc_valid = false };
        }
      else
        {
          if (bytes == 0)
            {
               
              bytes = 1;
              assert (*iter == '\0');
              assert (wc == 0);
            }
          #if !GNULIB_MBRTOC32_REGULAR
          else if (bytes == (size_t) -3)
             
            bytes = 0;
          #endif

           
          #if !GNULIB_MBRTOC32_REGULAR
          if (mbsinit (&ps->state))
            ps->in_shift = false;
          #endif
          return (mbchar_t) { .ptr = iter, .bytes = bytes, .wc_valid = true, .wc = wc };
        }
    }
}

 
typedef struct mbuif_state mbuif_state_t;
#if !GNULIB_MBRTOC32_REGULAR
#define mbuif_init(st) \
  ((st).in_shift = false, mbszero (&(st).state), \
   (st).cur_max = MB_CUR_MAX)
#else
 
#define mbuif_init(st) \
  (mbszero (&(st).state), \
   (st).cur_max = MB_CUR_MAX)
#endif
#if !GNULIB_MBRTOC32_REGULAR
#define mbuif_avail(st, iter) ((st).in_shift || (*(iter) != '\0'))
#else
 
#define mbuif_avail(st, iter) (*(iter) != '\0')
#endif
#define mbuif_next(st, iter) \
  mbuiterf_next (&(st), (iter))

_GL_INLINE_HEADER_END

#endif  
