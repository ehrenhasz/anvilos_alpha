#include <sys/param.h>
#include <sys/byteorder.h>
#include <sys/spa_checksum.h>
#include <sys/string.h>
#include <zfs_fletcher.h>
static void
fletcher_4_superscalar4_init(fletcher_4_ctx_t *ctx)
{
	memset(ctx->superscalar, 0, 4 * sizeof (zfs_fletcher_superscalar_t));
}
static void
fletcher_4_superscalar4_fini(fletcher_4_ctx_t *ctx, zio_cksum_t *zcp)
{
	uint64_t A, B, C, D;
	A = ctx->superscalar[0].v[0] + ctx->superscalar[0].v[1] +
	    ctx->superscalar[0].v[2] + ctx->superscalar[0].v[3];
	B = 0 - ctx->superscalar[0].v[1] - 2 * ctx->superscalar[0].v[2] -
	    3 * ctx->superscalar[0].v[3] + 4 * ctx->superscalar[1].v[0] +
	    4 * ctx->superscalar[1].v[1] + 4 * ctx->superscalar[1].v[2] +
	    4 * ctx->superscalar[1].v[3];
	C = ctx->superscalar[0].v[2] + 3 * ctx->superscalar[0].v[3] -
	    6 * ctx->superscalar[1].v[0] - 10 * ctx->superscalar[1].v[1] -
	    14 * ctx->superscalar[1].v[2] - 18 * ctx->superscalar[1].v[3] +
	    16 * ctx->superscalar[2].v[0] + 16 * ctx->superscalar[2].v[1] +
	    16 * ctx->superscalar[2].v[2] + 16 * ctx->superscalar[2].v[3];
	D = 0 - ctx->superscalar[0].v[3] + 4 * ctx->superscalar[1].v[0] +
	    10 * ctx->superscalar[1].v[1] + 20 * ctx->superscalar[1].v[2] +
	    34 * ctx->superscalar[1].v[3] - 48 * ctx->superscalar[2].v[0] -
	    64 * ctx->superscalar[2].v[1] - 80 * ctx->superscalar[2].v[2] -
	    96 * ctx->superscalar[2].v[3] + 64 * ctx->superscalar[3].v[0] +
	    64 * ctx->superscalar[3].v[1] + 64 * ctx->superscalar[3].v[2] +
	    64 * ctx->superscalar[3].v[3];
	ZIO_SET_CHECKSUM(zcp, A, B, C, D);
}
static void
fletcher_4_superscalar4_native(fletcher_4_ctx_t *ctx,
    const void *buf, uint64_t size)
{
	const uint32_t *ip = buf;
	const uint32_t *ipend = ip + (size / sizeof (uint32_t));
	uint64_t a, b, c, d;
	uint64_t a2, b2, c2, d2;
	uint64_t a3, b3, c3, d3;
	uint64_t a4, b4, c4, d4;
	a = ctx->superscalar[0].v[0];
	b = ctx->superscalar[1].v[0];
	c = ctx->superscalar[2].v[0];
	d = ctx->superscalar[3].v[0];
	a2 = ctx->superscalar[0].v[1];
	b2 = ctx->superscalar[1].v[1];
	c2 = ctx->superscalar[2].v[1];
	d2 = ctx->superscalar[3].v[1];
	a3 = ctx->superscalar[0].v[2];
	b3 = ctx->superscalar[1].v[2];
	c3 = ctx->superscalar[2].v[2];
	d3 = ctx->superscalar[3].v[2];
	a4 = ctx->superscalar[0].v[3];
	b4 = ctx->superscalar[1].v[3];
	c4 = ctx->superscalar[2].v[3];
	d4 = ctx->superscalar[3].v[3];
	do {
		a += ip[0];
		a2 += ip[1];
		a3 += ip[2];
		a4 += ip[3];
		b += a;
		b2 += a2;
		b3 += a3;
		b4 += a4;
		c += b;
		c2 += b2;
		c3 += b3;
		c4 += b4;
		d += c;
		d2 += c2;
		d3 += c3;
		d4 += c4;
	} while ((ip += 4) < ipend);
	ctx->superscalar[0].v[0] = a;
	ctx->superscalar[1].v[0] = b;
	ctx->superscalar[2].v[0] = c;
	ctx->superscalar[3].v[0] = d;
	ctx->superscalar[0].v[1] = a2;
	ctx->superscalar[1].v[1] = b2;
	ctx->superscalar[2].v[1] = c2;
	ctx->superscalar[3].v[1] = d2;
	ctx->superscalar[0].v[2] = a3;
	ctx->superscalar[1].v[2] = b3;
	ctx->superscalar[2].v[2] = c3;
	ctx->superscalar[3].v[2] = d3;
	ctx->superscalar[0].v[3] = a4;
	ctx->superscalar[1].v[3] = b4;
	ctx->superscalar[2].v[3] = c4;
	ctx->superscalar[3].v[3] = d4;
}
static void
fletcher_4_superscalar4_byteswap(fletcher_4_ctx_t *ctx,
    const void *buf, uint64_t size)
{
	const uint32_t *ip = buf;
	const uint32_t *ipend = ip + (size / sizeof (uint32_t));
	uint64_t a, b, c, d;
	uint64_t a2, b2, c2, d2;
	uint64_t a3, b3, c3, d3;
	uint64_t a4, b4, c4, d4;
	a = ctx->superscalar[0].v[0];
	b = ctx->superscalar[1].v[0];
	c = ctx->superscalar[2].v[0];
	d = ctx->superscalar[3].v[0];
	a2 = ctx->superscalar[0].v[1];
	b2 = ctx->superscalar[1].v[1];
	c2 = ctx->superscalar[2].v[1];
	d2 = ctx->superscalar[3].v[1];
	a3 = ctx->superscalar[0].v[2];
	b3 = ctx->superscalar[1].v[2];
	c3 = ctx->superscalar[2].v[2];
	d3 = ctx->superscalar[3].v[2];
	a4 = ctx->superscalar[0].v[3];
	b4 = ctx->superscalar[1].v[3];
	c4 = ctx->superscalar[2].v[3];
	d4 = ctx->superscalar[3].v[3];
	do {
		a += BSWAP_32(ip[0]);
		a2 += BSWAP_32(ip[1]);
		a3 += BSWAP_32(ip[2]);
		a4 += BSWAP_32(ip[3]);
		b += a;
		b2 += a2;
		b3 += a3;
		b4 += a4;
		c += b;
		c2 += b2;
		c3 += b3;
		c4 += b4;
		d += c;
		d2 += c2;
		d3 += c3;
		d4 += c4;
	} while ((ip += 4) < ipend);
	ctx->superscalar[0].v[0] = a;
	ctx->superscalar[1].v[0] = b;
	ctx->superscalar[2].v[0] = c;
	ctx->superscalar[3].v[0] = d;
	ctx->superscalar[0].v[1] = a2;
	ctx->superscalar[1].v[1] = b2;
	ctx->superscalar[2].v[1] = c2;
	ctx->superscalar[3].v[1] = d2;
	ctx->superscalar[0].v[2] = a3;
	ctx->superscalar[1].v[2] = b3;
	ctx->superscalar[2].v[2] = c3;
	ctx->superscalar[3].v[2] = d3;
	ctx->superscalar[0].v[3] = a4;
	ctx->superscalar[1].v[3] = b4;
	ctx->superscalar[2].v[3] = c4;
	ctx->superscalar[3].v[3] = d4;
}
static boolean_t fletcher_4_superscalar4_valid(void)
{
	return (B_TRUE);
}
const fletcher_4_ops_t fletcher_4_superscalar4_ops = {
	.init_native = fletcher_4_superscalar4_init,
	.compute_native = fletcher_4_superscalar4_native,
	.fini_native = fletcher_4_superscalar4_fini,
	.init_byteswap = fletcher_4_superscalar4_init,
	.compute_byteswap = fletcher_4_superscalar4_byteswap,
	.fini_byteswap = fletcher_4_superscalar4_fini,
	.valid = fletcher_4_superscalar4_valid,
	.uses_fpu = B_FALSE,
	.name = "superscalar4"
};
