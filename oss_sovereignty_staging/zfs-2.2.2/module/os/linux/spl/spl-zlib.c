 


#include <linux/percpu_compat.h>
#include <sys/kmem.h>
#include <sys/kmem_cache.h>
#include <sys/zmod.h>

static spl_kmem_cache_t *zlib_workspace_cache;

 
static void *
zlib_workspace_alloc(int flags)
{
	return (kmem_cache_alloc(zlib_workspace_cache, flags & ~(__GFP_FS)));
}

static void
zlib_workspace_free(void *workspace)
{
	kmem_cache_free(zlib_workspace_cache, workspace);
}

 
int
z_compress_level(void *dest, size_t *destLen, const void *source,
    size_t sourceLen, int level)
{
	z_stream stream;
	int err;

	stream.next_in = (Byte *)source;
	stream.avail_in = (uInt)sourceLen;
	stream.next_out = dest;
	stream.avail_out = (uInt)*destLen;

	if ((size_t)stream.avail_out != *destLen)
		return (Z_BUF_ERROR);

	stream.workspace = zlib_workspace_alloc(KM_SLEEP);
	if (!stream.workspace)
		return (Z_MEM_ERROR);

	err = zlib_deflateInit(&stream, level);
	if (err != Z_OK) {
		zlib_workspace_free(stream.workspace);
		return (err);
	}

	err = zlib_deflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		zlib_deflateEnd(&stream);
		zlib_workspace_free(stream.workspace);
		return (err == Z_OK ? Z_BUF_ERROR : err);
	}
	*destLen = stream.total_out;

	err = zlib_deflateEnd(&stream);
	zlib_workspace_free(stream.workspace);

	return (err);
}
EXPORT_SYMBOL(z_compress_level);

 
int
z_uncompress(void *dest, size_t *destLen, const void *source, size_t sourceLen)
{
	z_stream stream;
	int err;

	stream.next_in = (Byte *)source;
	stream.avail_in = (uInt)sourceLen;
	stream.next_out = dest;
	stream.avail_out = (uInt)*destLen;

	if ((size_t)stream.avail_out != *destLen)
		return (Z_BUF_ERROR);

	stream.workspace = zlib_workspace_alloc(KM_SLEEP);
	if (!stream.workspace)
		return (Z_MEM_ERROR);

	err = zlib_inflateInit(&stream);
	if (err != Z_OK) {
		zlib_workspace_free(stream.workspace);
		return (err);
	}

	err = zlib_inflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		zlib_inflateEnd(&stream);
		zlib_workspace_free(stream.workspace);

		if (err == Z_NEED_DICT ||
		    (err == Z_BUF_ERROR && stream.avail_in == 0))
			return (Z_DATA_ERROR);

		return (err);
	}
	*destLen = stream.total_out;

	err = zlib_inflateEnd(&stream);
	zlib_workspace_free(stream.workspace);

	return (err);
}
EXPORT_SYMBOL(z_uncompress);

int
spl_zlib_init(void)
{
	int size;

	size = MAX(zlib_deflate_workspacesize(MAX_WBITS, MAX_MEM_LEVEL),
	    zlib_inflate_workspacesize());

	zlib_workspace_cache = kmem_cache_create(
	    "spl_zlib_workspace_cache",
	    size, 0, NULL, NULL, NULL, NULL, NULL,
	    KMC_KVMEM);
	if (!zlib_workspace_cache)
		return (-ENOMEM);

	return (0);
}

void
spl_zlib_fini(void)
{
	kmem_cache_destroy(zlib_workspace_cache);
	zlib_workspace_cache = NULL;
}
