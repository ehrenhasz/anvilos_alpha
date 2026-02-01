 

 



#include <sys/debug.h>
#include <sys/types.h>
#include <sys/qat.h>
#include <sys/zio_compress.h>

#ifdef _KERNEL

#include <sys/zmod.h>
typedef size_t zlen_t;
#define	compress_func	z_compress_level
#define	uncompress_func	z_uncompress

#else  

#include <zlib.h>
typedef uLongf zlen_t;
#define	compress_func	compress2
#define	uncompress_func	uncompress

#endif

size_t
gzip_compress(void *s_start, void *d_start, size_t s_len, size_t d_len, int n)
{
	int ret;
	zlen_t dstlen = d_len;

	ASSERT(d_len <= s_len);

	 
	if (qat_dc_use_accel(s_len)) {
		ret = qat_compress(QAT_COMPRESS, s_start, s_len, d_start,
		    d_len, &dstlen);
		if (ret == CPA_STATUS_SUCCESS) {
			return ((size_t)dstlen);
		} else if (ret == CPA_STATUS_INCOMPRESSIBLE) {
			if (d_len != s_len)
				return (s_len);

			memcpy(d_start, s_start, s_len);
			return (s_len);
		}
		 
	}

	if (compress_func(d_start, &dstlen, s_start, s_len, n) != Z_OK) {
		if (d_len != s_len)
			return (s_len);

		memcpy(d_start, s_start, s_len);
		return (s_len);
	}

	return ((size_t)dstlen);
}

int
gzip_decompress(void *s_start, void *d_start, size_t s_len, size_t d_len, int n)
{
	(void) n;
	zlen_t dstlen = d_len;

	ASSERT(d_len >= s_len);

	 
	if (qat_dc_use_accel(d_len)) {
		if (qat_compress(QAT_DECOMPRESS, s_start, s_len,
		    d_start, d_len, &dstlen) == CPA_STATUS_SUCCESS)
			return (0);
		 
	}

	if (uncompress_func(d_start, &dstlen, s_start, s_len) != Z_OK)
		return (-1);

	return (0);
}
