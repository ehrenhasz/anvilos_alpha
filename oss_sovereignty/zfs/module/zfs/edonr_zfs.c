#include <sys/zfs_context.h>
#include <sys/zio.h>
#include <sys/zio_checksum.h>
#include <sys/edonr.h>
#include <sys/abd.h>
#define	EDONR_MODE		512
#define	EDONR_BLOCK_SIZE	EdonR512_BLOCK_SIZE
static int
edonr_incremental(void *buf, size_t size, void *arg)
{
	EdonRState *ctx = arg;
	EdonRUpdate(ctx, buf, size * 8);
	return (0);
}
void
abd_checksum_edonr_native(abd_t *abd, uint64_t size,
    const void *ctx_template, zio_cksum_t *zcp)
{
	uint8_t		digest[EDONR_MODE / 8];
	EdonRState	ctx;
	ASSERT(ctx_template != NULL);
	memcpy(&ctx, ctx_template, sizeof (ctx));
	(void) abd_iterate_func(abd, 0, size, edonr_incremental, &ctx);
	EdonRFinal(&ctx, digest);
	memcpy(zcp->zc_word, digest, sizeof (zcp->zc_word));
}
void
abd_checksum_edonr_byteswap(abd_t *abd, uint64_t size,
    const void *ctx_template, zio_cksum_t *zcp)
{
	zio_cksum_t	tmp;
	abd_checksum_edonr_native(abd, size, ctx_template, &tmp);
	zcp->zc_word[0] = BSWAP_64(zcp->zc_word[0]);
	zcp->zc_word[1] = BSWAP_64(zcp->zc_word[1]);
	zcp->zc_word[2] = BSWAP_64(zcp->zc_word[2]);
	zcp->zc_word[3] = BSWAP_64(zcp->zc_word[3]);
}
void *
abd_checksum_edonr_tmpl_init(const zio_cksum_salt_t *salt)
{
	EdonRState	*ctx;
	uint8_t		salt_block[EDONR_BLOCK_SIZE];
	_Static_assert(EDONR_BLOCK_SIZE == 2 * (EDONR_MODE / 8),
	    "Edon-R block size mismatch");
	EdonRHash(salt->zcs_bytes, sizeof (salt->zcs_bytes) * 8, salt_block);
	EdonRHash(salt_block, EDONR_MODE, salt_block + EDONR_MODE / 8);
	ctx = kmem_zalloc(sizeof (*ctx), KM_SLEEP);
	EdonRInit(ctx);
	EdonRUpdate(ctx, salt_block, sizeof (salt_block) * 8);
	return (ctx);
}
void
abd_checksum_edonr_tmpl_free(void *ctx_template)
{
	EdonRState *ctx = ctx_template;
	memset(ctx, 0, sizeof (*ctx));
	kmem_free(ctx, sizeof (*ctx));
}
