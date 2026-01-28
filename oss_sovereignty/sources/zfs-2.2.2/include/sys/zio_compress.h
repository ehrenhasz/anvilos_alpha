



#ifndef _SYS_ZIO_COMPRESS_H
#define	_SYS_ZIO_COMPRESS_H

#include <sys/abd.h>

#ifdef	__cplusplus
extern "C" {
#endif

enum zio_compress {
	ZIO_COMPRESS_INHERIT = 0,
	ZIO_COMPRESS_ON,
	ZIO_COMPRESS_OFF,
	ZIO_COMPRESS_LZJB,
	ZIO_COMPRESS_EMPTY,
	ZIO_COMPRESS_GZIP_1,
	ZIO_COMPRESS_GZIP_2,
	ZIO_COMPRESS_GZIP_3,
	ZIO_COMPRESS_GZIP_4,
	ZIO_COMPRESS_GZIP_5,
	ZIO_COMPRESS_GZIP_6,
	ZIO_COMPRESS_GZIP_7,
	ZIO_COMPRESS_GZIP_8,
	ZIO_COMPRESS_GZIP_9,
	ZIO_COMPRESS_ZLE,
	ZIO_COMPRESS_LZ4,
	ZIO_COMPRESS_ZSTD,
	ZIO_COMPRESS_FUNCTIONS
};


#define	ZIO_COMPRESS_HASLEVEL(compress)	((compress == ZIO_COMPRESS_ZSTD || \
					(compress >= ZIO_COMPRESS_GZIP_1 && \
					compress <= ZIO_COMPRESS_GZIP_9)))

#define	ZIO_COMPLEVEL_INHERIT	0
#define	ZIO_COMPLEVEL_DEFAULT	255

enum zio_zstd_levels {
	ZIO_ZSTD_LEVEL_INHERIT = 0,
	ZIO_ZSTD_LEVEL_1,
#define	ZIO_ZSTD_LEVEL_MIN	ZIO_ZSTD_LEVEL_1
	ZIO_ZSTD_LEVEL_2,
	ZIO_ZSTD_LEVEL_3,
#define	ZIO_ZSTD_LEVEL_DEFAULT	ZIO_ZSTD_LEVEL_3
	ZIO_ZSTD_LEVEL_4,
	ZIO_ZSTD_LEVEL_5,
	ZIO_ZSTD_LEVEL_6,
	ZIO_ZSTD_LEVEL_7,
	ZIO_ZSTD_LEVEL_8,
	ZIO_ZSTD_LEVEL_9,
	ZIO_ZSTD_LEVEL_10,
	ZIO_ZSTD_LEVEL_11,
	ZIO_ZSTD_LEVEL_12,
	ZIO_ZSTD_LEVEL_13,
	ZIO_ZSTD_LEVEL_14,
	ZIO_ZSTD_LEVEL_15,
	ZIO_ZSTD_LEVEL_16,
	ZIO_ZSTD_LEVEL_17,
	ZIO_ZSTD_LEVEL_18,
	ZIO_ZSTD_LEVEL_19,
#define	ZIO_ZSTD_LEVEL_MAX	ZIO_ZSTD_LEVEL_19
	ZIO_ZSTD_LEVEL_RESERVE = 101, 
	ZIO_ZSTD_LEVEL_FAST, 
	ZIO_ZSTD_LEVEL_FAST_1,
#define	ZIO_ZSTD_LEVEL_FAST_DEFAULT	ZIO_ZSTD_LEVEL_FAST_1
	ZIO_ZSTD_LEVEL_FAST_2,
	ZIO_ZSTD_LEVEL_FAST_3,
	ZIO_ZSTD_LEVEL_FAST_4,
	ZIO_ZSTD_LEVEL_FAST_5,
	ZIO_ZSTD_LEVEL_FAST_6,
	ZIO_ZSTD_LEVEL_FAST_7,
	ZIO_ZSTD_LEVEL_FAST_8,
	ZIO_ZSTD_LEVEL_FAST_9,
	ZIO_ZSTD_LEVEL_FAST_10,
	ZIO_ZSTD_LEVEL_FAST_20,
	ZIO_ZSTD_LEVEL_FAST_30,
	ZIO_ZSTD_LEVEL_FAST_40,
	ZIO_ZSTD_LEVEL_FAST_50,
	ZIO_ZSTD_LEVEL_FAST_60,
	ZIO_ZSTD_LEVEL_FAST_70,
	ZIO_ZSTD_LEVEL_FAST_80,
	ZIO_ZSTD_LEVEL_FAST_90,
	ZIO_ZSTD_LEVEL_FAST_100,
	ZIO_ZSTD_LEVEL_FAST_500,
	ZIO_ZSTD_LEVEL_FAST_1000,
#define	ZIO_ZSTD_LEVEL_FAST_MAX	ZIO_ZSTD_LEVEL_FAST_1000
	ZIO_ZSTD_LEVEL_AUTO = 251, 
	ZIO_ZSTD_LEVEL_LEVELS
};


struct zio_prop;


typedef size_t zio_compress_func_t(void *src, void *dst,
    size_t s_len, size_t d_len, int);

typedef int zio_decompress_func_t(void *src, void *dst,
    size_t s_len, size_t d_len, int);

typedef int zio_decompresslevel_func_t(void *src, void *dst,
    size_t s_len, size_t d_len, uint8_t *level);

typedef int zio_getlevel_func_t(void *src, size_t s_len, uint8_t *level);



typedef int zio_decompress_abd_func_t(abd_t *src, void *dst,
    size_t s_len, size_t d_len, int);

typedef const struct zio_compress_info {
	const char			*ci_name;
	int				ci_level;
	zio_compress_func_t		*ci_compress;
	zio_decompress_func_t		*ci_decompress;
	zio_decompresslevel_func_t	*ci_decompress_level;
} zio_compress_info_t;

extern zio_compress_info_t zio_compress_table[ZIO_COMPRESS_FUNCTIONS];


extern void lz4_init(void);
extern void lz4_fini(void);


extern size_t lzjb_compress(void *src, void *dst, size_t s_len, size_t d_len,
    int level);
extern int lzjb_decompress(void *src, void *dst, size_t s_len, size_t d_len,
    int level);
extern size_t gzip_compress(void *src, void *dst, size_t s_len, size_t d_len,
    int level);
extern int gzip_decompress(void *src, void *dst, size_t s_len, size_t d_len,
    int level);
extern size_t zle_compress(void *src, void *dst, size_t s_len, size_t d_len,
    int level);
extern int zle_decompress(void *src, void *dst, size_t s_len, size_t d_len,
    int level);
extern size_t lz4_compress_zfs(void *src, void *dst, size_t s_len, size_t d_len,
    int level);
extern int lz4_decompress_zfs(void *src, void *dst, size_t s_len, size_t d_len,
    int level);


extern size_t zio_compress_data(enum zio_compress c, abd_t *src, void **dst,
    size_t s_len, uint8_t level);
extern int zio_decompress_data(enum zio_compress c, abd_t *src, void *dst,
    size_t s_len, size_t d_len, uint8_t *level);
extern int zio_decompress_data_buf(enum zio_compress c, void *src, void *dst,
    size_t s_len, size_t d_len, uint8_t *level);
extern int zio_compress_to_feature(enum zio_compress comp);

#ifdef	__cplusplus
}
#endif

#endif	
