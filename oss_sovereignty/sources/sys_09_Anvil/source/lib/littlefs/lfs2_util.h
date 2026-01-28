
#ifndef LFS2_UTIL_H
#define LFS2_UTIL_H







#ifdef LFS2_CONFIG
#define LFS2_STRINGIZE(x) LFS2_STRINGIZE2(x)
#define LFS2_STRINGIZE2(x) #x
#include LFS2_STRINGIZE(LFS2_CONFIG)
#else


#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

#ifndef LFS2_NO_MALLOC
#include <stdlib.h>
#endif
#ifndef LFS2_NO_ASSERT
#include <assert.h>
#endif
#if !defined(LFS2_NO_DEBUG) || \
        !defined(LFS2_NO_WARN) || \
        !defined(LFS2_NO_ERROR) || \
        defined(LFS2_YES_TRACE)
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif







#ifndef LFS2_TRACE
#ifdef LFS2_YES_TRACE
#define LFS2_TRACE_(fmt, ...) \
    printf("%s:%d:trace: " fmt "%s\n", __FILE__, __LINE__, __VA_ARGS__)
#define LFS2_TRACE(...) LFS2_TRACE_(__VA_ARGS__, "")
#else
#define LFS2_TRACE(...)
#endif
#endif

#ifndef LFS2_DEBUG
#ifndef LFS2_NO_DEBUG
#define LFS2_DEBUG_(fmt, ...) \
    printf("%s:%d:debug: " fmt "%s\n", __FILE__, __LINE__, __VA_ARGS__)
#define LFS2_DEBUG(...) LFS2_DEBUG_(__VA_ARGS__, "")
#else
#define LFS2_DEBUG(...)
#endif
#endif

#ifndef LFS2_WARN
#ifndef LFS2_NO_WARN
#define LFS2_WARN_(fmt, ...) \
    printf("%s:%d:warn: " fmt "%s\n", __FILE__, __LINE__, __VA_ARGS__)
#define LFS2_WARN(...) LFS2_WARN_(__VA_ARGS__, "")
#else
#define LFS2_WARN(...)
#endif
#endif

#ifndef LFS2_ERROR
#ifndef LFS2_NO_ERROR
#define LFS2_ERROR_(fmt, ...) \
    printf("%s:%d:error: " fmt "%s\n", __FILE__, __LINE__, __VA_ARGS__)
#define LFS2_ERROR(...) LFS2_ERROR_(__VA_ARGS__, "")
#else
#define LFS2_ERROR(...)
#endif
#endif


#ifndef LFS2_ASSERT
#ifndef LFS2_NO_ASSERT
#define LFS2_ASSERT(test) assert(test)
#else
#define LFS2_ASSERT(test)
#endif
#endif







static inline uint32_t lfs2_max(uint32_t a, uint32_t b) {
    return (a > b) ? a : b;
}

static inline uint32_t lfs2_min(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}


static inline uint32_t lfs2_aligndown(uint32_t a, uint32_t alignment) {
    return a - (a % alignment);
}

static inline uint32_t lfs2_alignup(uint32_t a, uint32_t alignment) {
    return lfs2_aligndown(a + alignment-1, alignment);
}


static inline uint32_t lfs2_npw2(uint32_t a) {
#if !defined(LFS2_NO_INTRINSICS) && (defined(__GNUC__) || defined(__CC_ARM))
    return 32 - __builtin_clz(a-1);
#else
    uint32_t r = 0;
    uint32_t s;
    a -= 1;
    s = (a > 0xffff) << 4; a >>= s; r |= s;
    s = (a > 0xff  ) << 3; a >>= s; r |= s;
    s = (a > 0xf   ) << 2; a >>= s; r |= s;
    s = (a > 0x3   ) << 1; a >>= s; r |= s;
    return (r | (a >> 1)) + 1;
#endif
}



