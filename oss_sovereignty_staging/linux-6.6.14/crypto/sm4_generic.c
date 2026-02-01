

 

#include <crypto/algapi.h>
#include <crypto/sm4.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>

 
static int sm4_setkey(struct crypto_tfm *tfm, const u8 *in_key,
		       unsigned int key_len)
{
	struct sm4_ctx *ctx = crypto_tfm_ctx(tfm);

	return sm4_expandkey(ctx, in_key, key_len);
}

 

static void sm4_encrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	const struct sm4_ctx *ctx = crypto_tfm_ctx(tfm);

	sm4_crypt_block(ctx->rkey_enc, out, in);
}

 

static void sm4_decrypt(struct crypto_tfm *tfm, u8 *out, const u8 *in)
{
	const struct sm4_ctx *ctx = crypto_tfm_ctx(tfm);

	sm4_crypt_block(ctx->rkey_dec, out, in);
}

static struct crypto_alg sm4_alg = {
	.cra_name		=	"sm4",
	.cra_driver_name	=	"sm4-generic",
	.cra_priority		=	100,
	.cra_flags		=	CRYPTO_ALG_TYPE_CIPHER,
	.cra_blocksize		=	SM4_BLOCK_SIZE,
	.cra_ctxsize		=	sizeof(struct sm4_ctx),
	.cra_module		=	THIS_MODULE,
	.cra_u			=	{
		.cipher = {
			.cia_min_keysize	=	SM4_KEY_SIZE,
			.cia_max_keysize	=	SM4_KEY_SIZE,
			.cia_setkey		=	sm4_setkey,
			.cia_encrypt		=	sm4_encrypt,
			.cia_decrypt		=	sm4_decrypt
		}
	}
};

static int __init sm4_init(void)
{
	return crypto_register_alg(&sm4_alg);
}

static void __exit sm4_fini(void)
{
	crypto_unregister_alg(&sm4_alg);
}

subsys_initcall(sm4_init);
module_exit(sm4_fini);

MODULE_DESCRIPTION("SM4 Cipher Algorithm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS_CRYPTO("sm4");
MODULE_ALIAS_CRYPTO("sm4-generic");
