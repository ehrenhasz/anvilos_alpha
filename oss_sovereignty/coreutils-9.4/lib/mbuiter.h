 

 

#ifndef _MBUITER_H
#define _MBUITER_H 1

 
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
#ifndef MBUITER_INLINE
# define MBUITER_INLINE _GL_INLINE _GL_ATTRIBUTE_ALWAYS_INLINE
#endif

struct mbuiter_multi
{
  #if !GNULIB_MBRTOC32_REGULAR
  bool in_shift;         
                         
  #endif
  mbstate_t state;       
                         
  bool next_done;        
  unsigned int cur_max;  
  struct mbchar cur;     
};

MBUITER_INLINE void
mbuiter_multi_next (struct mbuiter_multi *iter)
{
  if (iter->next_done)
    return;
  #if !GNULIB_MBRTOC32_REGULAR
  if (iter->in_shift)
    goto with_shift;
  #endif
   
  if (is_basic (*iter->cur.ptr))
    {
       
      iter->cur.bytes = 1;
      iter->cur.wc = *iter->cur.ptr;
      iter->cur.wc_valid = true;
    }
  else
    {
      assert (mbsinit (&iter->state));
      #if !GNULIB_MBRTOC32_REGULAR
      iter->in_shift = true;
    with_shift:
      #endif
      iter->cur.bytes = mbrtoc32 (&iter->cur.wc, iter->cur.ptr,
                                  strnlen1 (iter->cur.ptr, iter->cur_max),
                                  &iter->state);
      if (iter->cur.bytes == (size_t) -1)
        {
           
          iter->cur.bytes = 1;
          iter->cur.wc_valid = false;
           
          #if !GNULIB_MBRTOC32_REGULAR
          iter->in_shift = false;
          #endif
          mbszero (&iter->state);
        }
      else if (iter->cur.bytes == (size_t) -2)
        {
           
          iter->cur.bytes = strlen (iter->cur.ptr);
          iter->cur.wc_valid = false;
           
        }
      else
        {
          if (iter->cur.bytes == 0)
            {
               
              iter->cur.bytes = 1;
              assert (*iter->cur.ptr == '\0');
              assert (iter->cur.wc == 0);
            }
          #if !GNULIB_MBRTOC32_REGULAR
          else if (iter->cur.bytes == (size_t) -3)
             
            iter->cur.bytes = 0;
          #endif
          iter->cur.wc_valid = true;

           
          #if !GNULIB_MBRTOC32_REGULAR
          if (mbsinit (&iter->state))
            iter->in_shift = false;
          #endif
        }
    }
  iter->next_done = true;
}

MBUITER_INLINE void
mbuiter_multi_reloc (struct mbuiter_multi *iter, ptrdiff_t ptrdiff)
{
  iter->cur.ptr += ptrdiff;
}

MBUITER_INLINE void
mbuiter_multi_copy (struct mbuiter_multi *new_iter, const struct mbuiter_multi *old_iter)
{
  #if !GNULIB_MBRTOC32_REGULAR
  if ((new_iter->in_shift = old_iter->in_shift))
    memcpy (&new_iter->state, &old_iter->state, sizeof (mbstate_t));
  else
  #endif
    mbszero (&new_iter->state);
  new_iter->next_done = old_iter->next_done;
  new_iter->cur_max = old_iter->cur_max;
  mb_copy (&new_iter->cur, &old_iter->cur);
}

 
typedef struct mbuiter_multi mbui_iterator_t;
#if !GNULIB_MBRTOC32_REGULAR
#define mbui_init(iter, startptr) \
  ((iter).cur.ptr = (startptr), \
   (iter).in_shift = false, mbszero (&(iter).state), \
   (iter).next_done = false, \
   (iter).cur_max = MB_CUR_MAX)
#else
 
#define mbui_init(iter, startptr) \
  ((iter).cur.ptr = (startptr), \
   mbszero (&(iter).state), \
   (iter).next_done = false, \
   (iter).cur_max = MB_CUR_MAX)
#endif
#define mbui_avail(iter) \
  (mbuiter_multi_next (&(iter)), !mb_isnul ((iter).cur))
#define mbui_advance(iter) \
  ((iter).cur.ptr += (iter).cur.bytes, (iter).next_done = false)

 
#define mbui_cur(iter) (iter).cur
#define mbui_cur_ptr(iter) (iter).cur.ptr

 
#define mbui_reloc(iter, ptrdiff) mbuiter_multi_reloc (&iter, ptrdiff)

 
#define mbui_copy mbuiter_multi_copy

_GL_INLINE_HEADER_END

#endif  
