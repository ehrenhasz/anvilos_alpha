 

#ifndef _GL_BITROTATE_H
#define _GL_BITROTATE_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <limits.h>
#include <stdint.h>
#include <sys/types.h>

_GL_INLINE_HEADER_BEGIN
#ifndef BITROTATE_INLINE
# define BITROTATE_INLINE _GL_INLINE
#endif

#ifdef UINT64_MAX
 
BITROTATE_INLINE uint64_t
rotl64 (uint64_t x, int n)
{
  return ((x << n) | (x >> (64 - n))) & UINT64_MAX;
}

 
BITROTATE_INLINE uint64_t
rotr64 (uint64_t x, int n)
{
  return ((x >> n) | (x << (64 - n))) & UINT64_MAX;
}
#endif

 
BITROTATE_INLINE uint32_t
rotl32 (uint32_t x, int n)
{
  return ((x << n) | (x >> (32 - n))) & UINT32_MAX;
}

 
BITROTATE_INLINE uint32_t
rotr32 (uint32_t x, int n)
{
  return ((x >> n) | (x << (32 - n))) & UINT32_MAX;
}

 
BITROTATE_INLINE size_t
rotl_sz (size_t x, int n)
{
  return ((x << n) | (x >> ((CHAR_BIT * sizeof x) - n))) & SIZE_MAX;
}

 
BITROTATE_INLINE size_t
rotr_sz (size_t x, int n)
{
  return ((x >> n) | (x << ((CHAR_BIT * sizeof x) - n))) & SIZE_MAX;
}

 
BITROTATE_INLINE uint16_t
rotl16 (uint16_t x, int n)
{
  return (((unsigned int) x << n) | ((unsigned int) x >> (16 - n)))
         & UINT16_MAX;
}

 
BITROTATE_INLINE uint16_t
rotr16 (uint16_t x, int n)
{
  return (((unsigned int) x >> n) | ((unsigned int) x << (16 - n)))
         & UINT16_MAX;
}

 
BITROTATE_INLINE uint8_t
rotl8 (uint8_t x, int n)
{
  return (((unsigned int) x << n) | ((unsigned int) x >> (8 - n))) & UINT8_MAX;
}

 
BITROTATE_INLINE uint8_t
rotr8 (uint8_t x, int n)
{
  return (((unsigned int) x >> n) | ((unsigned int) x << (8 - n))) & UINT8_MAX;
}

_GL_INLINE_HEADER_END

#endif  
