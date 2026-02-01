 

#if defined(HAVE_SSE2)

#include <sys/simd.h>
#include <sys/spa_checksum.h>
#include <sys/string.h>
#include <sys/byteorder.h>
#include <zfs_fletcher.h>

static void
fletcher_4_sse2_init(fletcher_4_ctx_t *ctx)
{
	memset(ctx->sse, 0, 4 * sizeof (zfs_fletcher_sse_t));
}

static void
fletcher_4_sse2_fini(fletcher_4_ctx_t *ctx, zio_cksum_t *zcp)
{
	uint64_t A, B, C, D;

	 
	A = ctx->sse[0].v[0] + ctx->sse[0].v[1];
	B = 2 * ctx->sse[1].v[0] + 2 * ctx->sse[1].v[1] - ctx->sse[0].v[1];
	C = 4 * ctx->sse[2].v[0] - ctx->sse[1].v[0] + 4 * ctx->sse[2].v[1] -
	    3 * ctx->sse[1].v[1];
	D = 8 * ctx->sse[3].v[0] - 4 * ctx->sse[2].v[0] + 8 * ctx->sse[3].v[1] -
	    8 * ctx->sse[2].v[1] + ctx->sse[1].v[1];

	ZIO_SET_CHECKSUM(zcp, A, B, C, D);
}

#define	FLETCHER_4_SSE_RESTORE_CTX(ctx)					\
{									\
	asm volatile("movdqu %0, %%xmm0" :: "m" ((ctx)->sse[0]));	\
	asm volatile("movdqu %0, %%xmm1" :: "m" ((ctx)->sse[1]));	\
	asm volatile("movdqu %0, %%xmm2" :: "m" ((ctx)->sse[2]));	\
	asm volatile("movdqu %0, %%xmm3" :: "m" ((ctx)->sse[3]));	\
}

#define	FLETCHER_4_SSE_SAVE_CTX(ctx)					\
{									\
	asm volatile("movdqu %%xmm0, %0" : "=m" ((ctx)->sse[0]));	\
	asm volatile("movdqu %%xmm1, %0" : "=m" ((ctx)->sse[1]));	\
	asm volatile("movdqu %%xmm2, %0" : "=m" ((ctx)->sse[2]));	\
	asm volatile("movdqu %%xmm3, %0" : "=m" ((ctx)->sse[3]));	\
}

static void
fletcher_4_sse2_native(fletcher_4_ctx_t *ctx, const void *buf, uint64_t size)
{
	const uint64_t *ip = buf;
	const uint64_t *ipend = (uint64_t *)((uint8_t *)ip + size);

	FLETCHER_4_SSE_RESTORE_CTX(ctx);

	asm volatile("pxor %xmm4, %xmm4");

	do {
		asm volatile("movdqu %0, %%xmm5" :: "m"(*ip));
		asm volatile("movdqa %xmm5, %xmm6");
		asm volatile("punpckldq %xmm4, %xmm5");
		asm volatile("punpckhdq %xmm4, %xmm6");
		asm volatile("paddq %xmm5, %xmm0");
		asm volatile("paddq %xmm0, %xmm1");
		asm volatile("paddq %xmm1, %xmm2");
		asm volatile("paddq %xmm2, %xmm3");
		asm volatile("paddq %xmm6, %xmm0");
		asm volatile("paddq %xmm0, %xmm1");
		asm volatile("paddq %xmm1, %xmm2");
		asm volatile("paddq %xmm2, %xmm3");
	} while ((ip += 2) < ipend);

	FLETCHER_4_SSE_SAVE_CTX(ctx);
}

static void
fletcher_4_sse2_byteswap(fletcher_4_ctx_t *ctx, const void *buf, uint64_t size)
{
	const uint32_t *ip = buf;
	const uint32_t *ipend = (uint32_t *)((uint8_t *)ip + size);

	FLETCHER_4_SSE_RESTORE_CTX(ctx);

	do {
		uint32_t scratch1 = BSWAP_32(ip[0]);
		uint32_t scratch2 = BSWAP_32(ip[1]);
		asm volatile("movd %0, %%xmm5" :: "r"(scratch1));
		asm volatile("movd %0, %%xmm6" :: "r"(scratch2));
		asm volatile("punpcklqdq %xmm6, %xmm5");
		asm volatile("paddq %xmm5, %xmm0");
		asm volatile("paddq %xmm0, %xmm1");
		asm volatile("paddq %xmm1, %xmm2");
		asm volatile("paddq %xmm2, %xmm3");
	} while ((ip += 2) < ipend);

	FLETCHER_4_SSE_SAVE_CTX(ctx);
}

static boolean_t fletcher_4_sse2_valid(void)
{
	return (kfpu_allowed() && zfs_sse2_available());
}

const fletcher_4_ops_t fletcher_4_sse2_ops = {
	.init_native = fletcher_4_sse2_init,
	.fini_native = fletcher_4_sse2_fini,
	.compute_native = fletcher_4_sse2_native,
	.init_byteswap = fletcher_4_sse2_init,
	.fini_byteswap = fletcher_4_sse2_fini,
	.compute_byteswap = fletcher_4_sse2_byteswap,
	.valid = fletcher_4_sse2_valid,
	.uses_fpu = B_TRUE,
	.name = "sse2"
};

#endif  

#if defined(HAVE_SSE2) && defined(HAVE_SSSE3)
static void
fletcher_4_ssse3_byteswap(fletcher_4_ctx_t *ctx, const void *buf, uint64_t size)
{
	static const zfs_fletcher_sse_t mask = {
		.v = { 0x0405060700010203, 0x0C0D0E0F08090A0B }
	};

	const uint64_t *ip = buf;
	const uint64_t *ipend = (uint64_t *)((uint8_t *)ip + size);

	FLETCHER_4_SSE_RESTORE_CTX(ctx);

	asm volatile("movdqu %0, %%xmm7"::"m" (mask));
	asm volatile("pxor %xmm4, %xmm4");

	do {
		asm volatile("movdqu %0, %%xmm5"::"m" (*ip));
		asm volatile("pshufb %xmm7, %xmm5");
		asm volatile("movdqa %xmm5, %xmm6");
		asm volatile("punpckldq %xmm4, %xmm5");
		asm volatile("punpckhdq %xmm4, %xmm6");
		asm volatile("paddq %xmm5, %xmm0");
		asm volatile("paddq %xmm0, %xmm1");
		asm volatile("paddq %xmm1, %xmm2");
		asm volatile("paddq %xmm2, %xmm3");
		asm volatile("paddq %xmm6, %xmm0");
		asm volatile("paddq %xmm0, %xmm1");
		asm volatile("paddq %xmm1, %xmm2");
		asm volatile("paddq %xmm2, %xmm3");
	} while ((ip += 2) < ipend);

	FLETCHER_4_SSE_SAVE_CTX(ctx);
}

static boolean_t fletcher_4_ssse3_valid(void)
{
	return (kfpu_allowed() && zfs_sse2_available() &&
	    zfs_ssse3_available());
}

const fletcher_4_ops_t fletcher_4_ssse3_ops = {
	.init_native = fletcher_4_sse2_init,
	.fini_native = fletcher_4_sse2_fini,
	.compute_native = fletcher_4_sse2_native,
	.init_byteswap = fletcher_4_sse2_init,
	.fini_byteswap = fletcher_4_sse2_fini,
	.compute_byteswap = fletcher_4_ssse3_byteswap,
	.valid = fletcher_4_ssse3_valid,
	.uses_fpu = B_TRUE,
	.name = "ssse3"
};

#endif  
