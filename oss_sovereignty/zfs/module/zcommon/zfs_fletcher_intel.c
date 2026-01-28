#if defined(HAVE_AVX) && defined(HAVE_AVX2)
#include <sys/spa_checksum.h>
#include <sys/string.h>
#include <sys/simd.h>
#include <zfs_fletcher.h>
static void
fletcher_4_avx2_init(fletcher_4_ctx_t *ctx)
{
	memset(ctx->avx, 0, 4 * sizeof (zfs_fletcher_avx_t));
}
static void
fletcher_4_avx2_fini(fletcher_4_ctx_t *ctx, zio_cksum_t *zcp)
{
	uint64_t A, B, C, D;
	A = ctx->avx[0].v[0] + ctx->avx[0].v[1] +
	    ctx->avx[0].v[2] + ctx->avx[0].v[3];
	B = 0 - ctx->avx[0].v[1] - 2 * ctx->avx[0].v[2] - 3 * ctx->avx[0].v[3] +
	    4 * ctx->avx[1].v[0] + 4 * ctx->avx[1].v[1] + 4 * ctx->avx[1].v[2] +
	    4 * ctx->avx[1].v[3];
	C = ctx->avx[0].v[2] + 3 * ctx->avx[0].v[3] - 6 * ctx->avx[1].v[0] -
	    10 * ctx->avx[1].v[1] - 14 * ctx->avx[1].v[2] -
	    18 * ctx->avx[1].v[3] + 16 * ctx->avx[2].v[0] +
	    16 * ctx->avx[2].v[1] + 16 * ctx->avx[2].v[2] +
	    16 * ctx->avx[2].v[3];
	D = 0 - ctx->avx[0].v[3] + 4 * ctx->avx[1].v[0] +
	    10 * ctx->avx[1].v[1] + 20 * ctx->avx[1].v[2] +
	    34 * ctx->avx[1].v[3] - 48 * ctx->avx[2].v[0] -
	    64 * ctx->avx[2].v[1] - 80 * ctx->avx[2].v[2] -
	    96 * ctx->avx[2].v[3] + 64 * ctx->avx[3].v[0] +
	    64 * ctx->avx[3].v[1] + 64 * ctx->avx[3].v[2] +
	    64 * ctx->avx[3].v[3];
	ZIO_SET_CHECKSUM(zcp, A, B, C, D);
}
#define	FLETCHER_4_AVX2_RESTORE_CTX(ctx)				\
{									\
	asm volatile("vmovdqu %0, %%ymm0" :: "m" ((ctx)->avx[0]));	\
	asm volatile("vmovdqu %0, %%ymm1" :: "m" ((ctx)->avx[1]));	\
	asm volatile("vmovdqu %0, %%ymm2" :: "m" ((ctx)->avx[2]));	\
	asm volatile("vmovdqu %0, %%ymm3" :: "m" ((ctx)->avx[3]));	\
}
#define	FLETCHER_4_AVX2_SAVE_CTX(ctx)					\
{									\
	asm volatile("vmovdqu %%ymm0, %0" : "=m" ((ctx)->avx[0]));	\
	asm volatile("vmovdqu %%ymm1, %0" : "=m" ((ctx)->avx[1]));	\
	asm volatile("vmovdqu %%ymm2, %0" : "=m" ((ctx)->avx[2]));	\
	asm volatile("vmovdqu %%ymm3, %0" : "=m" ((ctx)->avx[3]));	\
}
static void
fletcher_4_avx2_native(fletcher_4_ctx_t *ctx, const void *buf, uint64_t size)
{
	const uint64_t *ip = buf;
	const uint64_t *ipend = (uint64_t *)((uint8_t *)ip + size);
	FLETCHER_4_AVX2_RESTORE_CTX(ctx);
	do {
		asm volatile("vpmovzxdq %0, %%ymm4"::"m" (*ip));
		asm volatile("vpaddq %ymm4, %ymm0, %ymm0");
		asm volatile("vpaddq %ymm0, %ymm1, %ymm1");
		asm volatile("vpaddq %ymm1, %ymm2, %ymm2");
		asm volatile("vpaddq %ymm2, %ymm3, %ymm3");
	} while ((ip += 2) < ipend);
	FLETCHER_4_AVX2_SAVE_CTX(ctx);
	asm volatile("vzeroupper");
}
static void
fletcher_4_avx2_byteswap(fletcher_4_ctx_t *ctx, const void *buf, uint64_t size)
{
	static const zfs_fletcher_avx_t mask = {
		.v = { 0xFFFFFFFF00010203, 0xFFFFFFFF08090A0B,
		    0xFFFFFFFF00010203, 0xFFFFFFFF08090A0B }
	};
	const uint64_t *ip = buf;
	const uint64_t *ipend = (uint64_t *)((uint8_t *)ip + size);
	FLETCHER_4_AVX2_RESTORE_CTX(ctx);
	asm volatile("vmovdqu %0, %%ymm5" :: "m" (mask));
	do {
		asm volatile("vpmovzxdq %0, %%ymm4"::"m" (*ip));
		asm volatile("vpshufb %ymm5, %ymm4, %ymm4");
		asm volatile("vpaddq %ymm4, %ymm0, %ymm0");
		asm volatile("vpaddq %ymm0, %ymm1, %ymm1");
		asm volatile("vpaddq %ymm1, %ymm2, %ymm2");
		asm volatile("vpaddq %ymm2, %ymm3, %ymm3");
	} while ((ip += 2) < ipend);
	FLETCHER_4_AVX2_SAVE_CTX(ctx);
	asm volatile("vzeroupper");
}
static boolean_t fletcher_4_avx2_valid(void)
{
	return (kfpu_allowed() && zfs_avx_available() && zfs_avx2_available());
}
const fletcher_4_ops_t fletcher_4_avx2_ops = {
	.init_native = fletcher_4_avx2_init,
	.fini_native = fletcher_4_avx2_fini,
	.compute_native = fletcher_4_avx2_native,
	.init_byteswap = fletcher_4_avx2_init,
	.fini_byteswap = fletcher_4_avx2_fini,
	.compute_byteswap = fletcher_4_avx2_byteswap,
	.valid = fletcher_4_avx2_valid,
	.uses_fpu = B_TRUE,
	.name = "avx2"
};
#endif  
