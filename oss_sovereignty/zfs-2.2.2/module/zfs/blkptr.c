 

 

#include <sys/blkptr.h>
#include <sys/zfs_context.h>
#include <sys/zio.h>
#include <sys/zio_compress.h>

 

void
encode_embedded_bp_compressed(blkptr_t *bp, void *data,
    enum zio_compress comp, int uncompressed_size, int compressed_size)
{
	uint64_t *bp64 = (uint64_t *)bp;
	uint64_t w = 0;
	uint8_t *data8 = data;

	ASSERT3U(compressed_size, <=, BPE_PAYLOAD_SIZE);
	ASSERT(uncompressed_size == compressed_size ||
	    comp != ZIO_COMPRESS_OFF);
	ASSERT3U(comp, >=, ZIO_COMPRESS_OFF);
	ASSERT3U(comp, <, ZIO_COMPRESS_FUNCTIONS);

	memset(bp, 0, sizeof (*bp));
	BP_SET_EMBEDDED(bp, B_TRUE);
	BP_SET_COMPRESS(bp, comp);
	BP_SET_BYTEORDER(bp, ZFS_HOST_BYTEORDER);
	BPE_SET_LSIZE(bp, uncompressed_size);
	BPE_SET_PSIZE(bp, compressed_size);

	 
	for (int i = 0; i < compressed_size; i++) {
		BF64_SET(w, (i % sizeof (w)) * NBBY, NBBY, data8[i]);
		if (i % sizeof (w) == sizeof (w) - 1) {
			 
			ASSERT3P(bp64, <, bp + 1);
			*bp64 = w;
			bp64++;
			if (!BPE_IS_PAYLOADWORD(bp, bp64))
				bp64++;
			w = 0;
		}
	}
	 
	if (bp64 < (uint64_t *)(bp + 1))
		*bp64 = w;
}

 
void
decode_embedded_bp_compressed(const blkptr_t *bp, void *buf)
{
	int psize;
	uint8_t *buf8 = buf;
	uint64_t w = 0;
	const uint64_t *bp64 = (const uint64_t *)bp;

	ASSERT(BP_IS_EMBEDDED(bp));

	psize = BPE_GET_PSIZE(bp);

	 
	for (int i = 0; i < psize; i++) {
		if (i % sizeof (w) == 0) {
			 
			ASSERT3P(bp64, <, bp + 1);
			w = *bp64;
			bp64++;
			if (!BPE_IS_PAYLOADWORD(bp, bp64))
				bp64++;
		}
		buf8[i] = BF64_GET(w, (i % sizeof (w)) * NBBY, NBBY);
	}
}

 
int
decode_embedded_bp(const blkptr_t *bp, void *buf, int buflen)
{
	int lsize, psize;

	ASSERT(BP_IS_EMBEDDED(bp));

	lsize = BPE_GET_LSIZE(bp);
	psize = BPE_GET_PSIZE(bp);

	if (lsize > buflen)
		return (SET_ERROR(ENOSPC));
	ASSERT3U(lsize, ==, buflen);

	if (BP_GET_COMPRESS(bp) != ZIO_COMPRESS_OFF) {
		uint8_t dstbuf[BPE_PAYLOAD_SIZE];
		decode_embedded_bp_compressed(bp, dstbuf);
		VERIFY0(zio_decompress_data_buf(BP_GET_COMPRESS(bp),
		    dstbuf, buf, psize, buflen, NULL));
	} else {
		ASSERT3U(lsize, ==, psize);
		decode_embedded_bp_compressed(bp, buf);
	}

	return (0);
}
