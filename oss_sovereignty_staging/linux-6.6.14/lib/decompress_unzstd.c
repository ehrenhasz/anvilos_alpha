

 

 
#ifdef STATIC
# define UNZSTD_PREBOOT
# include "xxhash.c"
# include "zstd/decompress_sources.h"
#else
#include <linux/decompress/unzstd.h>
#endif

#include <linux/decompress/mm.h>
#include <linux/kernel.h>
#include <linux/zstd.h>

 
#define ZSTD_WINDOWSIZE_MAX	(1 << ZSTD_WINDOWLOG_MAX)
 
#define ZSTD_IOBUF_SIZE		(1 << 17)

static int INIT handle_zstd_error(size_t ret, void (*error)(char *x))
{
	const zstd_error_code err = zstd_get_error_code(ret);

	if (!zstd_is_error(ret))
		return 0;

	 
	switch (err) {
	case ZSTD_error_memory_allocation:
		error("ZSTD decompressor ran out of memory");
		break;
	case ZSTD_error_prefix_unknown:
		error("Input is not in the ZSTD format (wrong magic bytes)");
		break;
	case ZSTD_error_dstSize_tooSmall:
	case ZSTD_error_corruption_detected:
	case ZSTD_error_checksum_wrong:
		error("ZSTD-compressed data is corrupt");
		break;
	default:
		error("ZSTD-compressed data is probably corrupt");
		break;
	}
	return -1;
}

 
static int INIT decompress_single(const u8 *in_buf, long in_len, u8 *out_buf,
				  long out_len, long *in_pos,
				  void (*error)(char *x))
{
	const size_t wksp_size = zstd_dctx_workspace_bound();
	void *wksp = large_malloc(wksp_size);
	zstd_dctx *dctx = zstd_init_dctx(wksp, wksp_size);
	int err;
	size_t ret;

	if (dctx == NULL) {
		error("Out of memory while allocating zstd_dctx");
		err = -1;
		goto out;
	}
	 
	ret = zstd_find_frame_compressed_size(in_buf, in_len);
	err = handle_zstd_error(ret, error);
	if (err)
		goto out;
	in_len = (long)ret;

	ret = zstd_decompress_dctx(dctx, out_buf, out_len, in_buf, in_len);
	err = handle_zstd_error(ret, error);
	if (err)
		goto out;

	if (in_pos != NULL)
		*in_pos = in_len;

	err = 0;
out:
	if (wksp != NULL)
		large_free(wksp);
	return err;
}

static int INIT __unzstd(unsigned char *in_buf, long in_len,
			 long (*fill)(void*, unsigned long),
			 long (*flush)(void*, unsigned long),
			 unsigned char *out_buf, long out_len,
			 long *in_pos,
			 void (*error)(char *x))
{
	zstd_in_buffer in;
	zstd_out_buffer out;
	zstd_frame_header header;
	void *in_allocated = NULL;
	void *out_allocated = NULL;
	void *wksp = NULL;
	size_t wksp_size;
	zstd_dstream *dstream;
	int err;
	size_t ret;

	 
	if (out_len == 0)
		out_len = UINTPTR_MAX - (uintptr_t)out_buf;

	if (fill == NULL && flush == NULL)
		 
		return decompress_single(in_buf, in_len, out_buf, out_len,
					 in_pos, error);

	 
	if (in_buf == NULL) {
		in_allocated = large_malloc(ZSTD_IOBUF_SIZE);
		if (in_allocated == NULL) {
			error("Out of memory while allocating input buffer");
			err = -1;
			goto out;
		}
		in_buf = in_allocated;
		in_len = 0;
	}
	 
	if (fill != NULL)
		in_len = fill(in_buf, ZSTD_IOBUF_SIZE);
	if (in_len < 0) {
		error("ZSTD-compressed data is truncated");
		err = -1;
		goto out;
	}
	 
	in.src = in_buf;
	in.pos = 0;
	in.size = in_len;
	 
	if (flush != NULL) {
		out_allocated = large_malloc(ZSTD_IOBUF_SIZE);
		if (out_allocated == NULL) {
			error("Out of memory while allocating output buffer");
			err = -1;
			goto out;
		}
		out_buf = out_allocated;
		out_len = ZSTD_IOBUF_SIZE;
	}
	 
	out.dst = out_buf;
	out.pos = 0;
	out.size = out_len;

	 
	ret = zstd_get_frame_header(&header, in.src, in.size);
	err = handle_zstd_error(ret, error);
	if (err)
		goto out;
	if (ret != 0) {
		error("ZSTD-compressed data has an incomplete frame header");
		err = -1;
		goto out;
	}
	if (header.windowSize > ZSTD_WINDOWSIZE_MAX) {
		error("ZSTD-compressed data has too large a window size");
		err = -1;
		goto out;
	}

	 
	wksp_size = zstd_dstream_workspace_bound(header.windowSize);
	wksp = large_malloc(wksp_size);
	dstream = zstd_init_dstream(header.windowSize, wksp, wksp_size);
	if (dstream == NULL) {
		error("Out of memory while allocating ZSTD_DStream");
		err = -1;
		goto out;
	}

	 
	if (in_pos != NULL)
		*in_pos = 0;
	do {
		 
		if (in.pos == in.size) {
			if (in_pos != NULL)
				*in_pos += in.pos;
			in_len = fill ? fill(in_buf, ZSTD_IOBUF_SIZE) : -1;
			if (in_len < 0) {
				error("ZSTD-compressed data is truncated");
				err = -1;
				goto out;
			}
			in.pos = 0;
			in.size = in_len;
		}
		 
		ret = zstd_decompress_stream(dstream, &out, &in);
		err = handle_zstd_error(ret, error);
		if (err)
			goto out;
		 
		if (flush != NULL && out.pos > 0) {
			if (out.pos != flush(out.dst, out.pos)) {
				error("Failed to flush()");
				err = -1;
				goto out;
			}
			out.pos = 0;
		}
	} while (ret != 0);

	if (in_pos != NULL)
		*in_pos += in.pos;

	err = 0;
out:
	if (in_allocated != NULL)
		large_free(in_allocated);
	if (out_allocated != NULL)
		large_free(out_allocated);
	if (wksp != NULL)
		large_free(wksp);
	return err;
}

#ifndef UNZSTD_PREBOOT
STATIC int INIT unzstd(unsigned char *buf, long len,
		       long (*fill)(void*, unsigned long),
		       long (*flush)(void*, unsigned long),
		       unsigned char *out_buf,
		       long *pos,
		       void (*error)(char *x))
{
	return __unzstd(buf, len, fill, flush, out_buf, 0, pos, error);
}
#else
STATIC int INIT __decompress(unsigned char *buf, long len,
			     long (*fill)(void*, unsigned long),
			     long (*flush)(void*, unsigned long),
			     unsigned char *out_buf, long out_len,
			     long *pos,
			     void (*error)(char *x))
{
	return __unzstd(buf, len, fill, flush, out_buf, out_len, pos, error);
}
#endif
