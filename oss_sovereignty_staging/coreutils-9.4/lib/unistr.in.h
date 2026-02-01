 
#include <stdbool.h>

 
#include <stddef.h>

 
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


 


 

 
extern const uint8_t *
       u8_check (const uint8_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;

 
extern const uint16_t *
       u16_check (const uint16_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;

 
extern const uint32_t *
       u32_check (const uint32_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;


 

 
extern uint16_t *
       u8_to_u16 (const uint8_t *s, size_t n, uint16_t *resultbuf,
                  size_t *lengthp);

 
extern uint32_t *
       u8_to_u32 (const uint8_t *s, size_t n, uint32_t *resultbuf,
                  size_t *lengthp);

 
extern uint8_t *
       u16_to_u8 (const uint16_t *s, size_t n, uint8_t *resultbuf,
                  size_t *lengthp);

 
extern uint32_t *
       u16_to_u32 (const uint16_t *s, size_t n, uint32_t *resultbuf,
                   size_t *lengthp);

 
extern uint8_t *
       u32_to_u8 (const uint32_t *s, size_t n, uint8_t *resultbuf,
                  size_t *lengthp);

 
extern uint16_t *
       u32_to_u16 (const uint32_t *s, size_t n, uint16_t *resultbuf,
                   size_t *lengthp);


 

 
 
extern int
       u8_mblen (const uint8_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;
extern int
       u16_mblen (const uint16_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_mblen (const uint32_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;

 
 
 

#if GNULIB_UNISTR_U8_MBTOUC_UNSAFE || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u8_mbtouc_unsafe (ucs4_t *puc, const uint8_t *s, size_t n);
# else
extern int
       u8_mbtouc_unsafe_aux (ucs4_t *puc, const uint8_t *s, size_t n);
static inline int
u8_mbtouc_unsafe (ucs4_t *puc, const uint8_t *s, size_t n)
{
  uint8_t c = *s;

  if (c < 0x80)
    {
      *puc = c;
      return 1;
    }
  else
    return u8_mbtouc_unsafe_aux (puc, s, n);
}
# endif
#endif

#if GNULIB_UNISTR_U16_MBTOUC_UNSAFE || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u16_mbtouc_unsafe (ucs4_t *puc, const uint16_t *s, size_t n);
# else
extern int
       u16_mbtouc_unsafe_aux (ucs4_t *puc, const uint16_t *s, size_t n);
static inline int
u16_mbtouc_unsafe (ucs4_t *puc, const uint16_t *s, size_t n)
{
  uint16_t c = *s;

  if (c < 0xd800 || c >= 0xe000)
    {
      *puc = c;
      return 1;
    }
  else
    return u16_mbtouc_unsafe_aux (puc, s, n);
}
# endif
#endif

#if GNULIB_UNISTR_U32_MBTOUC_UNSAFE || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u32_mbtouc_unsafe (ucs4_t *puc, const uint32_t *s, size_t n);
# else
static inline int
u32_mbtouc_unsafe (ucs4_t *puc,
                   const uint32_t *s, _GL_ATTRIBUTE_MAYBE_UNUSED size_t n)
{
  uint32_t c = *s;

  if (c < 0xd800 || (c >= 0xe000 && c < 0x110000))
    *puc = c;
  else
     
    *puc = 0xfffd;
  return 1;
}
# endif
#endif

#if GNULIB_UNISTR_U8_MBTOUC || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u8_mbtouc (ucs4_t *puc, const uint8_t *s, size_t n);
# else
extern int
       u8_mbtouc_aux (ucs4_t *puc, const uint8_t *s, size_t n);
static inline int
u8_mbtouc (ucs4_t *puc, const uint8_t *s, size_t n)
{
  uint8_t c = *s;

  if (c < 0x80)
    {
      *puc = c;
      return 1;
    }
  else
    return u8_mbtouc_aux (puc, s, n);
}
# endif
#endif

#if GNULIB_UNISTR_U16_MBTOUC || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u16_mbtouc (ucs4_t *puc, const uint16_t *s, size_t n);
# else
extern int
       u16_mbtouc_aux (ucs4_t *puc, const uint16_t *s, size_t n);
static inline int
u16_mbtouc (ucs4_t *puc, const uint16_t *s, size_t n)
{
  uint16_t c = *s;

  if (c < 0xd800 || c >= 0xe000)
    {
      *puc = c;
      return 1;
    }
  else
    return u16_mbtouc_aux (puc, s, n);
}
# endif
#endif

#if GNULIB_UNISTR_U32_MBTOUC || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u32_mbtouc (ucs4_t *puc, const uint32_t *s, size_t n);
# else
static inline int
u32_mbtouc (ucs4_t *puc, const uint32_t *s,
            _GL_ATTRIBUTE_MAYBE_UNUSED size_t n)
{
  uint32_t c = *s;

  if (c < 0xd800 || (c >= 0xe000 && c < 0x110000))
    *puc = c;
  else
     
    *puc = 0xfffd;
  return 1;
}
# endif
#endif

 
 

#if GNULIB_UNISTR_U8_MBTOUCR || HAVE_LIBUNISTRING
extern int
       u8_mbtoucr (ucs4_t *puc, const uint8_t *s, size_t n);
#endif

#if GNULIB_UNISTR_U16_MBTOUCR || HAVE_LIBUNISTRING
extern int
       u16_mbtoucr (ucs4_t *puc, const uint16_t *s, size_t n);
#endif

#if GNULIB_UNISTR_U32_MBTOUCR || HAVE_LIBUNISTRING
extern int
       u32_mbtoucr (ucs4_t *puc, const uint32_t *s, size_t n);
#endif

 
 

#if GNULIB_UNISTR_U8_UCTOMB || HAVE_LIBUNISTRING
 
extern int
       u8_uctomb_aux (uint8_t *s, ucs4_t uc, ptrdiff_t n);
# if !HAVE_INLINE
extern int
       u8_uctomb (uint8_t *s, ucs4_t uc, ptrdiff_t n);
# else
static inline int
u8_uctomb (uint8_t *s, ucs4_t uc, ptrdiff_t n)
{
  if (uc < 0x80 && n > 0)
    {
      s[0] = uc;
      return 1;
    }
  else
    return u8_uctomb_aux (s, uc, n);
}
# endif
#endif

#if GNULIB_UNISTR_U16_UCTOMB || HAVE_LIBUNISTRING
 
extern int
       u16_uctomb_aux (uint16_t *s, ucs4_t uc, ptrdiff_t n);
# if !HAVE_INLINE
extern int
       u16_uctomb (uint16_t *s, ucs4_t uc, ptrdiff_t n);
# else
static inline int
u16_uctomb (uint16_t *s, ucs4_t uc, ptrdiff_t n)
{
  if (uc < 0xd800 && n > 0)
    {
      s[0] = uc;
      return 1;
    }
  else
    return u16_uctomb_aux (s, uc, n);
}
# endif
#endif

#if GNULIB_UNISTR_U32_UCTOMB || HAVE_LIBUNISTRING
# if !HAVE_INLINE
extern int
       u32_uctomb (uint32_t *s, ucs4_t uc, ptrdiff_t n);
# else
static inline int
u32_uctomb (uint32_t *s, ucs4_t uc, ptrdiff_t n)
{
  if (uc < 0xd800 || (uc >= 0xe000 && uc < 0x110000))
    {
      if (n > 0)
        {
          *s = uc;
          return 1;
        }
      else
        return -2;
    }
  else
    return -1;
}
# endif
#endif

 
 
extern uint8_t *
       u8_cpy (uint8_t *_UC_RESTRICT dest, const uint8_t *src, size_t n);
extern uint16_t *
       u16_cpy (uint16_t *_UC_RESTRICT dest, const uint16_t *src, size_t n);
extern uint32_t *
       u32_cpy (uint32_t *_UC_RESTRICT dest, const uint32_t *src, size_t n);

 
 
extern uint8_t *
       u8_pcpy (uint8_t *_UC_RESTRICT dest, const uint8_t *src, size_t n);
extern uint16_t *
       u16_pcpy (uint16_t *_UC_RESTRICT dest, const uint16_t *src, size_t n);
extern uint32_t *
       u32_pcpy (uint32_t *_UC_RESTRICT dest, const uint32_t *src, size_t n);

 
 
extern uint8_t *
       u8_move (uint8_t *dest, const uint8_t *src, size_t n);
extern uint16_t *
       u16_move (uint16_t *dest, const uint16_t *src, size_t n);
extern uint32_t *
       u32_move (uint32_t *dest, const uint32_t *src, size_t n);

 
 
extern uint8_t *
       u8_set (uint8_t *s, ucs4_t uc, size_t n);
extern uint16_t *
       u16_set (uint16_t *s, ucs4_t uc, size_t n);
extern uint32_t *
       u32_set (uint32_t *s, ucs4_t uc, size_t n);

 
 
extern int
       u8_cmp (const uint8_t *s1, const uint8_t *s2, size_t n)
       _UC_ATTRIBUTE_PURE;
extern int
       u16_cmp (const uint16_t *s1, const uint16_t *s2, size_t n)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_cmp (const uint32_t *s1, const uint32_t *s2, size_t n)
       _UC_ATTRIBUTE_PURE;

 
 
extern int
       u8_cmp2 (const uint8_t *s1, size_t n1, const uint8_t *s2, size_t n2)
       _UC_ATTRIBUTE_PURE;
extern int
       u16_cmp2 (const uint16_t *s1, size_t n1, const uint16_t *s2, size_t n2)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_cmp2 (const uint32_t *s1, size_t n1, const uint32_t *s2, size_t n2)
       _UC_ATTRIBUTE_PURE;

 
 
extern uint8_t *
       u8_chr (const uint8_t *s, size_t n, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;
extern uint16_t *
       u16_chr (const uint16_t *s, size_t n, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;
extern uint32_t *
       u32_chr (const uint32_t *s, size_t n, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;

 
 
extern size_t
       u8_mbsnlen (const uint8_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u16_mbsnlen (const uint16_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u32_mbsnlen (const uint32_t *s, size_t n)
       _UC_ATTRIBUTE_PURE;

 

 
extern uint8_t *
       u8_cpy_alloc (const uint8_t *s, size_t n);
extern uint16_t *
       u16_cpy_alloc (const uint16_t *s, size_t n);
extern uint32_t *
       u32_cpy_alloc (const uint32_t *s, size_t n);

 

 
extern int
       u8_strmblen (const uint8_t *s)
       _UC_ATTRIBUTE_PURE;
extern int
       u16_strmblen (const uint16_t *s)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_strmblen (const uint32_t *s)
       _UC_ATTRIBUTE_PURE;

 
extern int
       u8_strmbtouc (ucs4_t *puc, const uint8_t *s);
extern int
       u16_strmbtouc (ucs4_t *puc, const uint16_t *s);
extern int
       u32_strmbtouc (ucs4_t *puc, const uint32_t *s);

 
extern const uint8_t *
       u8_next (ucs4_t *puc, const uint8_t *s);
extern const uint16_t *
       u16_next (ucs4_t *puc, const uint16_t *s);
extern const uint32_t *
       u32_next (ucs4_t *puc, const uint32_t *s);

 
extern const uint8_t *
       u8_prev (ucs4_t *puc, const uint8_t *s, const uint8_t *start);
extern const uint16_t *
       u16_prev (ucs4_t *puc, const uint16_t *s, const uint16_t *start);
extern const uint32_t *
       u32_prev (ucs4_t *puc, const uint32_t *s, const uint32_t *start);

 
 
extern size_t
       u8_strlen (const uint8_t *s)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u16_strlen (const uint16_t *s)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u32_strlen (const uint32_t *s)
       _UC_ATTRIBUTE_PURE;

 
 
extern size_t
       u8_strnlen (const uint8_t *s, size_t maxlen)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u16_strnlen (const uint16_t *s, size_t maxlen)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u32_strnlen (const uint32_t *s, size_t maxlen)
       _UC_ATTRIBUTE_PURE;

 
 
extern uint8_t *
       u8_strcpy (uint8_t *_UC_RESTRICT dest, const uint8_t *src);
extern uint16_t *
       u16_strcpy (uint16_t *_UC_RESTRICT dest, const uint16_t *src);
extern uint32_t *
       u32_strcpy (uint32_t *_UC_RESTRICT dest, const uint32_t *src);

 
 
extern uint8_t *
       u8_stpcpy (uint8_t *_UC_RESTRICT dest, const uint8_t *src);
extern uint16_t *
       u16_stpcpy (uint16_t *_UC_RESTRICT dest, const uint16_t *src);
extern uint32_t *
       u32_stpcpy (uint32_t *_UC_RESTRICT dest, const uint32_t *src);

 
 
extern uint8_t *
       u8_strncpy (uint8_t *_UC_RESTRICT dest, const uint8_t *src, size_t n);
extern uint16_t *
       u16_strncpy (uint16_t *_UC_RESTRICT dest, const uint16_t *src, size_t n);
extern uint32_t *
       u32_strncpy (uint32_t *_UC_RESTRICT dest, const uint32_t *src, size_t n);

 
 
extern uint8_t *
       u8_stpncpy (uint8_t *_UC_RESTRICT dest, const uint8_t *src, size_t n);
extern uint16_t *
       u16_stpncpy (uint16_t *_UC_RESTRICT dest, const uint16_t *src, size_t n);
extern uint32_t *
       u32_stpncpy (uint32_t *_UC_RESTRICT dest, const uint32_t *src, size_t n);

 
 
extern uint8_t *
       u8_strcat (uint8_t *_UC_RESTRICT dest, const uint8_t *src);
extern uint16_t *
       u16_strcat (uint16_t *_UC_RESTRICT dest, const uint16_t *src);
extern uint32_t *
       u32_strcat (uint32_t *_UC_RESTRICT dest, const uint32_t *src);

 
 
extern uint8_t *
       u8_strncat (uint8_t *_UC_RESTRICT dest, const uint8_t *src, size_t n);
extern uint16_t *
       u16_strncat (uint16_t *_UC_RESTRICT dest, const uint16_t *src, size_t n);
extern uint32_t *
       u32_strncat (uint32_t *_UC_RESTRICT dest, const uint32_t *src, size_t n);

 
 
#ifdef __sun
 
extern int
       u8_strcmp_gnu (const uint8_t *s1, const uint8_t *s2)
       _UC_ATTRIBUTE_PURE;
# define u8_strcmp u8_strcmp_gnu
#else
extern int
       u8_strcmp (const uint8_t *s1, const uint8_t *s2)
       _UC_ATTRIBUTE_PURE;
#endif
extern int
       u16_strcmp (const uint16_t *s1, const uint16_t *s2)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_strcmp (const uint32_t *s1, const uint32_t *s2)
       _UC_ATTRIBUTE_PURE;

 
 
extern int
       u8_strcoll (const uint8_t *s1, const uint8_t *s2);
extern int
       u16_strcoll (const uint16_t *s1, const uint16_t *s2);
extern int
       u32_strcoll (const uint32_t *s1, const uint32_t *s2);

 
 
extern int
       u8_strncmp (const uint8_t *s1, const uint8_t *s2, size_t n)
       _UC_ATTRIBUTE_PURE;
extern int
       u16_strncmp (const uint16_t *s1, const uint16_t *s2, size_t n)
       _UC_ATTRIBUTE_PURE;
extern int
       u32_strncmp (const uint32_t *s1, const uint32_t *s2, size_t n)
       _UC_ATTRIBUTE_PURE;

 
 
extern uint8_t *
       u8_strdup (const uint8_t *s)
       _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;
extern uint16_t *
       u16_strdup (const uint16_t *s)
       _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;
extern uint32_t *
       u32_strdup (const uint32_t *s)
       _GL_ATTRIBUTE_MALLOC _GL_ATTRIBUTE_DEALLOC_FREE;

 
 
extern uint8_t *
       u8_strchr (const uint8_t *str, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;
extern uint16_t *
       u16_strchr (const uint16_t *str, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;
extern uint32_t *
       u32_strchr (const uint32_t *str, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;

 
 
extern uint8_t *
       u8_strrchr (const uint8_t *str, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;
extern uint16_t *
       u16_strrchr (const uint16_t *str, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;
extern uint32_t *
       u32_strrchr (const uint32_t *str, ucs4_t uc)
       _UC_ATTRIBUTE_PURE;

 
 
extern size_t
       u8_strcspn (const uint8_t *str, const uint8_t *reject)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u16_strcspn (const uint16_t *str, const uint16_t *reject)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u32_strcspn (const uint32_t *str, const uint32_t *reject)
       _UC_ATTRIBUTE_PURE;

 
 
extern size_t
       u8_strspn (const uint8_t *str, const uint8_t *accept)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u16_strspn (const uint16_t *str, const uint16_t *accept)
       _UC_ATTRIBUTE_PURE;
extern size_t
       u32_strspn (const uint32_t *str, const uint32_t *accept)
       _UC_ATTRIBUTE_PURE;

 
 
extern uint8_t *
       u8_strpbrk (const uint8_t *str, const uint8_t *accept)
       _UC_ATTRIBUTE_PURE;
extern uint16_t *
       u16_strpbrk (const uint16_t *str, const uint16_t *accept)
       _UC_ATTRIBUTE_PURE;
extern uint32_t *
       u32_strpbrk (const uint32_t *str, const uint32_t *accept)
       _UC_ATTRIBUTE_PURE;

 
 
extern uint8_t *
       u8_strstr (const uint8_t *haystack, const uint8_t *needle)
       _UC_ATTRIBUTE_PURE;
extern uint16_t *
       u16_strstr (const uint16_t *haystack, const uint16_t *needle)
       _UC_ATTRIBUTE_PURE;
extern uint32_t *
       u32_strstr (const uint32_t *haystack, const uint32_t *needle)
       _UC_ATTRIBUTE_PURE;

 
extern bool
       u8_startswith (const uint8_t *str, const uint8_t *prefix)
       _UC_ATTRIBUTE_PURE;
extern bool
       u16_startswith (const uint16_t *str, const uint16_t *prefix)
       _UC_ATTRIBUTE_PURE;
extern bool
       u32_startswith (const uint32_t *str, const uint32_t *prefix)
       _UC_ATTRIBUTE_PURE;

 
extern bool
       u8_endswith (const uint8_t *str, const uint8_t *suffix)
       _UC_ATTRIBUTE_PURE;
extern bool
       u16_endswith (const uint16_t *str, const uint16_t *suffix)
       _UC_ATTRIBUTE_PURE;
extern bool
       u32_endswith (const uint32_t *str, const uint32_t *suffix)
       _UC_ATTRIBUTE_PURE;

 
 
extern uint8_t *
       u8_strtok (uint8_t *_UC_RESTRICT str, const uint8_t *delim,
                  uint8_t **ptr);
extern uint16_t *
       u16_strtok (uint16_t *_UC_RESTRICT str, const uint16_t *delim,
                   uint16_t **ptr);
extern uint32_t *
       u32_strtok (uint32_t *_UC_RESTRICT str, const uint32_t *delim,
                   uint32_t **ptr);


#ifdef __cplusplus
}
#endif

#endif  
