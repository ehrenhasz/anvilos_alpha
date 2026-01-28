#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/zfs_context.h>
#include <sys/zio_compress.h>
#include <sys/spa.h>
#include <sys/zstd/zstd.h>
#define	ZSTD_STATIC_LINKING_ONLY
#include "lib/zstd.h"
#include "lib/common/zstd_errors.h"
static uint_t zstd_earlyabort_pass = 1;
static int zstd_cutoff_level = ZIO_ZSTD_LEVEL_3;
static unsigned int zstd_abort_size = (128 * 1024);
static kstat_t *zstd_ksp = NULL;
typedef struct zstd_stats {
	kstat_named_t	zstd_stat_alloc_fail;
	kstat_named_t	zstd_stat_alloc_fallback;
	kstat_named_t	zstd_stat_com_alloc_fail;
	kstat_named_t	zstd_stat_dec_alloc_fail;
	kstat_named_t	zstd_stat_com_inval;
	kstat_named_t	zstd_stat_dec_inval;
	kstat_named_t	zstd_stat_dec_header_inval;
	kstat_named_t	zstd_stat_com_fail;
	kstat_named_t	zstd_stat_dec_fail;
	kstat_named_t	zstd_stat_lz4pass_allowed;
	kstat_named_t	zstd_stat_lz4pass_rejected;
	kstat_named_t	zstd_stat_zstdpass_allowed;
	kstat_named_t	zstd_stat_zstdpass_rejected;
	kstat_named_t	zstd_stat_passignored;
	kstat_named_t	zstd_stat_passignored_size;
	kstat_named_t	zstd_stat_buffers;
	kstat_named_t	zstd_stat_size;
} zstd_stats_t;
static zstd_stats_t zstd_stats = {
	{ "alloc_fail",			KSTAT_DATA_UINT64 },
	{ "alloc_fallback",		KSTAT_DATA_UINT64 },
	{ "compress_alloc_fail",	KSTAT_DATA_UINT64 },
	{ "decompress_alloc_fail",	KSTAT_DATA_UINT64 },
	{ "compress_level_invalid",	KSTAT_DATA_UINT64 },
	{ "decompress_level_invalid",	KSTAT_DATA_UINT64 },
	{ "decompress_header_invalid",	KSTAT_DATA_UINT64 },
	{ "compress_failed",		KSTAT_DATA_UINT64 },
	{ "decompress_failed",		KSTAT_DATA_UINT64 },
	{ "lz4pass_allowed",		KSTAT_DATA_UINT64 },
	{ "lz4pass_rejected",		KSTAT_DATA_UINT64 },
	{ "zstdpass_allowed",		KSTAT_DATA_UINT64 },
	{ "zstdpass_rejected",		KSTAT_DATA_UINT64 },
	{ "passignored",		KSTAT_DATA_UINT64 },
	{ "passignored_size",		KSTAT_DATA_UINT64 },
	{ "buffers",			KSTAT_DATA_UINT64 },
	{ "size",			KSTAT_DATA_UINT64 },
};
#ifdef _KERNEL
static int
kstat_zstd_update(kstat_t *ksp, int rw)
{
	ASSERT(ksp != NULL);
	if (rw == KSTAT_WRITE && ksp == zstd_ksp) {
		ZSTDSTAT_ZERO(zstd_stat_alloc_fail);
		ZSTDSTAT_ZERO(zstd_stat_alloc_fallback);
		ZSTDSTAT_ZERO(zstd_stat_com_alloc_fail);
		ZSTDSTAT_ZERO(zstd_stat_dec_alloc_fail);
		ZSTDSTAT_ZERO(zstd_stat_com_inval);
		ZSTDSTAT_ZERO(zstd_stat_dec_inval);
		ZSTDSTAT_ZERO(zstd_stat_dec_header_inval);
		ZSTDSTAT_ZERO(zstd_stat_com_fail);
		ZSTDSTAT_ZERO(zstd_stat_dec_fail);
		ZSTDSTAT_ZERO(zstd_stat_lz4pass_allowed);
		ZSTDSTAT_ZERO(zstd_stat_lz4pass_rejected);
		ZSTDSTAT_ZERO(zstd_stat_zstdpass_allowed);
		ZSTDSTAT_ZERO(zstd_stat_zstdpass_rejected);
		ZSTDSTAT_ZERO(zstd_stat_passignored);
		ZSTDSTAT_ZERO(zstd_stat_passignored_size);
	}
	return (0);
}
#endif
enum zstd_kmem_type {
	ZSTD_KMEM_UNKNOWN = 0,
	ZSTD_KMEM_DEFAULT,
	ZSTD_KMEM_POOL,
	ZSTD_KMEM_DCTX,
	ZSTD_KMEM_COUNT,
};
struct zstd_pool {
	void *mem;
	size_t size;
	kmutex_t barrier;
	hrtime_t timeout;
};
struct zstd_kmem {
	enum zstd_kmem_type kmem_type;
	size_t kmem_size;
	struct zstd_pool *pool;
};
struct zstd_fallback_mem {
	size_t mem_size;
	void *mem;
	kmutex_t barrier;
};
struct zstd_levelmap {
	int16_t zstd_level;
	enum zio_zstd_levels level;
};
static void *zstd_alloc(void *opaque, size_t size);
static void *zstd_dctx_alloc(void *opaque, size_t size);
static void zstd_free(void *opaque, void *ptr);
static const ZSTD_customMem zstd_malloc = {
	zstd_alloc,
	zstd_free,
	NULL,
};
static const ZSTD_customMem zstd_dctx_malloc = {
	zstd_dctx_alloc,
	zstd_free,
	NULL,
};
static struct zstd_levelmap zstd_levels[] = {
	{ZIO_ZSTD_LEVEL_1, ZIO_ZSTD_LEVEL_1},
	{ZIO_ZSTD_LEVEL_2, ZIO_ZSTD_LEVEL_2},
	{ZIO_ZSTD_LEVEL_3, ZIO_ZSTD_LEVEL_3},
	{ZIO_ZSTD_LEVEL_4, ZIO_ZSTD_LEVEL_4},
	{ZIO_ZSTD_LEVEL_5, ZIO_ZSTD_LEVEL_5},
	{ZIO_ZSTD_LEVEL_6, ZIO_ZSTD_LEVEL_6},
	{ZIO_ZSTD_LEVEL_7, ZIO_ZSTD_LEVEL_7},
	{ZIO_ZSTD_LEVEL_8, ZIO_ZSTD_LEVEL_8},
	{ZIO_ZSTD_LEVEL_9, ZIO_ZSTD_LEVEL_9},
	{ZIO_ZSTD_LEVEL_10, ZIO_ZSTD_LEVEL_10},
	{ZIO_ZSTD_LEVEL_11, ZIO_ZSTD_LEVEL_11},
	{ZIO_ZSTD_LEVEL_12, ZIO_ZSTD_LEVEL_12},
	{ZIO_ZSTD_LEVEL_13, ZIO_ZSTD_LEVEL_13},
	{ZIO_ZSTD_LEVEL_14, ZIO_ZSTD_LEVEL_14},
	{ZIO_ZSTD_LEVEL_15, ZIO_ZSTD_LEVEL_15},
	{ZIO_ZSTD_LEVEL_16, ZIO_ZSTD_LEVEL_16},
	{ZIO_ZSTD_LEVEL_17, ZIO_ZSTD_LEVEL_17},
	{ZIO_ZSTD_LEVEL_18, ZIO_ZSTD_LEVEL_18},
	{ZIO_ZSTD_LEVEL_19, ZIO_ZSTD_LEVEL_19},
	{-1, ZIO_ZSTD_LEVEL_FAST_1},
	{-2, ZIO_ZSTD_LEVEL_FAST_2},
	{-3, ZIO_ZSTD_LEVEL_FAST_3},
	{-4, ZIO_ZSTD_LEVEL_FAST_4},
	{-5, ZIO_ZSTD_LEVEL_FAST_5},
	{-6, ZIO_ZSTD_LEVEL_FAST_6},
	{-7, ZIO_ZSTD_LEVEL_FAST_7},
	{-8, ZIO_ZSTD_LEVEL_FAST_8},
	{-9, ZIO_ZSTD_LEVEL_FAST_9},
	{-10, ZIO_ZSTD_LEVEL_FAST_10},
	{-20, ZIO_ZSTD_LEVEL_FAST_20},
	{-30, ZIO_ZSTD_LEVEL_FAST_30},
	{-40, ZIO_ZSTD_LEVEL_FAST_40},
	{-50, ZIO_ZSTD_LEVEL_FAST_50},
	{-60, ZIO_ZSTD_LEVEL_FAST_60},
	{-70, ZIO_ZSTD_LEVEL_FAST_70},
	{-80, ZIO_ZSTD_LEVEL_FAST_80},
	{-90, ZIO_ZSTD_LEVEL_FAST_90},
	{-100, ZIO_ZSTD_LEVEL_FAST_100},
	{-500, ZIO_ZSTD_LEVEL_FAST_500},
	{-1000, ZIO_ZSTD_LEVEL_FAST_1000},
};
static int pool_count = 16;
#define	ZSTD_POOL_MAX		pool_count
#define	ZSTD_POOL_TIMEOUT	60 * 2
static struct zstd_fallback_mem zstd_dctx_fallback;
static struct zstd_pool *zstd_mempool_cctx;
static struct zstd_pool *zstd_mempool_dctx;
#if defined(ZFS_ASAN_ENABLED)
#define	ADDRESS_SANITIZER 1
#endif
#if defined(_KERNEL) && defined(ADDRESS_SANITIZER)
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
void __asan_poison_memory_region(void const volatile *addr, size_t size);
void __asan_unpoison_memory_region(void const volatile *addr, size_t size) {};
void __asan_poison_memory_region(void const volatile *addr, size_t size) {};
#endif
static void
zstd_mempool_reap(struct zstd_pool *zstd_mempool)
{
	struct zstd_pool *pool;
	if (!zstd_mempool || !ZSTDSTAT(zstd_stat_buffers)) {
		return;
	}
	for (int i = 0; i < ZSTD_POOL_MAX; i++) {
		pool = &zstd_mempool[i];
		if (pool->mem && mutex_tryenter(&pool->barrier)) {
			if (pool->mem && gethrestime_sec() > pool->timeout) {
				vmem_free(pool->mem, pool->size);
				ZSTDSTAT_SUB(zstd_stat_buffers, 1);
				ZSTDSTAT_SUB(zstd_stat_size, pool->size);
				pool->mem = NULL;
				pool->size = 0;
				pool->timeout = 0;
			}
			mutex_exit(&pool->barrier);
		}
	}
}
static void *
zstd_mempool_alloc(struct zstd_pool *zstd_mempool, size_t size)
{
	struct zstd_pool *pool;
	struct zstd_kmem *mem = NULL;
	if (!zstd_mempool) {
		return (NULL);
	}
	for (int i = 0; i < ZSTD_POOL_MAX; i++) {
		pool = &zstd_mempool[i];
		if (mutex_tryenter(&pool->barrier)) {
			if (pool->mem && size <= pool->size) {
				pool->timeout = gethrestime_sec() +
				    ZSTD_POOL_TIMEOUT;
				mem = pool->mem;
				return (mem);
			}
			mutex_exit(&pool->barrier);
		}
	}
	for (int i = 0; i < ZSTD_POOL_MAX; i++) {
		pool = &zstd_mempool[i];
		if (mutex_tryenter(&pool->barrier)) {
			if (!pool->mem) {
				mem = vmem_alloc(size, KM_SLEEP);
				if (mem) {
					ZSTDSTAT_ADD(zstd_stat_buffers, 1);
					ZSTDSTAT_ADD(zstd_stat_size, size);
					pool->mem = mem;
					pool->size = size;
					mem->pool = pool;
					mem->kmem_type = ZSTD_KMEM_POOL;
					mem->kmem_size = size;
				}
			}
			if (size <= pool->size) {
				pool->timeout = gethrestime_sec() +
				    ZSTD_POOL_TIMEOUT;
				return (pool->mem);
			}
			mutex_exit(&pool->barrier);
		}
	}
	if (!mem) {
		mem = vmem_alloc(size, KM_NOSLEEP);
		if (mem) {
			mem->pool = NULL;
			mem->kmem_type = ZSTD_KMEM_DEFAULT;
			mem->kmem_size = size;
		}
	}
	return (mem);
}
static void
zstd_mempool_free(struct zstd_kmem *z)
{
	mutex_exit(&z->pool->barrier);
}
static int
zstd_enum_to_level(enum zio_zstd_levels level, int16_t *zstd_level)
{
	if (level > 0 && level <= ZIO_ZSTD_LEVEL_19) {
		*zstd_level = zstd_levels[level - 1].zstd_level;
		return (0);
	}
	if (level >= ZIO_ZSTD_LEVEL_FAST_1 &&
	    level <= ZIO_ZSTD_LEVEL_FAST_1000) {
		*zstd_level = zstd_levels[level - ZIO_ZSTD_LEVEL_FAST_1
		    + ZIO_ZSTD_LEVEL_19].zstd_level;
		return (0);
	}
	return (1);
}
size_t
zfs_zstd_compress_wrap(void *s_start, void *d_start, size_t s_len, size_t d_len,
    int level)
{
	int16_t zstd_level;
	if (zstd_enum_to_level(level, &zstd_level)) {
		ZSTDSTAT_BUMP(zstd_stat_com_inval);
		return (s_len);
	}
	size_t actual_abort_size = zstd_abort_size;
	if (zstd_earlyabort_pass > 0 && zstd_level >= zstd_cutoff_level &&
	    s_len >= actual_abort_size) {
		int pass_len = 1;
		pass_len = lz4_compress_zfs(s_start, d_start, s_len, d_len, 0);
		if (pass_len < d_len) {
			ZSTDSTAT_BUMP(zstd_stat_lz4pass_allowed);
			goto keep_trying;
		}
		ZSTDSTAT_BUMP(zstd_stat_lz4pass_rejected);
		pass_len = zfs_zstd_compress(s_start, d_start, s_len, d_len,
		    ZIO_ZSTD_LEVEL_1);
		if (pass_len == s_len || pass_len <= 0 || pass_len > d_len) {
			ZSTDSTAT_BUMP(zstd_stat_zstdpass_rejected);
			return (s_len);
		}
		ZSTDSTAT_BUMP(zstd_stat_zstdpass_allowed);
	} else {
		ZSTDSTAT_BUMP(zstd_stat_passignored);
		if (s_len < actual_abort_size) {
			ZSTDSTAT_BUMP(zstd_stat_passignored_size);
		}
	}
keep_trying:
	return (zfs_zstd_compress(s_start, d_start, s_len, d_len, level));
}
size_t
zfs_zstd_compress(void *s_start, void *d_start, size_t s_len, size_t d_len,
    int level)
{
	size_t c_len;
	int16_t zstd_level;
	zfs_zstdhdr_t *hdr;
	ZSTD_CCtx *cctx;
	hdr = (zfs_zstdhdr_t *)d_start;
	if (zstd_enum_to_level(level, &zstd_level)) {
		ZSTDSTAT_BUMP(zstd_stat_com_inval);
		return (s_len);
	}
	ASSERT3U(d_len, >=, sizeof (*hdr));
	ASSERT3U(d_len, <=, s_len);
	ASSERT3U(zstd_level, !=, 0);
	cctx = ZSTD_createCCtx_advanced(zstd_malloc);
	if (!cctx) {
		ZSTDSTAT_BUMP(zstd_stat_com_alloc_fail);
		return (s_len);
	}
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, zstd_level);
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_format, ZSTD_f_zstd1_magicless);
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 0);
	ZSTD_CCtx_setParameter(cctx, ZSTD_c_contentSizeFlag, 0);
	c_len = ZSTD_compress2(cctx,
	    hdr->data,
	    d_len - sizeof (*hdr),
	    s_start, s_len);
	ZSTD_freeCCtx(cctx);
	if (ZSTD_isError(c_len)) {
		int err = ZSTD_getErrorCode(c_len);
		if (err != ZSTD_error_dstSize_tooSmall) {
			ZSTDSTAT_BUMP(zstd_stat_com_fail);
			dprintf("Error: %s", ZSTD_getErrorString(err));
		}
		return (s_len);
	}
	hdr->c_len = BE_32(c_len);
	ASSERT3U(ZSTD_VERSION_NUMBER, <=, 0xFFFFFF);
	zfs_set_hdrversion(hdr, ZSTD_VERSION_NUMBER);
	zfs_set_hdrlevel(hdr, level);
	hdr->raw_version_level = BE_32(hdr->raw_version_level);
	return (c_len + sizeof (*hdr));
}
int
zfs_zstd_decompress_level(void *s_start, void *d_start, size_t s_len,
    size_t d_len, uint8_t *level)
{
	ZSTD_DCtx *dctx;
	size_t result;
	int16_t zstd_level;
	uint32_t c_len;
	const zfs_zstdhdr_t *hdr;
	zfs_zstdhdr_t hdr_copy;
	hdr = (const zfs_zstdhdr_t *)s_start;
	c_len = BE_32(hdr->c_len);
	hdr_copy.raw_version_level = BE_32(hdr->raw_version_level);
	uint8_t curlevel = zfs_get_hdrlevel(&hdr_copy);
	if (zstd_enum_to_level(curlevel, &zstd_level)) {
		ZSTDSTAT_BUMP(zstd_stat_dec_inval);
		return (1);
	}
	ASSERT3U(d_len, >=, s_len);
	ASSERT3U(curlevel, !=, ZIO_COMPLEVEL_INHERIT);
	if (c_len + sizeof (*hdr) > s_len) {
		ZSTDSTAT_BUMP(zstd_stat_dec_header_inval);
		return (1);
	}
	dctx = ZSTD_createDCtx_advanced(zstd_dctx_malloc);
	if (!dctx) {
		ZSTDSTAT_BUMP(zstd_stat_dec_alloc_fail);
		return (1);
	}
	ZSTD_DCtx_setParameter(dctx, ZSTD_d_format, ZSTD_f_zstd1_magicless);
	result = ZSTD_decompressDCtx(dctx, d_start, d_len, hdr->data, c_len);
	ZSTD_freeDCtx(dctx);
	if (ZSTD_isError(result)) {
		ZSTDSTAT_BUMP(zstd_stat_dec_fail);
		return (1);
	}
	if (level) {
		*level = curlevel;
	}
	return (0);
}
int
zfs_zstd_decompress(void *s_start, void *d_start, size_t s_len, size_t d_len,
    int level __maybe_unused)
{
	return (zfs_zstd_decompress_level(s_start, d_start, s_len, d_len,
	    NULL));
}
static void *
zstd_alloc(void *opaque __maybe_unused, size_t size)
{
	size_t nbytes = sizeof (struct zstd_kmem) + size;
	struct zstd_kmem *z = NULL;
	z = (struct zstd_kmem *)zstd_mempool_alloc(zstd_mempool_cctx, nbytes);
	if (!z) {
		ZSTDSTAT_BUMP(zstd_stat_alloc_fail);
		return (NULL);
	}
	return ((void*)z + (sizeof (struct zstd_kmem)));
}
static void *
zstd_dctx_alloc(void *opaque __maybe_unused, size_t size)
{
	size_t nbytes = sizeof (struct zstd_kmem) + size;
	struct zstd_kmem *z = NULL;
	enum zstd_kmem_type type = ZSTD_KMEM_DEFAULT;
	z = (struct zstd_kmem *)zstd_mempool_alloc(zstd_mempool_dctx, nbytes);
	if (!z) {
		z = vmem_alloc(nbytes, KM_SLEEP);
		if (z) {
			z->pool = NULL;
		}
		ZSTDSTAT_BUMP(zstd_stat_alloc_fail);
	} else {
		return ((void*)z + (sizeof (struct zstd_kmem)));
	}
	if (!z) {
		mutex_enter(&zstd_dctx_fallback.barrier);
		z = zstd_dctx_fallback.mem;
		type = ZSTD_KMEM_DCTX;
		ZSTDSTAT_BUMP(zstd_stat_alloc_fallback);
	}
	if (!z) {
		return (NULL);
	}
	z->kmem_type = type;
	z->kmem_size = nbytes;
	return ((void*)z + (sizeof (struct zstd_kmem)));
}
static void
zstd_free(void *opaque __maybe_unused, void *ptr)
{
	struct zstd_kmem *z = (ptr - sizeof (struct zstd_kmem));
	enum zstd_kmem_type type;
	ASSERT3U(z->kmem_type, <, ZSTD_KMEM_COUNT);
	ASSERT3U(z->kmem_type, >, ZSTD_KMEM_UNKNOWN);
	type = z->kmem_type;
	switch (type) {
	case ZSTD_KMEM_DEFAULT:
		vmem_free(z, z->kmem_size);
		break;
	case ZSTD_KMEM_POOL:
		zstd_mempool_free(z);
		break;
	case ZSTD_KMEM_DCTX:
		mutex_exit(&zstd_dctx_fallback.barrier);
		break;
	default:
		break;
	}
}
static void __init
create_fallback_mem(struct zstd_fallback_mem *mem, size_t size)
{
	mem->mem_size = size;
	mem->mem = vmem_zalloc(mem->mem_size, KM_SLEEP);
	mutex_init(&mem->barrier, NULL, MUTEX_DEFAULT, NULL);
}
static void __init
zstd_mempool_init(void)
{
	zstd_mempool_cctx =
	    kmem_zalloc(ZSTD_POOL_MAX * sizeof (struct zstd_pool), KM_SLEEP);
	zstd_mempool_dctx =
	    kmem_zalloc(ZSTD_POOL_MAX * sizeof (struct zstd_pool), KM_SLEEP);
	for (int i = 0; i < ZSTD_POOL_MAX; i++) {
		mutex_init(&zstd_mempool_cctx[i].barrier, NULL,
		    MUTEX_DEFAULT, NULL);
		mutex_init(&zstd_mempool_dctx[i].barrier, NULL,
		    MUTEX_DEFAULT, NULL);
	}
}
static int __init
zstd_meminit(void)
{
	zstd_mempool_init();
	create_fallback_mem(&zstd_dctx_fallback,
	    P2ROUNDUP(ZSTD_estimateDCtxSize() + sizeof (struct zstd_kmem),
	    PAGESIZE));
	return (0);
}
static void
release_pool(struct zstd_pool *pool)
{
	mutex_destroy(&pool->barrier);
	vmem_free(pool->mem, pool->size);
	pool->mem = NULL;
	pool->size = 0;
}
static void
zstd_mempool_deinit(void)
{
	for (int i = 0; i < ZSTD_POOL_MAX; i++) {
		release_pool(&zstd_mempool_cctx[i]);
		release_pool(&zstd_mempool_dctx[i]);
	}
	kmem_free(zstd_mempool_dctx, ZSTD_POOL_MAX * sizeof (struct zstd_pool));
	kmem_free(zstd_mempool_cctx, ZSTD_POOL_MAX * sizeof (struct zstd_pool));
	zstd_mempool_dctx = NULL;
	zstd_mempool_cctx = NULL;
}
void
zfs_zstd_cache_reap_now(void)
{
	zstd_mempool_reap(zstd_mempool_cctx);
	zstd_mempool_reap(zstd_mempool_dctx);
}
extern int __init
zstd_init(void)
{
	pool_count = (boot_ncpus * 4);
	zstd_meminit();
	zstd_ksp = kstat_create("zfs", 0, "zstd", "misc",
	    KSTAT_TYPE_NAMED, sizeof (zstd_stats) / sizeof (kstat_named_t),
	    KSTAT_FLAG_VIRTUAL);
	if (zstd_ksp != NULL) {
		zstd_ksp->ks_data = &zstd_stats;
		kstat_install(zstd_ksp);
#ifdef _KERNEL
		zstd_ksp->ks_update = kstat_zstd_update;
#endif
	}
	return (0);
}
extern void
zstd_fini(void)
{
	if (zstd_ksp != NULL) {
		kstat_delete(zstd_ksp);
		zstd_ksp = NULL;
	}
	vmem_free(zstd_dctx_fallback.mem, zstd_dctx_fallback.mem_size);
	mutex_destroy(&zstd_dctx_fallback.barrier);
	zstd_mempool_deinit();
}
#if defined(_KERNEL)
#ifdef __FreeBSD__
module_init(zstd_init);
module_exit(zstd_fini);
#endif
ZFS_MODULE_PARAM(zfs, zstd_, earlyabort_pass, UINT, ZMOD_RW,
	"Enable early abort attempts when using zstd");
ZFS_MODULE_PARAM(zfs, zstd_, abort_size, UINT, ZMOD_RW,
	"Minimal size of block to attempt early abort");
#endif
