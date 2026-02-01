 
 
 

#ifndef	_ZFS_FLETCHER_H
#define	_ZFS_FLETCHER_H extern __attribute__((visibility("default")))

#include <sys/types.h>
#include <sys/spa_checksum.h>

#ifdef	__cplusplus
extern "C" {
#endif

 
_ZFS_FLETCHER_H void fletcher_init(zio_cksum_t *);
_ZFS_FLETCHER_H void fletcher_2_native(const void *, uint64_t, const void *,
    zio_cksum_t *);
_ZFS_FLETCHER_H void fletcher_2_byteswap(const void *, uint64_t, const void *,
    zio_cksum_t *);
_ZFS_FLETCHER_H void fletcher_4_native(const void *, uint64_t, const void *,
    zio_cksum_t *);
_ZFS_FLETCHER_H int fletcher_2_incremental_native(void *, size_t, void *);
_ZFS_FLETCHER_H int fletcher_2_incremental_byteswap(void *, size_t, void *);
_ZFS_FLETCHER_H void fletcher_4_native_varsize(const void *, uint64_t,
    zio_cksum_t *);
_ZFS_FLETCHER_H void fletcher_4_byteswap(const void *, uint64_t, const void *,
    zio_cksum_t *);
_ZFS_FLETCHER_H int fletcher_4_incremental_native(void *, size_t, void *);
_ZFS_FLETCHER_H int fletcher_4_incremental_byteswap(void *, size_t, void *);
_ZFS_FLETCHER_H int fletcher_4_impl_set(const char *selector);
_ZFS_FLETCHER_H void fletcher_4_init(void);
_ZFS_FLETCHER_H void fletcher_4_fini(void);



 

typedef struct zfs_fletcher_superscalar {
	uint64_t v[4];
} zfs_fletcher_superscalar_t;

typedef struct zfs_fletcher_sse {
	uint64_t v[2];
} zfs_fletcher_sse_t;

typedef struct zfs_fletcher_avx {
	uint64_t v[4];
} zfs_fletcher_avx_t;

typedef struct zfs_fletcher_avx512 {
	uint64_t v[8];
} zfs_fletcher_avx512_t;

typedef struct zfs_fletcher_aarch64_neon {
	uint64_t v[2];
} zfs_fletcher_aarch64_neon_t;


typedef union fletcher_4_ctx {
	zio_cksum_t scalar;
	zfs_fletcher_superscalar_t superscalar[4];

#if defined(HAVE_SSE2) || (defined(HAVE_SSE2) && defined(HAVE_SSSE3))
	zfs_fletcher_sse_t sse[4];
#endif
#if defined(HAVE_AVX) && defined(HAVE_AVX2)
	zfs_fletcher_avx_t avx[4];
#endif
#if defined(__x86_64) && defined(HAVE_AVX512F)
	zfs_fletcher_avx512_t avx512[4];
#endif
#if defined(__aarch64__)
	zfs_fletcher_aarch64_neon_t aarch64_neon[4];
#endif
} fletcher_4_ctx_t;

 
typedef void (*fletcher_4_init_f)(fletcher_4_ctx_t *);
typedef void (*fletcher_4_fini_f)(fletcher_4_ctx_t *, zio_cksum_t *);
typedef void (*fletcher_4_compute_f)(fletcher_4_ctx_t *,
    const void *, uint64_t);

typedef struct fletcher_4_func {
	fletcher_4_init_f init_native;
	fletcher_4_fini_f fini_native;
	fletcher_4_compute_f compute_native;
	fletcher_4_init_f init_byteswap;
	fletcher_4_fini_f fini_byteswap;
	fletcher_4_compute_f compute_byteswap;
	boolean_t (*valid)(void);
	boolean_t uses_fpu;
	const char *name;
} __attribute__((aligned(64))) fletcher_4_ops_t;

_ZFS_FLETCHER_H const fletcher_4_ops_t fletcher_4_superscalar_ops;
_ZFS_FLETCHER_H const fletcher_4_ops_t fletcher_4_superscalar4_ops;

#if defined(HAVE_SSE2)
_ZFS_FLETCHER_H const fletcher_4_ops_t fletcher_4_sse2_ops;
#endif

#if defined(HAVE_SSE2) && defined(HAVE_SSSE3)
_ZFS_FLETCHER_H const fletcher_4_ops_t fletcher_4_ssse3_ops;
#endif

#if defined(HAVE_AVX) && defined(HAVE_AVX2)
_ZFS_FLETCHER_H const fletcher_4_ops_t fletcher_4_avx2_ops;
#endif

#if defined(__x86_64) && defined(HAVE_AVX512F)
_ZFS_FLETCHER_H const fletcher_4_ops_t fletcher_4_avx512f_ops;
#endif

#if defined(__x86_64) && defined(HAVE_AVX512BW)
_ZFS_FLETCHER_H const fletcher_4_ops_t fletcher_4_avx512bw_ops;
#endif

#if defined(__aarch64__)
_ZFS_FLETCHER_H const fletcher_4_ops_t fletcher_4_aarch64_neon_ops;
#endif

#ifdef	__cplusplus
}
#endif

#endif	 
