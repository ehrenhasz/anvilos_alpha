
#ifndef LFS1_UTIL_H
#define LFS1_UTIL_H







#ifdef LFS1_CONFIG
#define LFS1_STRINGIZE(x) LFS1_STRINGIZE2(x)
#define LFS1_STRINGIZE2(x) #x
#include LFS1_STRINGIZE(LFS1_CONFIG)
#else


#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef LFS1_NO_MALLOC
#include <stdlib.h>
#endif
#ifndef LFS1_NO_ASSERT
#include <assert.h>
#endif
#if !defined(LFS1_NO_DEBUG) || !defined(LFS1_NO_WARN) || !defined(LFS1_NO_ERROR)
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif







#ifndef LFS1_NO_DEBUG
#define LFS1_DEBUG(fmt, ...) \
    printf("lfs1 debug:%d: " fmt "\n", __LINE__, __VA_ARGS__)
#else
#define LFS1_DEBUG(fmt, ...)
#endif

#ifndef LFS1_NO_WARN
#define LFS1_WARN(fmt, ...) \
    printf("lfs1 warn:%d: " fmt "\n", __LINE__, __VA_ARGS__)
#else
#define LFS1_WARN(fmt, ...)
#endif

#ifndef LFS1_NO_ERROR
#define LFS1_ERROR(fmt, ...) \
    printf("lfs1 error:%d: " fmt "\n", __LINE__, __VA_ARGS__)
#else
#define LFS1_ERROR(fmt, ...)
#endif


#ifndef LFS1_NO_ASSERT
#define LFS1_ASSERT(test) assert(test)
#else
#define LFS1_ASSERT(test)
#endif







static inline uint32_t lfs1_max(uint32_t a, uint32_t b) {
    return (a > b) ? a : b;
}

static inline uint32_t lfs1_min(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}


static inline uint32_t lfs1_npw2(uint32_t a) {
#if !defined(LFS1_NO_INTRINSICS) && (defined(__GNUC__) || defined(__CC_ARM))
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



static inline uint32_t lfs1_ctz(uint32_t a) {
#if !defined(LFS1_NO_INTRINSICS) && defined(__GNUC__)
    return __builtin_ctz(a);
#else
    return lfs1_npw2((a & -a) + 1) - 1;
#endif
}


static inline uint32_t lfs1_popc(uint32_t a) {
#if !defined(LFS1_NO_INTRINSICS) && (defined(__GNUC__) || defined(__CC_ARM))
    return __builtin_popcount(a);
#else
    a = a - ((a >> 1) & 0x55555555);
    a = (a & 0x33333333) + ((a >> 2) & 0x33333333);
    return (((a + (a >> 4)) & 0xf0f0f0f) * 0x1010101) >> 24;
#endif
}



static inline int lfs1_scmp(uint32_t a, uint32_t b) {
    return (int)(unsigned)(a - b);
}


static inline uint32_t lfs1_fromle32(uint32_t a) {
#if !defined(LFS1_NO_INTRINSICS) && ( \
    (defined(  BYTE_ORDER  ) &&   BYTE_ORDER   ==   ORDER_LITTLE_ENDIAN  ) || \
    (defined(__BYTE_ORDER  ) && __BYTE_ORDER   == __ORDER_LITTLE_ENDIAN  ) || \
    (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
    return a;
#elif !defined(LFS1_NO_INTRINSICS) && ( \
    (defined(  BYTE_ORDER  ) &&   BYTE_ORDER   ==   ORDER_BIG_ENDIAN  ) || \
    (defined(__BYTE_ORDER  ) && __BYTE_ORDER   == __ORDER_BIG_ENDIAN  ) || \
    (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
    return __builtin_bswap32(a);
#else
    return (((uint8_t*)&a)[0] <<  0) |
           (((uint8_t*)&a)[1] <<  8) |
           (((uint8_t*)&a)[2] << 16) |
           (((uint8_t*)&a)[3] << 24);
#endif
}


static inline uint32_t lfs1_tole32(uint32_t a) {
    return lfs1_fromle32(a);
}


void lfs1_crc(uint32_t *crc, const void *buffer, size_t size);


static inline void *lfs1_malloc(size_t size) {
#ifndef LFS1_NO_MALLOC
    return malloc(size);
#else
    (void)size;
    return NULL;
#endif
}


static inline void lfs1_free(void *p) {
#ifndef LFS1_NO_MALLOC
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
