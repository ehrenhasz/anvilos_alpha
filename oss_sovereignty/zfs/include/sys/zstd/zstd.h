#ifndef	_ZFS_ZSTD_H
#define	_ZFS_ZSTD_H
#ifdef	__cplusplus
extern "C" {
#endif
typedef struct zfs_zstd_header {
	uint32_t c_len;
	uint32_t raw_version_level;
	char data[];
} zfs_zstdhdr_t;
typedef struct zfs_zstd_meta {
	uint8_t level;
	uint32_t version;
} zfs_zstdmeta_t;
#define	ZSTDSTAT(stat)		(zstd_stats.stat.value.ui64)
#define	ZSTDSTAT_ZERO(stat)	\
	atomic_store_64(&zstd_stats.stat.value.ui64, 0)
#define	ZSTDSTAT_ADD(stat, val) \
	atomic_add_64(&zstd_stats.stat.value.ui64, (val))
#define	ZSTDSTAT_SUB(stat, val) \
	atomic_sub_64(&zstd_stats.stat.value.ui64, (val))
#define	ZSTDSTAT_BUMP(stat)	ZSTDSTAT_ADD(stat, 1)
int zstd_init(void);
void zstd_fini(void);
size_t zfs_zstd_compress(void *s_start, void *d_start, size_t s_len,
    size_t d_len, int level);
size_t zfs_zstd_compress_wrap(void *s_start, void *d_start, size_t s_len,
    size_t d_len, int level);
int zfs_zstd_get_level(void *s_start, size_t s_len, uint8_t *level);
int zfs_zstd_decompress_level(void *s_start, void *d_start, size_t s_len,
    size_t d_len, uint8_t *level);
int zfs_zstd_decompress(void *s_start, void *d_start, size_t s_len,
    size_t d_len, int n);
void zfs_zstd_cache_reap_now(void);
static inline void
zfs_get_hdrmeta(const zfs_zstdhdr_t *blob, zfs_zstdmeta_t *res)
{
	uint32_t raw = blob->raw_version_level;
	uint8_t findme = 0xff;
	int shift;
	for (shift = 0; shift < 4; shift++) {
		findme = BF32_GET(raw, 8*shift, 8);
		if (findme == 0)
			break;
	}
	switch (shift) {
	case 0:
		res->level = BF32_GET(raw, 24, 8);
		res->version = BSWAP_32(raw);
		res->version = BF32_GET(res->version, 8, 24);
		break;
	case 1:
		res->level = BF32_GET(raw, 0, 8);
		res->version = BSWAP_32(raw);
		res->version = BF32_GET(res->version, 0, 24);
		break;
	case 2:
		res->level = BF32_GET(raw, 24, 8);
		res->version = BF32_GET(raw, 0, 24);
		break;
	case 3:
		res->level = BF32_GET(raw, 0, 8);
		res->version = BF32_GET(raw, 8, 24);
		break;
	default:
		res->level = 0;
		res->version = 0;
		break;
	}
}
static inline uint8_t
zfs_get_hdrlevel(const zfs_zstdhdr_t *blob)
{
	uint8_t level = 0;
	zfs_zstdmeta_t res;
	zfs_get_hdrmeta(blob, &res);
	level = res.level;
	return (level);
}
static inline uint32_t
zfs_get_hdrversion(const zfs_zstdhdr_t *blob)
{
	uint32_t version = 0;
	zfs_zstdmeta_t res;
	zfs_get_hdrmeta(blob, &res);
	version = res.version;
	return (version);
}
static inline void
zfs_set_hdrversion(zfs_zstdhdr_t *blob, uint32_t version)
{
	BF32_SET(blob->raw_version_level, 0, 24, version);
}
static inline void
zfs_set_hdrlevel(zfs_zstdhdr_t *blob, uint8_t level)
{
	BF32_SET(blob->raw_version_level, 24, 8, level);
}
#ifdef	__cplusplus
}
#endif
#endif  