static inline uint32_t lfs2_ctz(uint32_t a) {
#if !defined(LFS2_NO_INTRINSICS) && defined(__GNUC__)
    return __builtin_ctz(a);
#else
    return lfs2_npw2((a & -a) + 1) - 1;
#endif
}


static inline uint32_t lfs2_popc(uint32_t a) {
#if !defined(LFS2_NO_INTRINSICS) && (defined(__GNUC__) || defined(__CC_ARM))
    return __builtin_popcount(a);
#else
    a = a - ((a >> 1) & 0x55555555);
    a = (a & 0x33333333) + ((a >> 2) & 0x33333333);
    return (((a + (a >> 4)) & 0xf0f0f0f) * 0x1010101) >> 24;
#endif
}



static inline int lfs2_scmp(uint32_t a, uint32_t b) {
    return (int)(unsigned)(a - b);
}


static inline uint32_t lfs2_fromle32(uint32_t a) {
#if (defined(  BYTE_ORDER  ) && defined(  ORDER_LITTLE_ENDIAN  ) &&   BYTE_ORDER   ==   ORDER_LITTLE_ENDIAN  ) || \
    (defined(__BYTE_ORDER  ) && defined(__ORDER_LITTLE_ENDIAN  ) && __BYTE_ORDER   == __ORDER_LITTLE_ENDIAN  ) || \
    (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    return a;
#elif !defined(LFS2_NO_INTRINSICS) && ( \
    (defined(  BYTE_ORDER  ) && defined(  ORDER_BIG_ENDIAN  ) &&   BYTE_ORDER   ==   ORDER_BIG_ENDIAN  ) || \
    (defined(__BYTE_ORDER  ) && defined(__ORDER_BIG_ENDIAN  ) && __BYTE_ORDER   == __ORDER_BIG_ENDIAN  ) || \
    (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
    return __builtin_bswap32(a);
#else
    return (((uint8_t*)&a)[0] <<  0) |
           (((uint8_t*)&a)[1] <<  8) |
           (((uint8_t*)&a)[2] << 16) |
           (((uint8_t*)&a)[3] << 24);
#endif
}

static inline uint32_t lfs2_tole32(uint32_t a) {
    return lfs2_fromle32(a);
}


static inline uint32_t lfs2_frombe32(uint32_t a) {
#if !defined(LFS2_NO_INTRINSICS) && ( \
    (defined(  BYTE_ORDER  ) && defined(  ORDER_LITTLE_ENDIAN  ) &&   BYTE_ORDER   ==   ORDER_LITTLE_ENDIAN  ) || \
    (defined(__BYTE_ORDER  ) && defined(__ORDER_LITTLE_ENDIAN  ) && __BYTE_ORDER   == __ORDER_LITTLE_ENDIAN  ) || \
    (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
    return __builtin_bswap32(a);
#elif (defined(  BYTE_ORDER  ) && defined(  ORDER_BIG_ENDIAN  ) &&   BYTE_ORDER   ==   ORDER_BIG_ENDIAN  ) || \
    (defined(__BYTE_ORDER  ) && defined(__ORDER_BIG_ENDIAN  ) && __BYTE_ORDER   == __ORDER_BIG_ENDIAN  ) || \
    (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return a;
#else
    return (((uint8_t*)&a)[0] << 24) |
           (((uint8_t*)&a)[1] << 16) |
           (((uint8_t*)&a)[2] <<  8) |
           (((uint8_t*)&a)[3] <<  0);
#endif
}

static inline uint32_t lfs2_tobe32(uint32_t a) {
    return lfs2_frombe32(a);
}


uint32_t lfs2_crc(uint32_t crc, const void *buffer, size_t size);



static inline void *lfs2_malloc(size_t size) {
#ifndef LFS2_NO_MALLOC
    return malloc(size);
#else
    (void)size;
    return NULL;
#endif
}


static inline void lfs2_free(void *p) {
#ifndef LFS2_NO_MALLOC
    free(p);
#else
    (void)p;
#endif
}


#ifdef __cplusplus
} 
#endif

#endif
#endif
