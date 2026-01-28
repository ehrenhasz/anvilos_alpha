#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/kmem_cache.h>
#include <sys/zmod.h>
#include <contrib/zlib/zlib.h>
#include <sys/kobj.h>
static void *
zcalloc(void *opaque, uint_t items, uint_t size)
{
	(void) opaque;
	return (malloc((size_t)items*size, M_SOLARIS, M_NOWAIT));
}
static void
zcfree(void *opaque, void *ptr)
{
	(void) opaque;
	free(ptr, M_SOLARIS);
}
static int
zlib_deflateInit(z_stream *stream, int level)
{
	stream->zalloc = zcalloc;
	stream->opaque = NULL;
	stream->zfree = zcfree;
	return (deflateInit(stream, level));
}
static int
zlib_deflate(z_stream *stream, int flush)
{
	return (deflate(stream, flush));
}
static int
zlib_deflateEnd(z_stream *stream)
{
	return (deflateEnd(stream));
}
static int
zlib_inflateInit(z_stream *stream)
{
	stream->zalloc = zcalloc;
	stream->opaque = NULL;
	stream->zfree = zcfree;
	return (inflateInit(stream));
}
static int
zlib_inflate(z_stream *stream, int finish)
{
	return (inflate(stream, finish));
}
static int
zlib_inflateEnd(z_stream *stream)
{
	return (inflateEnd(stream));
}
static void *
zlib_workspace_alloc(int flags)
{
	return (NULL);
}
static void
zlib_workspace_free(void *workspace)
{
}
int
z_compress_level(void *dest, size_t *destLen, const void *source,
    size_t sourceLen, int level)
{
	z_stream stream = {0};
	int err;
	stream.next_in = (Byte *)source;
	stream.avail_in = (uInt)sourceLen;
	stream.next_out = dest;
	stream.avail_out = (uInt)*destLen;
	stream.opaque = NULL;
	if ((size_t)stream.avail_out != *destLen)
		return (Z_BUF_ERROR);
	stream.opaque = zlib_workspace_alloc(KM_SLEEP);
#if 0
	if (!stream.opaque)
		return (Z_MEM_ERROR);
#endif
	err = zlib_deflateInit(&stream, level);
	if (err != Z_OK) {
		zlib_workspace_free(stream.opaque);
		return (err);
	}
	err = zlib_deflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		zlib_deflateEnd(&stream);
		zlib_workspace_free(stream.opaque);
		return (err == Z_OK ? Z_BUF_ERROR : err);
	}
	*destLen = stream.total_out;
	err = zlib_deflateEnd(&stream);
	zlib_workspace_free(stream.opaque);
	return (err);
}
int
z_uncompress(void *dest, size_t *destLen, const void *source, size_t sourceLen)
{
	z_stream stream = {0};
	int err;
	stream.next_in = (Byte *)source;
	stream.avail_in = (uInt)sourceLen;
	stream.next_out = dest;
	stream.avail_out = (uInt)*destLen;
	if ((size_t)stream.avail_out != *destLen)
		return (Z_BUF_ERROR);
	stream.opaque = zlib_workspace_alloc(KM_SLEEP);
#if 0
	if (!stream.opaque)
		return (Z_MEM_ERROR);
#endif
	err = zlib_inflateInit(&stream);
	if (err != Z_OK) {
		zlib_workspace_free(stream.opaque);
		return (err);
	}
	err = zlib_inflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		zlib_inflateEnd(&stream);
		zlib_workspace_free(stream.opaque);
		if (err == Z_NEED_DICT ||
		    (err == Z_BUF_ERROR && stream.avail_in == 0))
			return (Z_DATA_ERROR);
		return (err);
	}
	*destLen = stream.total_out;
	err = zlib_inflateEnd(&stream);
	zlib_workspace_free(stream.opaque);
	return (err);
}
