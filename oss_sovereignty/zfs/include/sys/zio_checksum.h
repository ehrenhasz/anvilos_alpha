#ifndef _SYS_ZIO_CHECKSUM_H
#define	_SYS_ZIO_CHECKSUM_H extern __attribute__((visibility("default")))
#include <sys/zio.h>
#include <zfeature_common.h>
#include <zfs_fletcher.h>
#ifdef	__cplusplus
extern "C" {
#endif
struct abd;
typedef void zio_checksum_t(struct abd *abd, uint64_t size,
    const void *ctx_template, zio_cksum_t *zcp);
typedef void *zio_checksum_tmpl_init_t(const zio_cksum_salt_t *salt);
typedef void zio_checksum_tmpl_free_t(void *ctx_template);
typedef enum zio_checksum_flags {
	ZCHECKSUM_FLAG_METADATA = (1 << 1),
	ZCHECKSUM_FLAG_EMBEDDED = (1 << 2),
	ZCHECKSUM_FLAG_DEDUP = (1 << 3),
	ZCHECKSUM_FLAG_SALTED = (1 << 4),
	ZCHECKSUM_FLAG_NOPWRITE = (1 << 5)
} zio_checksum_flags_t;
typedef enum {
	ZIO_CHECKSUM_NATIVE,
	ZIO_CHECKSUM_BYTESWAP
} zio_byteorder_t;
typedef struct zio_abd_checksum_data {
	zio_byteorder_t		acd_byteorder;
	fletcher_4_ctx_t	*acd_ctx;
	zio_cksum_t 		*acd_zcp;
	void 			*acd_private;
} zio_abd_checksum_data_t;
typedef void zio_abd_checksum_init_t(zio_abd_checksum_data_t *);
typedef void zio_abd_checksum_fini_t(zio_abd_checksum_data_t *);
typedef int zio_abd_checksum_iter_t(void *, size_t, void *);
typedef const struct zio_abd_checksum_func {
	zio_abd_checksum_init_t *acf_init;
	zio_abd_checksum_fini_t *acf_fini;
	zio_abd_checksum_iter_t *acf_iter;
} zio_abd_checksum_func_t;
typedef const struct zio_checksum_info {
	zio_checksum_t			*ci_func[2];
	zio_checksum_tmpl_init_t	*ci_tmpl_init;
	zio_checksum_tmpl_free_t	*ci_tmpl_free;
	zio_checksum_flags_t		ci_flags;
	const char			*ci_name;	 
} zio_checksum_info_t;
typedef struct zio_bad_cksum {
	const char		*zbc_checksum_name;
	uint8_t			zbc_byteswapped;
	uint8_t			zbc_injected;
	uint8_t			zbc_has_cksum;	 
} zio_bad_cksum_t;
_SYS_ZIO_CHECKSUM_H zio_checksum_info_t
    zio_checksum_table[ZIO_CHECKSUM_FUNCTIONS];
extern zio_checksum_t abd_checksum_sha256;
extern zio_checksum_t abd_checksum_sha512_native;
extern zio_checksum_t abd_checksum_sha512_byteswap;
extern zio_checksum_t abd_checksum_skein_native;
extern zio_checksum_t abd_checksum_skein_byteswap;
extern zio_checksum_tmpl_init_t abd_checksum_skein_tmpl_init;
extern zio_checksum_tmpl_free_t abd_checksum_skein_tmpl_free;
extern zio_checksum_t abd_checksum_edonr_native;
extern zio_checksum_t abd_checksum_edonr_byteswap;
extern zio_checksum_tmpl_init_t abd_checksum_edonr_tmpl_init;
extern zio_checksum_tmpl_free_t abd_checksum_edonr_tmpl_free;
extern zio_checksum_t abd_checksum_blake3_native;
extern zio_checksum_t abd_checksum_blake3_byteswap;
extern zio_checksum_tmpl_init_t abd_checksum_blake3_tmpl_init;
extern zio_checksum_tmpl_free_t abd_checksum_blake3_tmpl_free;
_SYS_ZIO_CHECKSUM_H zio_abd_checksum_func_t fletcher_4_abd_ops;
extern zio_checksum_t abd_fletcher_4_native;
extern zio_checksum_t abd_fletcher_4_byteswap;
extern int zio_checksum_equal(spa_t *, blkptr_t *, enum zio_checksum,
    void *, uint64_t, uint64_t, zio_bad_cksum_t *);
extern void zio_checksum_compute(zio_t *, enum zio_checksum,
    struct abd *, uint64_t);
extern int zio_checksum_error_impl(spa_t *, const blkptr_t *, enum zio_checksum,
    struct abd *, uint64_t, uint64_t, zio_bad_cksum_t *);
extern int zio_checksum_error(zio_t *zio, zio_bad_cksum_t *out);
extern enum zio_checksum spa_dedup_checksum(spa_t *spa);
extern void zio_checksum_templates_free(spa_t *spa);
extern spa_feature_t zio_checksum_to_feature(enum zio_checksum cksum);
#ifdef	__cplusplus
}
#endif
#endif	 
